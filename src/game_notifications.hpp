#pragma once
#include <windows.h>
#include <string>
namespace mywarkey {
// UI-thread state machine. All input stays targeted to the original foreground
// game/process; no window activation, clipboard writes, or game memory access.
class GameNotifications {
    HWND target = nullptr;
    DWORD process = 0;
    std::wstring pending, current;
    ULONGLONG deadline = 0, next = 0;
    int stage = 0;
    bool sameTarget() const;
    bool idle() const;
    bool key(WORD scan);
public:
    void queue(HWND game, std::wstring message);
    void tick();
    void cancel();
    bool readyToStart() const { return stage == 0 && !pending.empty() && sameTarget() && idle(); }
    bool typing() const { return stage != 0; }
};
}
