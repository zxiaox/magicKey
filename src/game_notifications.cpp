#include "game_notifications.hpp"
#include "profile_hotkeys.hpp"
#include <filesystem>
#include <vector>
namespace mywarkey {
namespace {
DWORD warcraftProcess(HWND window, std::wstring& name) {
    DWORD pid = 0;
    if (!window || GetForegroundWindow() != window) { name = L"No matching foreground window"; return 0; }
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) { name = L"Cannot inspect foreground process (check administrator privileges)"; return 0; }
    wchar_t path[32768]; DWORD length = DWORD(std::size(path));
    bool valid = QueryFullProcessImageNameW(process, 0, path, &length);
    name = valid ? std::filesystem::path(std::wstring(path, length)).filename().wstring() : L"Unknown process";
    CloseHandle(process);
    return valid && warcraftExecutable(name) ? pid : 0;
}
INPUT scanInput(WORD scan, bool down) {
    INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wScan = scan & 0xFF;
    i.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) | ((scan & 0xFF00) == 0xE000 ? KEYEVENTF_EXTENDEDKEY : 0);
    return i;
}
}
bool GameNotifications::sameTarget() const {
    DWORD pid = 0;
    if (!target || GetForegroundWindow() != target || !IsWindow(target)) return false;
    GetWindowThreadProcessId(target, &pid);
    return pid == process;
}
bool GameNotifications::idle() const {
    for (int key = 1; key < 255; ++key) if (GetAsyncKeyState(key) & 0x8000) return false;
    return true;
}
bool GameNotifications::enter(bool down) {
    auto input = scanInput(0x1C, down);
    if (SendInput(1, &input, sizeof(INPUT)) != 1) return false;
    enterHeld = down;
    return true;
}
bool GameNotifications::text() {
    DWORD thread = GetWindowThreadProcessId(target, nullptr);
    HKL layout = GetKeyboardLayout(thread);
    std::vector<INPUT> input;
    for (wchar_t c : current) {
        SHORT mapped = c < 128 ? VkKeyScanExW(c, layout) : SHORT(-1);
        // Use real keyboard scan codes for ordinary chat text. Unicode remains
        // a fallback for non-Latin profile names; never use Ctrl/Alt text chords.
        if (mapped != -1 && ((mapped >> 8) & ~1) == 0) {
            bool shift = (mapped >> 8) & 1;
            WORD scan = WORD(MapVirtualKeyExW(mapped & 0xFF, MAPVK_VK_TO_VSC_EX, layout));
            if (shift) input.push_back(scanInput(0x2A, true));
            input.push_back(scanInput(scan, true)); input.push_back(scanInput(scan, false));
            if (shift) input.push_back(scanInput(0x2A, false));
        } else {
            INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wScan = c; i.ki.dwFlags = KEYEVENTF_UNICODE;
            input.push_back(i); i.ki.dwFlags |= KEYEVENTF_KEYUP; input.push_back(i);
        }
    }
    UINT sent = SendInput(UINT(input.size()), input.data(), sizeof(INPUT));
    if (sent == input.size()) return true;
    // Release every injected key whose down was accepted before a partial send.
    std::vector<INPUT> releases;
    for (UINT n = 0; n < sent; ++n) if (!(input[n].ki.dwFlags & KEYEVENTF_KEYUP)) {
        auto up = input[n]; up.ki.dwFlags |= KEYEVENTF_KEYUP; releases.push_back(up);
    }
    if (!releases.empty()) SendInput(UINT(releases.size()), releases.data(), sizeof(INPUT));
    return false;
}
void GameNotifications::queue(HWND game, std::wstring message) {
    std::wstring name;
    DWORD pid = warcraftProcess(game, name);
    if (!pid) { report = L"Not sent: Warcraft was not recognized as the foreground game.\nForeground: " + name; return; }
    if (target) cancel();
    target = game; process = pid; pending = std::move(message);
    deadline = GetTickCount64() + 2500;
    report = L"Detected " + name + L". Waiting for shortcut keys to be released.";
}
void GameNotifications::reset() {
    // Always pair an injected Enter, including after focus loss/cancellation.
    if (enterHeld) enter(false);
    enterHeld = false; promptOpened = false;
    stage = 0; pending.clear(); current.clear(); target = nullptr; process = 0;
}
void GameNotifications::cancel() {
    if (enterHeld) enter(false);
    if (promptOpened && sameTarget() && idle()) {
        INPUT escape[]{scanInput(0x01, true), scanInput(0x01, false)};
        SendInput(2, escape, sizeof(INPUT));
    }
    report = L"Notification interrupted before completion.";
    reset();
}
void GameNotifications::tick() {
    if (!target) return;
    if (!sameTarget()) { cancel(); report = L"Not sent: foreground window changed."; return; }
    auto now = GetTickCount64();
    if (now > deadline) { cancel(); report = L"Not sent: shortcut release/input sequence timed out."; return; }
    // Our own held Enter is intentionally down during active stages. Physical
    // user input is detected by MagicKey's hook and cancels the sequence.
    bool failed = false;
    if (stage == 0) {
        if (pending.empty() || !idle()) return;
        current = std::move(pending); pending.clear();
        failed = !enter(true); // Enter -> text -> Enter. No opening Escape.
        stage = 1; next = now + 80; deadline = now + 5000;
    } else if (now >= next && stage == 1) {
        failed = !enter(false); promptOpened = true;
        stage = 2; next = now + 300;
    } else if (now >= next && stage == 2) {
        failed = !text(); stage = 3; next = now + 250;
    } else if (now >= next && stage == 3) {
        failed = !enter(true); stage = 4; next = now + 80;
    } else if (now >= next && stage == 4) {
        failed = !enter(false);
        if (!failed) {
            report = L"Enter -> message -> Enter delivered to Windows.\nThe game does not expose confirmation that chat was accepted.";
            reset(); return;
        }
    }
    if (failed) { cancel(); report = L"Windows blocked or incompletely accepted the input. Run MagicKey at the same administrator privilege level as Warcraft."; }
}
}
