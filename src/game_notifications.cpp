#include "game_notifications.hpp"
#include "profile_hotkeys.hpp"
#include <filesystem>
#include <vector>
namespace mywarkey {
namespace {
DWORD warcraftProcess(HWND window, std::wstring& name, bool& classic) {
    DWORD pid = 0;
    if (!window || GetForegroundWindow() != window) { name = L"No matching foreground window"; return 0; }
    GetWindowThreadProcessId(window, &pid);
    if (!pid) { name = L"Foreground window has no process ID"; return 0; }
    wchar_t className[256]{}, title[512]{};
    GetClassNameW(window, className, int(std::size(className)));
    GetWindowTextW(window, title, int(std::size(title)));
    std::wstring executable;
    DWORD queryError = ERROR_SUCCESS;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        wchar_t path[32768]; DWORD length = DWORD(std::size(path));
        if (QueryFullProcessImageNameW(process, 0, path, &length))
            executable = std::filesystem::path(std::wstring(path, length)).filename().wstring();
        else queryError = GetLastError();
        CloseHandle(process);
    } else queryError = GetLastError();
    const bool byProcess = warcraftExecutable(executable);
    const bool byClass = warcraftWindowClass(className);
    std::wstring lower = executable;
    for (auto& c : lower) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    classic = lower == L"war3.exe" || (byClass && !byProcess);
    // A window title alone (or a generic Qt/OsWindow class) is not enough.
    // The class fallback needs no handle to the protected game process.
    name = L"PID " + std::to_wstring(pid) + L"; class: " + className + L"; title: " + title;
    if (!executable.empty()) name += L"; executable: " + executable;
    if (queryError) name += L"; process query error: " + std::to_wstring(queryError);
    if (byClass && !byProcess) name += L"; recognized by classic Warcraft window class";
    return byProcess || byClass ? pid : 0;
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
bool GameNotifications::paste(bool down) {
    INPUT input[]{scanInput(down ? 0x1D : 0x2F, down), scanInput(down ? 0x2F : 0x1D, down)};
    UINT sent = SendInput(2, input, sizeof(INPUT));
    // Keep cleanup armed if a Ctrl/V release was only partially accepted.
    if (down) pasteHeld = sent != 0;
    else pasteHeld = sent != 2;
    return sent == 2;
}
bool GameNotifications::text() {
    // Paste complete messages in every language. A scan-code batch can lose
    // letters in a game's polling loop and is affected by Caps Lock/IME state.
    if (!clipboard.put(current, classicUtf8)) { clipboardFailed = true; return false; }
    return paste(true);
}

void GameNotifications::queue(HWND game, std::wstring message) {
    lastMessage = message;
    std::wstring name;
    DWORD pid = warcraftProcess(game, name, classicUtf8);
    targetDescription = name;
    if (!pid) { report = L"Not sent: Warcraft was not recognized as the foreground game.\nForeground: " + name; return; }
    if (target) cancel();
    target = game; process = pid; pending = std::move(message);
    deadline = GetTickCount64() + 2500;
    report = L"Detected " + name + L". Waiting for shortcut keys to be released.";
}
void GameNotifications::reset() {
    // Always pair an injected Enter, including after focus loss/cancellation.
    if (enterHeld) enter(false);
    if (pasteHeld) paste(false);
    clipboard.restore();
    enterHeld = false; promptOpened = false;
    stage = 0; pending.clear(); current.clear(); target = nullptr; process = 0;
}
void GameNotifications::cancel() {
    if (pasteHeld) paste(false);
    if (enterHeld) enter(false);
    if (promptOpened && sameTarget() && idle()) {
        INPUT escape[]{scanInput(0x01, true), scanInput(0x01, false)};
        SendInput(2, escape, sizeof(INPUT));
    }
    report = L"Notification interrupted before completion.";
    reset();
}
void GameNotifications::tick() {
    if (!target) { clipboard.restore(); return; }
    if (!sameTarget()) { cancel(); report = L"Not sent: foreground window changed."; return; }
    auto now = GetTickCount64();
    if (now > deadline) { cancel(); report = L"Not sent: shortcut release/input sequence timed out."; return; }
    // Our own held Enter is intentionally down during active stages. Physical
    // user input is detected by MagicKey's hook and cancels the sequence.
    bool failed = false;
    if (stage == 0) {
        if (pending.empty() || !idle()) return;
        current = std::move(pending); pending.clear(); clipboardFailed = false;
        failed = !enter(true); // Enter -> text -> Enter. No opening Escape.
        stage = 1; next = now + 80; deadline = now + 5000;
    } else if (now >= next && stage == 1) {
        failed = !enter(false); promptOpened = true;
        stage = 2; next = now + 50;
    } else if (now >= next && stage == 2) {
        failed = !text(); stage = pasteHeld ? 5 : 3; next = now + 50;
    } else if (now >= next && stage == 5) {
        failed = !paste(false); stage = 3; next = now + 50;
    } else if (now >= next && stage == 3) {
        if (!clipboard.ownsCurrent()) {
            cancel(); report = L"Not sent: clipboard changed during the paste sequence."; return;
        }
        failed = !enter(true); stage = 4; next = now + 80;
    } else if (now >= next && stage == 4) {
        failed = !enter(false);
        if (!failed) {
            report = L"Enter -> message -> Enter delivered to Windows.\nThe game does not expose confirmation that chat was accepted.";
            report += classicUtf8 ? L"\nTransport: classic CF_TEXT / UTF-8 paste." : L"\nTransport: Unicode clipboard paste.";
            reset(); return;
        }
    }
    if (failed) { bool failedClipboard = clipboardFailed; cancel(); if (failedClipboard) { report = L"Not sent: the clipboard could not be safely preserved or opened. Try again after closing clipboard tools."; return; } report = L"Windows blocked or incompletely accepted the input. Run MagicKey at the same administrator privilege level as Warcraft."; }
}
}
