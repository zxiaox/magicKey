#include "../src/game_notifications.hpp"
#include "../src/profile_hotkeys.hpp"
#include <cstring>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace mywarkey;
bool chat = false, packet = false, enterTimingValid = true;
ULONGLONG enterDown = 0;
int escapes = 0, asciiPackets = 0, pastes = 0;
std::wstring draft;
std::vector<std::wstring> submitted;
LRESULT CALLBACK receiver(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_KEYDOWN) {
        packet = w == VK_PACKET;
        if (w == VK_ESCAPE) { ++escapes; chat = false; draft.clear(); return 0; }
        if (w == 'V' && (GetKeyState(VK_CONTROL) & 0x8000) && chat) {
            if (OpenClipboard(window)) {
                // Model classic Warcraft's narrow clipboard reader, rather
                // than assuming it asks Windows for UTF-16 text.
                auto data = GetClipboardData(CF_TEXT);
                auto value = data ? static_cast<const char*>(GlobalLock(data)) : nullptr;
                if (value) {
                    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
                    if (count > 0) {
                        std::wstring decoded(count, L'\0');
                        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, decoded.data(), count);
                        decoded.pop_back(); draft += decoded; ++pastes;
                    }
                    GlobalUnlock(data);
                }
                CloseClipboard();
            }
            return 0;
        }
        if (w == VK_RETURN) enterDown = GetTickCount64();
    }
    // Model a game's frame-based Enter handling: instantaneous down/up pairs
    // are rejected, even though a normal text editor would accept them.
    if (message == WM_KEYUP && w == VK_RETURN) {
        if (!enterDown || GetTickCount64() - enterDown < 50) { enterTimingValid = false; return 0; }
        if (chat) { submitted.push_back(draft); draft.clear(); }
        chat = !chat; enterDown = 0; return 0;
    }
    if ((message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && w == VK_MENU) return 0;
    if (message == WM_CHAR) {
        if (w >= 32 && chat) { if (packet && w < 128) ++asciiPackets; draft += wchar_t(w); }
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
void clipboardText(HWND window, const std::wstring& text, bool custom = true) {
    require(OpenClipboard(window), "Cannot seed test clipboard");
    EmptyClipboard();
    auto data = GlobalAlloc(GMEM_MOVEABLE, (text.size()+1)*sizeof(wchar_t));
    auto value = GlobalLock(data); std::memcpy(value, text.c_str(), (text.size()+1)*sizeof(wchar_t)); GlobalUnlock(data);
    require(SetClipboardData(CF_UNICODETEXT, data) != nullptr, "Cannot set clipboard text");
    if (custom) {
        auto binary = GlobalAlloc(GMEM_MOVEABLE, 8); auto bytes = GlobalLock(binary);
        std::memcpy(bytes, "KEEP123", 8); GlobalUnlock(binary);
        require(SetClipboardData(RegisterClipboardFormatW(L"MagicKey.Test.Binary"), binary) != nullptr, "Cannot set custom clipboard format");
    }
    CloseClipboard();
}
void checkClipboard(HWND window, const std::wstring& expected, bool custom = true) {
    require(OpenClipboard(window), "Cannot inspect restored clipboard");
    auto data = GetClipboardData(CF_UNICODETEXT);
    auto value = data ? static_cast<const wchar_t*>(GlobalLock(data)) : nullptr;
    bool correct = value && expected == value;
    if (value) GlobalUnlock(data);
    bool binaryCorrect = true;
    if (custom) {
        auto binary = GetClipboardData(RegisterClipboardFormatW(L"MagicKey.Test.Binary"));
        auto bytes = binary ? GlobalLock(binary) : nullptr;
        binaryCorrect = bytes && std::memcmp(bytes, "KEEP123", 8) == 0;
        if (bytes) GlobalUnlock(binary);
    }
    CloseClipboard();
    if (!correct || !binaryCorrect) std::cerr << "Clipboard verification: text=" << correct << ", custom=" << binaryCorrect << ", formats=" << CountClipboardFormats() << "\n";
    require(correct && binaryCorrect, "Clipboard text or custom format was not preserved");
}
int main() {
    try {
        WNDCLASSW wc{}; wc.lpfnWndProc = receiver;
#ifdef MAGICKEY_WINDOW_FALLBACK_TEST
        wc.lpszClassName = L"Warcraft III";
#else
        wc.lpszClassName = L"MagicKey.Notification.Test";
#endif
        wc.hInstance = GetModuleHandleW(nullptr);
        RegisterClassW(&wc);
        HWND game = CreateWindowW(wc.lpszClassName, L"Notification input receiver", WS_OVERLAPPEDWINDOW, 100,100,600,300,nullptr,nullptr,wc.hInstance,nullptr);
        ShowWindow(game, SW_SHOW); SetForegroundWindow(game);
        GameNotifications notifications;
        pump(notifications, 200);
        require(GetForegroundWindow() == game, "Test window could not become foreground");
#ifdef MAGICKEY_WINDOW_FALLBACK_TEST
        // A title alone must not identify an unrelated foreground app.
        WNDCLASSW ordinary = wc; ordinary.lpszClassName = L"MagicKey.Ordinary.Test";
        RegisterClassW(&ordinary);
        HWND spoofTitle = CreateWindowW(ordinary.lpszClassName, L"Warcraft III", WS_OVERLAPPEDWINDOW, 200,200,500,200,nullptr,nullptr,wc.hInstance,nullptr);
        ShowWindow(spoofTitle, SW_SHOW); SetForegroundWindow(spoofTitle); pump(notifications, 100);
        notifications.queue(spoofTitle, L"must not send to title-only match"); pump(notifications, 100);
        require(!notifications.readyToStart() && notifications.diagnostics().find(L"not recognized") != std::wstring::npos, "Title-only window was recognized");
        DestroyWindow(spoofTitle); SetForegroundWindow(game); pump(notifications, 100);
#endif
        clipboardText(game, L"original clipboard");
        alt(true); pump(notifications, 50);
        notifications.queue(game, L"Configuration 测试 日本語 한국어 enabled.");
#ifdef MAGICKEY_WINDOW_FALLBACK_TEST
        require(notifications.diagnostics().find(L"recognized by classic Warcraft window class") != std::wstring::npos, "Classic class fallback was not used");
#endif
        pump(notifications, 300);
        require(submitted.empty() && !notifications.typing(), "Sent while Alt was held");
        alt(false); pump(notifications, 900);
        require(submitted == std::vector<std::wstring>{L"Configuration 测试 日本語 한국어 enabled."}, "Unicode message mismatch");
        notifications.queue(game, L"magicKey disabled."); pump(notifications, 1000);
        require(submitted.size() == 2 && submitted.back() == L"magicKey disabled.", "Disable message mismatch");
        notifications.queue(game, L"magicKey enabled."); pump(notifications, 1000);
        require(submitted.size() == 3 && submitted.back() == L"magicKey enabled." && !chat, "Enable notification mismatch");
        require(escapes == 0, "An Escape preceded the normal chat sequence");
        require(enterTimingValid, "Enter was not held long enough for a game frame");
        require(asciiPackets == 0, "English chat used Unicode packets instead of ordinary keys");
        require(notifications.diagnostics().find(L"delivered to Windows") != std::wstring::npos, "No delivery diagnostic");
        require(pastes == 3, "All messages must paste complete text, including English");
        checkClipboard(game, L"original clipboard");
        const wchar_t* expectedProfiles[]{L"Configuration 诸葛亮 enabled.", L"配置「诸葛亮」已启用。", L"設定「诸葛亮」を有効にしました。", L"설정 「诸葛亮」이 활성화되었습니다."};
        const wchar_t* expectedEnabled[]{L"magicKey enabled.", L"magicKey 已启用。", L"magicKey を有効にしました。", L"magicKey가 활성화되었습니다."};
        const wchar_t* expectedDisabled[]{L"magicKey disabled.", L"magicKey 已停用。", L"magicKey を無効にしました。", L"magicKey가 비활성화되었습니다."};
        for (int language = 0; language < 4; ++language) {
            notifications.queue(game, configurationMessage(L"诸葛亮", language)); pump(notifications, 600);
            require(submitted.back() == expectedProfiles[language], "Exact localized profile message differs");
            checkClipboard(game, L"original clipboard");
            notifications.queue(game, toggleMessage(true, language)); pump(notifications, 600);
            require(submitted.back() == expectedEnabled[language], "Exact enabled message differs");
            checkClipboard(game, L"original clipboard");
            notifications.queue(game, toggleMessage(false, language)); pump(notifications, 600);
            require(submitted.back() == expectedDisabled[language], "Exact disabled message differs");
            checkClipboard(game, L"original clipboard");
        }
        // Modern clipboard readers must still receive real UTF-16 text.
        {
            ClipboardText modern;
            require(modern.put(L"诸葛亮 日本語 한국어", false), "Cannot prepare modern clipboard");
            checkClipboard(game, L"诸葛亮 日本語 한국어", false);
            require(modern.restore(), "Modern clipboard restoration failed");
            checkClipboard(game, L"original clipboard");
        }
        // A user copy made during the paste must win over the saved snapshot.
        notifications.queue(game, configurationMessage(L"新方案", 1));
        pump(notifications, 160); clipboardText(game, L"new user copy", false);
        pump(notifications, 600); checkClipboard(game, L"new user copy", false);
        std::size_t countBeforeFocusLoss = submitted.size();
        // Different foreground HWND in the same process still cancels delivery.
        HWND other = CreateWindowW(wc.lpszClassName, L"Other app receiver", WS_OVERLAPPEDWINDOW, 150,150,500,250,nullptr,nullptr,wc.hInstance,nullptr);
        notifications.queue(game, L"must not be sent");
        pump(notifications, 20); // Interrupt while our opening Enter is held.
        ShowWindow(other, SW_SHOW); SetForegroundWindow(other); pump(notifications, 900);
        require(submitted.size() == countBeforeFocusLoss, "Message leaked after focus changed");
        notifications.queue(game, L"background must not send"); pump(notifications, 500);
        require(submitted.size() == countBeforeFocusLoss, "Background target accepted");
        require(!(GetAsyncKeyState(VK_RETURN) & 0x8000), "Cancelled Enter remained down");
        require(notifications.diagnostics().find(L"not recognized") != std::wstring::npos, "Skipped notification has no diagnostic");
        DestroyWindow(other); DestroyWindow(game);
        std::cout << "Game notification input, timed Enter, classic UTF-8 clipboard text, exact four-language messages, clipboard preservation, modifiers and focus tests passed.\n";
        return 0;
    } catch (const std::exception& error) { alt(false); std::cerr << error.what() << '\n'; return 1; }
}
