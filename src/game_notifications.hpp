#pragma once
#include <windows.h>
#include <string>
namespace mywarkey {
class GameNotifications {
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
    std::wstring diagnostics() const { return report + (targetDescription.empty() ? L"" : L"\n\nLast foreground: " + targetDescription); }
    bool readyToStart() const { return stage == 0 && !pending.empty() && sameTarget() && idle(); }
    bool typing() const { return stage != 0; }
};
}
