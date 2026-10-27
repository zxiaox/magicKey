#pragma once
#include <windows.h>
#include <commctrl.h>
#include "themes.hpp"

namespace magickey {
enum class ButtonRole { Secondary, Primary, Danger, Key, Checkbox };
class ThemeUI {
    ThemeId selected = ThemeId::Ivory;
    HBRUSH backgroundBrush = nullptr, panelBrush = nullptr, inputBrush = nullptr;
    HFONT bodyFont = nullptr, smallFont = nullptr, strongFont = nullptr, headingFont = nullptr;
public:
    ~ThemeUI();
    void initialize(ThemeId theme);
    void apply(HWND root, HWND list, ThemeId theme);
    ThemeId theme() const { return selected; }
    const Palette& colors() const { return palette(selected); }
    HFONT body() const { return bodyFont; }
    HFONT small() const { return smallFont; }
    HBRUSH background() const { return backgroundBrush; }
    HBRUSH input() const { return inputBrush; }
    HBRUSH staticBackground(HWND child, HWND root) const;
    void paint(HDC dc, const RECT& bounds, bool enabled, int language) const;
    LRESULT button(NMCUSTOMDRAW* draw, ButtonRole role, bool listening) const;
    LRESULT list(NMLVCUSTOMDRAW* draw) const;
    LRESULT header(NMCUSTOMDRAW* draw) const;
    void combo(DRAWITEMSTRUCT* draw) const;
};
inline COLORREF color(std::uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
}
