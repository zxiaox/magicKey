#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace mywarkey {
class ClipboardText {
    struct Saved { UINT format; HANDLE data; };
    std::vector<Saved> saved;
    HWND owner = nullptr;
    DWORD sequence = 0;
    bool active = false;
    void discard();
public:
    ClipboardText() = default;
    ClipboardText(const ClipboardText&) = delete;
    ClipboardText& operator=(const ClipboardText&) = delete;
    ~ClipboardText();
    bool put(const std::wstring& text);
    bool restore();
};
}
