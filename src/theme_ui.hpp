#pragma once
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include "themes.hpp"

namespace magickey {
enum class ButtonRole { Secondary, Primary, Danger, Key, Checkbox };
class ThemeUI {
    ULONG_PTR graphicsToken = 0;
    IStream* artworkStream = nullptr;
    Gdiplus::Image* artwork = nullptr;
    int mappingBottom = 430;
    ThemeId selected = ThemeId::Ivory;
    HBRUSH backgroundBrush = nullptr, panelBrush = nullptr, inputBrush = nullptr;
    HFONT bodyFont = nullptr, smallFont = nullptr, strongFont = nullptr, headingFont = nullptr;
public:
    ~ThemeUI();
    void initialize(ThemeId theme);
    void setMappingBottom(int bottom) { mappingBottom = bottom; }
    void apply(HWND root, HWND list, ThemeId theme);
    ThemeId theme() const { return selected; }
    const Palette& colors() const { return palette(selected); }
    HFONT body() const { return bodyFont; }
    HFONT smallTextFont() const { return smallFont; }
    HBRUSH background() const { return backgroundBrush; }
    HBRUSH panel() const { return panelBrush; }
    HBRUSH input() const { return inputBrush; }
    HBRUSH staticBackground(HWND child, HWND root) const;
    void paint(HDC dc, const RECT& bounds, bool enabled, int language, bool reverseCombo = false) const;
    LRESULT button(NMCUSTOMDRAW* draw, ButtonRole role, bool listening, bool checked = false) const;
    void ownerButton(DRAWITEMSTRUCT* item, ButtonRole role, bool listening, bool checked = false) const;
    void prepareButton(HWND button);
    LRESULT list(NMLVCUSTOMDRAW* draw) const;
    LRESULT header(NMCUSTOMDRAW* draw) const;
    void combo(DRAWITEMSTRUCT* draw) const;
    void prepareCombo(HWND combo);
    void comboFrame(HWND combo, HDC dc) const;
};
inline COLORREF color(std::uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
}
