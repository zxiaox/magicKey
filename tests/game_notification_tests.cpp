#include "../src/game_notifications.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace mywarkey;
bool chat = false;
std::wstring draft;
std::vector<std::wstring> submitted;
LRESULT CALLBACK receiver(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_KEYDOWN && w == VK_ESCAPE) { chat = false; draft.clear(); return 0; }
    if ((message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && w == VK_MENU) return 0;
    if (message == WM_CHAR) {
        if (w == 13) {
            if (chat) { submitted.push_back(draft); draft.clear(); }
            chat = !chat;
        } else if (chat && w >= 32) draft += wchar_t(w);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void pump(GameNotifications& notifications, int milliseconds) {
    auto until = GetTickCount64() + milliseconds;
    do {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        notifications.tick();
        Sleep(5);
    } while (GetTickCount64() < until);
}
void alt(bool down) {
    INPUT input{}; input.type = INPUT_KEYBOARD; input.ki.wVk = VK_LMENU;
    if (!down) input.ki.dwFlags = KEYEVENTF_KEYUP;
    require(SendInput(1, &input, sizeof(input)) == 1, "Cannot send test modifier");
}
int main() {
    try {
        WNDCLASSW wc{}; wc.lpfnWndProc = receiver; wc.lpszClassName = L"MagicKey.Notification.Test"; wc.hInstance = GetModuleHandleW(nullptr);
        RegisterClassW(&wc);
        HWND game = CreateWindowW(wc.lpszClassName, L"Notification input receiver", WS_OVERLAPPEDWINDOW, 100,100,600,300,nullptr,nullptr,wc.hInstance,nullptr);
        ShowWindow(game, SW_SHOW); SetForegroundWindow(game);
        GameNotifications notifications;
        pump(notifications, 200);
        require(GetForegroundWindow() == game, "Test window could not become foreground");
        alt(true); pump(notifications, 50);
        notifications.queue(game, L"Configuration 测试 日本語 한국어 enabled.");
        pump(notifications, 300);
        require(submitted.empty() && !notifications.typing(), "Sent while Alt was held");
        alt(false); pump(notifications, 900);
        require(submitted == std::vector<std::wstring>{L"Configuration 测试 日本語 한국어 enabled."}, "Unicode message mismatch");
        // Never send an unrelated existing draft when opening the prompt.
        chat = true; draft = L"do not submit this draft";
        notifications.queue(game, L"magicKey disabled."); pump(notifications, 900);
        require(submitted.size() == 2 && submitted.back() == L"magicKey disabled.", "Existing draft was submitted");
        notifications.queue(game, L"magicKey enabled."); pump(notifications, 900);
        require(submitted.size() == 3 && submitted.back() == L"magicKey enabled." && !chat, "Enable notification mismatch");
        // Different foreground HWND in the same process still cancels delivery.
        HWND other = CreateWindowW(wc.lpszClassName, L"Other app receiver", WS_OVERLAPPEDWINDOW, 150,150,500,250,nullptr,nullptr,wc.hInstance,nullptr);
        notifications.queue(game, L"must not be sent");
        ShowWindow(other, SW_SHOW); SetForegroundWindow(other); pump(notifications, 900);
        require(submitted.size() == 3, "Message leaked after focus changed");
        notifications.queue(game, L"background must not send"); pump(notifications, 500);
        require(submitted.size() == 3, "Background target accepted");
        DestroyWindow(other); DestroyWindow(game);
        std::cout << "Game notification input, Unicode, modifiers, drafts and focus tests passed.\n";
        return 0;
    } catch (const std::exception& error) { alt(false); std::cerr << error.what() << '\n'; return 1; }
}
