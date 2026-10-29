#pragma once
namespace mywarkey {
struct ToggleHotkeyResult { bool swallow = false, toggle = false; };
class ToggleHotkey {
    bool held = false;
public:
    ToggleHotkeyResult event(int key, bool down, bool wasDown, bool control, bool alt) {
        if (key != 0x7B) return {}; // Physical F12.
        if (held) {
            if (!down) held = false;
            return {true, false};
        }
        if (down && !wasDown && control && alt) {
            held = true;
            return {true, true};
        }
        return {};
    }
};
}
