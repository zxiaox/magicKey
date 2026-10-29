#pragma once
#include <windows.h>
#include <string>
#include "clipboard_text.hpp"
namespace mywarkey {
class GameNotifications {
    ClipboardText clipboard;
    bool pasteHeld = false, clipboardFailed = false, classicUtf8 = false;
    std::wstring lastMessage;
    bool paste(bool down);
    HWND target = nullptr;
    DWORD process = 0;
    std::wstring pending, current, targetDescription;
    std::wstring report = L"No game notification has been requested yet.";
    ULONGLONG deadline = 0, next = 0;
    int stage = 0;
    bool enterHeld = false, promptOpened = false;
    bool sameTarget() const;
    bool idle() const;
    bool enter(bool down);
    bool text();
    void reset();
public:
    void queue(HWND game, std::wstring message);
    void tick();
    void cancel();
    std::wstring diagnostics() const { return report + (targetDescription.empty() ? L"" : L"\n\nLast foreground: " + targetDescription) + (lastMessage.empty() ? L"" : L"\n\nMessage text: " + lastMessage); }
    bool readyToStart() const { return stage == 0 && !pending.empty() && sameTarget() && idle(); }
    bool typing() const { return stage != 0; }
};
}
