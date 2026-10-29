#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace mywarkey {
class ClipboardText {
    struct Saved { UINT format; HANDLE data; };
    std::vector<Saved> saved;
    HWND owner = nullptr;
    bool active = false;
    void discard();
public:
    ClipboardText() = default;
    ClipboardText(const ClipboardText&) = delete;
    ClipboardText& operator=(const ClipboardText&) = delete;
    ~ClipboardText();
    bool put(const std::wstring& text, bool classicUtf8 = false);
    bool restore();
};
}
