#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
namespace mywarkey {
inline int profileDigit(int key) {
    return key == '0' ? 9 : key >= '1' && key <= '9' ? key - '1' : -1;
}
inline std::wstring profileShortcut(std::size_t index) {
    return index < 10 ? std::wstring(L"Alt + ") + (index == 9 ? L'0' : wchar_t(L'1' + index)) : L"";
}
struct ProfileHotkeyResult { bool swallow = false; int index = -1; };
class ProfileHotkeys {
    std::array<bool, 10> held{};
public:
    ProfileHotkeyResult event(int key, bool down, bool wasDown, bool alt, bool control, bool shift, std::size_t count) {
        int index = profileDigit(key);
        if (index < 0) return {};
        if (held[index]) {
            if (!down) held[index] = false;
            return {true, -1};
        }
        if (down && !wasDown && alt && !control && !shift && std::size_t(index) < count) {
            held[index] = true;
            return {true, index};
        }
        return {};
    }
};
inline bool warcraftExecutable(std::wstring_view name) {
    std::wstring lower(name);
    for (auto& c : lower) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    return lower == L"war3.exe" || lower == L"warcraft iii.exe" || lower == L"warcraft iii reforged.exe";
}
inline std::wstring configurationMessage(std::wstring name) {
    if (name.size() > 80) name.resize(80);
    for (auto& c : name) if (c < 32 || c == L'|' || c == 127) c = L' ';
    return L"Configuration " + name + L" enabled.";
}
inline std::wstring toggleMessage(bool enabled) { return enabled ? L"magicKey enabled." : L"magicKey disabled."; }
}
