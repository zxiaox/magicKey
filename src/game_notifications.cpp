#include "game_notifications.hpp"
#include "profile_hotkeys.hpp"
#include <filesystem>
#include <vector>
namespace mywarkey {
namespace {
DWORD warcraftProcess(HWND window) {
    DWORD pid = 0;
    if (!window || GetForegroundWindow() != window) return 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 0;
    wchar_t path[32768]; DWORD length = DWORD(std::size(path));
    bool valid = QueryFullProcessImageNameW(process, 0, path, &length) &&
        warcraftExecutable(std::filesystem::path(std::wstring(path, length)).filename().wstring());
    CloseHandle(process);
    return valid ? pid : 0;
}
}
bool GameNotifications::sameTarget() const {
    DWORD pid = 0;
    if (!target || GetForegroundWindow() != target || !IsWindow(target)) return false;
    GetWindowThreadProcessId(target, &pid);
    return pid == process;
}
bool GameNotifications::idle() const {
    // Wait for physical modifiers and ordinary keys. Never synthesize their
    // releases: a user-held Alt must not turn Enter into Alt+Enter.
    for (int key = 1; key < 255; ++key) if (GetAsyncKeyState(key) & 0x8000) return false;
    return true;
}
bool GameNotifications::key(WORD scan) {
    INPUT input[2]{};
    for (auto& i : input) { i.type = INPUT_KEYBOARD; i.ki.wScan = scan; i.ki.dwFlags = KEYEVENTF_SCANCODE; }
    input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    UINT sent = SendInput(2, input, sizeof(INPUT));
    if (sent == 1) SendInput(1, &input[1], sizeof(INPUT)); // Pair partial down.
    return sent == 2;
}
void GameNotifications::queue(HWND game, std::wstring message) {
    DWORD pid = warcraftProcess(game);
    if (!pid) return;
    if (typing()) cancel();
    target = game; process = pid; pending = std::move(message);
    deadline = GetTickCount64() + 2500;
}
void GameNotifications::cancel() {
    // If we opened a chat box and the game is still foreground, close it.
    if (stage >= 2 && sameTarget() && idle()) key(0x01); // Escape.
    stage = 0; pending.clear(); current.clear(); target = nullptr; process = 0;
}
void GameNotifications::tick() {
    if (!target) return;
    if (!sameTarget()) { cancel(); return; }
    auto now = GetTickCount64();
    if (now > deadline) { cancel(); return; }
    if (!idle()) { if (typing()) cancel(); return; }
    if (stage == 0) {
        if (pending.empty()) return;
        current = std::move(pending); pending.clear();
        // Reset an existing prompt first, preventing Enter from sending an
        // unrelated draft. This can discard a draft; documented in the UI help.
        if (!key(0x01)) { cancel(); return; }
        stage = 1; next = now + 100;
    } else if (now >= next && stage == 1) {
        if (!key(0x1C)) { cancel(); return; } // Enter: open default chat channel.
        stage = 2; next = now + 150;
    } else if (now >= next && stage == 2) {
        std::vector<INPUT> input;
        for (wchar_t c : current) {
            INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wScan = c; i.ki.dwFlags = KEYEVENTF_UNICODE;
            input.push_back(i); i.ki.dwFlags |= KEYEVENTF_KEYUP; input.push_back(i);
        }
        // The whole text is inserted atomically, bypassing our remapper.
        UINT sent = SendInput(UINT(input.size()), input.data(), sizeof(INPUT));
        if (sent != input.size()) {
            if ((sent & 1) && sent < input.size()) SendInput(1, &input[sent], sizeof(INPUT));
            cancel(); return;
        }
        stage = 3; next = now + 100;
    } else if (now >= next && stage == 3) {
        if (!key(0x1C)) { cancel(); return; } // Submit only to the same window.
        stage = 0; current.clear(); target = nullptr; process = 0;
    }
}
}
