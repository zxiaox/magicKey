#include "theme_ui.hpp"
#include "localization.hpp"
#include "app_config.hpp"
#include <dwmapi.h>
#include <uxtheme.h>
#include <shlwapi.h>
#include <algorithm>
#include <string>

namespace magickey {
namespace {
void fill(HDC dc, RECT box, std::uint32_t rgb) {
    HBRUSH brush = CreateSolidBrush(color(rgb)); FillRect(dc, &box, brush); DeleteObject(brush);
}
void roundBox(HDC dc, RECT box, std::uint32_t background, std::uint32_t edge, int radius = 12) {
    HBRUSH brush = CreateSolidBrush(color(background));
    HPEN pen = CreatePen(PS_SOLID, 1, color(edge));
    auto oldBrush = SelectObject(dc, brush); auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(brush); DeleteObject(pen);
}
void line(HDC dc, int x1, int y1, int x2, int y2, std::uint32_t rgb, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color(rgb)); auto old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2); SelectObject(dc, old); DeleteObject(pen);
}
void text(HDC dc, const wchar_t* label, RECT box, HFONT font, std::uint32_t rgb, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) {
    auto old = SelectObject(dc, font); SetTextColor(dc, color(rgb)); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, label, -1, &box, flags); SelectObject(dc, old);
}
HFONT makeFont(int height, int weight) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}
LRESULT CALLBACK listSubclass(HWND hwnd, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR value) {
    if (message == WM_NOTIFY) {
        auto draw = reinterpret_cast<NMCUSTOMDRAW*>(l);
        if (draw->hdr.code == NM_CUSTOMDRAW && draw->hdr.hwndFrom == ListView_GetHeader(hwnd))
            return reinterpret_cast<ThemeUI*>(value)->header(draw);
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(hwnd, listSubclass, id);
    return DefSubclassProc(hwnd, message, w, l);
}
LRESULT CALLBACK comboSubclass(HWND hwnd, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR value) {
    LRESULT result = DefSubclassProc(hwnd, message, w, l);
    auto ui = reinterpret_cast<ThemeUI*>(value);
    if (message == WM_PAINT) {
        HDC dc = GetDC(hwnd); ui->comboFrame(hwnd, dc); ReleaseDC(hwnd, dc);
    } else if (message == WM_PRINT || message == WM_PRINTCLIENT) {
        ui->comboFrame(hwnd, reinterpret_cast<HDC>(w));
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(hwnd, comboSubclass, id);
    return result;
}
}
ThemeUI::~ThemeUI() {
    delete artwork;
    if (artworkStream) artworkStream->Release();
    if (graphicsToken) Gdiplus::GdiplusShutdown(graphicsToken);
    for (auto brush : {backgroundBrush, panelBrush, inputBrush}) if (brush) DeleteObject(brush);
    for (auto font : {bodyFont, smallFont, strongFont, headingFont}) if (font) DeleteObject(font);
}
void ThemeUI::initialize(ThemeId theme) {
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&graphicsToken, &startup, nullptr) == Gdiplus::Ok) {
        auto module = GetModuleHandleW(nullptr);
        auto resource = FindResourceW(module, MAKEINTRESOURCEW(102), RT_RCDATA);
        auto data = resource ? LoadResource(module, resource) : nullptr;
        if (data) {
            artworkStream = SHCreateMemStream(static_cast<const BYTE*>(LockResource(data)), SizeofResource(module, resource));
            if (artworkStream) {
                artwork = Gdiplus::Image::FromStream(artworkStream);
                if (artwork->GetLastStatus() != Gdiplus::Ok) { delete artwork; artwork = nullptr; }
            }
        }
    }

    bodyFont = makeFont(15, FW_NORMAL); smallFont = makeFont(13, FW_NORMAL);
    strongFont = makeFont(15, FW_SEMIBOLD); headingFont = makeFont(30, FW_SEMIBOLD);
    apply(nullptr, nullptr, theme);
}
void ThemeUI::apply(HWND root, HWND list, ThemeId theme) {
    selected = theme;
    for (auto brush : {backgroundBrush, panelBrush, inputBrush}) if (brush) DeleteObject(brush);
    const auto& p = colors();
    backgroundBrush = CreateSolidBrush(color(p.background));
    panelBrush = CreateSolidBrush(color(p.panel)); inputBrush = CreateSolidBrush(color(p.control));
    if (list) {
        ListView_SetBkColor(list, color(p.panel)); ListView_SetTextBkColor(list, color(p.panel));
        ListView_SetTextColor(list, color(p.text));
        SetWindowTheme(list, L"", L"");
        SetWindowSubclass(list, listSubclass, 1, reinterpret_cast<DWORD_PTR>(this));
    }
    if (root) {
        BOOL dark = p.dark;
        DwmSetWindowAttribute(root, 20, &dark, sizeof(dark));
        COLORREF caption = color(p.background), ink = color(p.text);
        DwmSetWindowAttribute(root, 35, &caption, sizeof(caption));
        DwmSetWindowAttribute(root, 36, &ink, sizeof(ink));
        RedrawWindow(root, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }
}
HBRUSH ThemeUI::staticBackground(HWND child, HWND root) const {
    RECT rect; GetWindowRect(child, &rect); MapWindowPoints(nullptr, root, reinterpret_cast<POINT*>(&rect), 2);
    return rect.top >= 118 ? panelBrush : backgroundBrush;
}
void ThemeUI::paint(HDC dc, const RECT& bounds, bool enabled, int language) const {
    const auto& p = colors();
    auto tr = [language](const wchar_t* s) { return mywarkey::translate(s, language); };
    fill(dc, bounds, p.background);
    if (selected == ThemeId::Warcraft) {
        if (artwork) {
            Gdiplus::Graphics graphics(dc);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            const float width = float(artwork->GetWidth()), height = float(artwork->GetHeight());
            // Cover the whole client area, then show the face prominently in the header.
            Gdiplus::ColorMatrix matrix = {1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,0.32f,0, 0,0,0,0,1};
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&matrix);
            float sourceHeight = width * float(bounds.bottom) / float(bounds.right);
            graphics.DrawImage(artwork, Gdiplus::Rect(0,0,bounds.right,bounds.bottom),
                0.0f, (height-sourceHeight)/2, width, sourceHeight, Gdiplus::UnitPixel, &attributes);
            matrix.m[3][3] = 0.75f; attributes.SetColorMatrix(&matrix);
            graphics.DrawImage(artwork, Gdiplus::Rect(480,0,420,103),
                0.0f, 58.0f, width, width*103/420, Gdiplus::UnitPixel, &attributes);
        }
        line(dc, 24, 103, 876, 103, p.border);
    }
    for (RECT card : {RECT{24,118,270,592}, RECT{288,118,876,478}, RECT{288,494,876,592}, RECT{24,610,876,662}}) {
        roundBox(dc, card, p.panel, p.border, selected == ThemeId::Warcraft ? 3 : 16);
        if (selected == ThemeId::Warcraft) {
            for (int x : {card.left + 6, card.right - 6}) for (int y : {card.top + 6, card.bottom - 6}) {
                int dx = x < card.left + 20 ? 12 : -12, dy = y < card.top + 20 ? 12 : -12;
                line(dc, x, y, x + dx, y, p.accent); line(dc, x, y, x, y + dy, p.accent);
            }
        }
    }
    auto icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, 52, 52, LR_SHARED));
    if (icon) DrawIconEx(dc, 27, 24, icon, 52, 52, 0, nullptr, DI_NORMAL);
    text(dc, L"MagicKey", {92,23,360,61}, headingFont, selected == ThemeId::Warcraft ? p.accent : p.text);
    const auto subtitle = std::wstring(L"v") + MAGICKEY_VERSION_W + L"  /  " + tr(L"Portable keyboard control");
    text(dc, subtitle.c_str(), {94,64,550,86}, smallFont, p.muted);
    line(dc, 44, 377, 250, 377, p.border);
    text(dc, tr(L"Toggle remapping"), {44,495,250,515}, smallFont, p.muted);
    roundBox(dc, {44,523,250,557}, p.control, p.border, 8);
    text(dc, L"Ctrl + Alt + F12", {44,523,250,557}, strongFont, p.accent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, tr(L"Profiles save automatically."), {44,565,250,583}, smallFont, p.muted);
    roundBox(dc, {40,632,48,640}, enabled ? p.success : p.muted, enabled ? p.success : p.muted, 8);
    text(dc, L"\x2192", {500,390,522,430}, strongFont, p.muted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
LRESULT ThemeUI::button(NMCUSTOMDRAW* draw, ButtonRole role, bool listening) const {
    if (draw->dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    const auto& p = colors();
    HWND button = draw->hdr.hwndFrom;
    const bool disabled = !IsWindowEnabled(button), hot = draw->uItemState & CDIS_HOT;
    const bool pressed = draw->uItemState & CDIS_SELECTED;
    RECT box = draw->rc; InflateRect(&box, -1, -1);
    wchar_t label[512]{}; GetWindowTextW(button, label, 512);
    if (role == ButtonRole::Checkbox) {
        fill(draw->hdc, draw->rc, p.panel);
        RECT check{box.left + 1, box.top + (box.bottom - box.top - 17) / 2, box.left + 18, box.top + (box.bottom - box.top - 17) / 2 + 17};
        const bool checked = SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED;
        roundBox(draw->hdc, check, checked ? p.accent : p.control, hot ? p.accent : p.border, 5);
        if (checked) {
            line(draw->hdc, check.left + 4, check.top + 8, check.left + 7, check.top + 11, p.accentInk, 2);
            line(draw->hdc, check.left + 7, check.top + 11, check.left + 13, check.top + 5, p.accentInk, 2);
        }
        box.left += 29; text(draw->hdc, label, box, bodyFont, disabled ? p.muted : p.text);
    } else {
        fill(draw->hdc, draw->rc, p.panel);
        const bool primary = role == ButtonRole::Primary;
        auto surface = primary ? p.accent : (hot || pressed || listening) ? p.hover : p.control;
        auto edge = (primary || listening || hot) ? p.accent : p.border;
        auto ink = disabled ? p.muted : primary ? p.accentInk : role == ButtonRole::Danger ? p.muted : p.text;
        roundBox(draw->hdc, box, disabled ? p.control : surface, edge, selected == ThemeId::Warcraft ? 4 : 10);
        text(draw->hdc, label, box, strongFont, listening ? p.accent : ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    if ((draw->uItemState & CDIS_FOCUS) && !(SendMessageW(button, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        RECT focus = draw->rc; InflateRect(&focus, -4, -4); DrawFocusRect(draw->hdc, &focus);
    }
    return CDRF_SKIPDEFAULT;
}
LRESULT ThemeUI::list(NMLVCUSTOMDRAW* draw) const {
    if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;
    const auto& p = colors();
    HWND list = draw->nmcd.hdr.hwndFrom; int row = int(draw->nmcd.dwItemSpec);
    RECT rect = draw->nmcd.rc, client; GetClientRect(list, &client); rect.right = client.right;
    const bool selectedRow = ListView_GetItemState(list, row, LVIS_SELECTED) != 0;
    fill(draw->nmcd.hdc, rect, selectedRow ? p.selection : p.panel);
    const int divider = ListView_GetColumnWidth(list, 0);
    for (int column = 0; column < 2; ++column) {
        wchar_t label[512]{}; ListView_GetItemText(list, row, column, label, 512);
        SIZE size{}; auto old = SelectObject(draw->nmcd.hdc, strongFont);
        GetTextExtentPoint32W(draw->nmcd.hdc, label, int(wcslen(label)), &size); SelectObject(draw->nmcd.hdc, old);
        int left = column ? divider + 12 : 12;
        int maximum = column ? client.right - left - 12 : divider - left - 40;
        RECT cap{left, rect.top + 5, left + std::min(int(size.cx) + 22, maximum), rect.bottom - 5};
        roundBox(draw->nmcd.hdc, cap, p.control, selectedRow ? p.accent : p.border, 7);
        text(draw->nmcd.hdc, label, cap, strongFont, p.text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    text(draw->nmcd.hdc, L"\x2192", {divider-32,rect.top,divider-8,rect.bottom}, bodyFont, p.muted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (selectedRow && GetFocus() == list) { InflateRect(&rect, -2, -2); DrawFocusRect(draw->nmcd.hdc, &rect); }
    return CDRF_SKIPDEFAULT;
}
LRESULT ThemeUI::header(NMCUSTOMDRAW* draw) const {
    if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (draw->dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;
    wchar_t label[512]{};
    HDITEMW item{}; item.mask = HDI_TEXT; item.pszText = label; item.cchTextMax = 512;
    Header_GetItem(draw->hdr.hwndFrom, int(draw->dwItemSpec), &item);
    const auto& p = colors(); fill(draw->hdc, draw->rc, p.panel);
    RECT box = draw->rc; box.left += 12;
    text(draw->hdc, label, box, smallFont, p.muted);
    line(draw->hdc, draw->rc.left, draw->rc.bottom - 1, draw->rc.right, draw->rc.bottom - 1, p.border);
    return CDRF_SKIPDEFAULT;
}
void ThemeUI::combo(DRAWITEMSTRUCT* draw) const {
    const auto& p = colors();
    const bool highlighted = draw->itemState & ODS_SELECTED;
    fill(draw->hDC, draw->rcItem, highlighted ? p.selection : p.control);
    if (draw->itemID != UINT(-1)) {
        wchar_t label[512]{}; SendMessageW(draw->hwndItem, CB_GETLBTEXT, draw->itemID, LPARAM(label));
        RECT box = draw->rcItem; box.left += 10; box.right -= 6;
        text(draw->hDC, label, box, bodyFont, p.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    if ((draw->itemState & ODS_FOCUS) && !(draw->itemState & ODS_NOFOCUSRECT)) DrawFocusRect(draw->hDC, &draw->rcItem);
}
void ThemeUI::prepareCombo(HWND combo) {
    SetWindowSubclass(combo, comboSubclass, 1, reinterpret_cast<DWORD_PTR>(this));
}
void ThemeUI::comboFrame(HWND combo, HDC dc) const {
    const auto& p = colors();
    RECT box; GetClientRect(combo, &box);
    const bool focused = GetFocus() == combo && !(SendMessageW(combo, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
    fill(dc, box, p.control);
    roundBox(dc, box, p.control, focused ? p.accent : p.border, selected == ThemeId::Warcraft ? 3 : 8);
    const auto index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index != CB_ERR) {
        wchar_t label[512]{}; SendMessageW(combo, CB_GETLBTEXT, index, LPARAM(label));
        RECT content = box; content.left += 10; content.right -= 27;
        text(dc, label, content, bodyFont, IsWindowEnabled(combo) ? p.text : p.muted,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    int x = box.right - 15, y = (box.bottom + box.top) / 2;
    bool dropped = SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) != 0;
    int dy = dropped ? -2 : 2;
    line(dc, x - 4, y - dy, x, y + dy, p.muted);
    line(dc, x, y + dy, x + 4, y - dy, p.muted);
}
}
