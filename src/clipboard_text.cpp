#include "clipboard_text.hpp"
#include <ole2.h>
#include <cstring>
namespace mywarkey {
namespace {
void freeData(UINT format, HANDLE data) {
    if (format == CF_BITMAP || format == CF_PALETTE || format == CF_DSPBITMAP) DeleteObject(data);
    else if (format == CF_ENHMETAFILE || format == CF_DSPENHMETAFILE) DeleteEnhMetaFile(static_cast<HENHMETAFILE>(data));
    else if (format == CF_METAFILEPICT || format == CF_DSPMETAFILEPICT) {
        auto picture = static_cast<METAFILEPICT*>(GlobalLock(data));
        if (picture) { DeleteMetaFile(picture->hMF); GlobalUnlock(data); }
        GlobalFree(data);
    } else GlobalFree(data);
}
HANDLE duplicate(UINT format, HANDLE data) {
    if (format == CF_ENHMETAFILE || format == CF_DSPENHMETAFILE)
        return CopyEnhMetaFileW(static_cast<HENHMETAFILE>(data), nullptr);
    if (format == CF_OWNERDISPLAY || (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST)) return nullptr;
    UINT kind = format == CF_DSPBITMAP ? CF_BITMAP : format == CF_DSPMETAFILEPICT ? CF_METAFILEPICT : format;
    return OleDuplicateData(data, CLIPFORMAT(kind), 0);
}
}
void ClipboardText::discard() {
    for (const auto& item : saved) freeData(item.format, item.data);
    saved.clear(); active = false;
}
ClipboardText::~ClipboardText() {
    restore(); discard();
    if (owner) DestroyWindow(owner);
}
bool ClipboardText::put(const std::wstring& text) {
    if (!restore()) return false;
    if (!owner) owner = CreateWindowExW(0, L"STATIC", L"MagicKey clipboard owner", 0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!owner) return false;
    HGLOBAL replacement = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!replacement) return false;
    auto buffer = GlobalLock(replacement);
    if (!buffer) { GlobalFree(replacement); return false; }
    std::memcpy(buffer, text.c_str(), (text.size() + 1) * sizeof(wchar_t)); GlobalUnlock(replacement);
    if (!OpenClipboard(owner)) { GlobalFree(replacement); return false; }
    // Duplicate every existing format before changing anything. If one cannot
    // be preserved safely, leave the clipboard untouched and report failure.
    SetLastError(ERROR_SUCCESS);
    UINT format = 0;
    bool valid = true;
    while ((format = EnumClipboardFormats(format)) != 0) {
        HANDLE original = GetClipboardData(format);
        HANDLE copy = original ? duplicate(format, original) : nullptr;
        if (!copy) { valid = false; break; }
        saved.push_back({format, copy});
        SetLastError(ERROR_SUCCESS);
    }
    if (!valid || GetLastError() != ERROR_SUCCESS || !EmptyClipboard()) {
        CloseClipboard(); GlobalFree(replacement); discard(); return false;
    }
    bool placed = SetClipboardData(CF_UNICODETEXT, replacement) != nullptr;
    if (!placed) GlobalFree(replacement);
    sequence = GetClipboardSequenceNumber(); active = true;
    CloseClipboard();
    if (!placed) restore();
    return placed;
}
bool ClipboardText::restore() {
    if (!active) return true;
    if (GetClipboardSequenceNumber() != sequence) { discard(); return true; } // Respect a newer user copy.
    if (!OpenClipboard(owner)) return false;
    if (GetClipboardSequenceNumber() != sequence) { CloseClipboard(); discard(); return true; }
    if (!EmptyClipboard()) { CloseClipboard(); return false; }
    bool restored = true;
    for (auto& item : saved) {
        if (SetClipboardData(item.format, item.data)) item.data = nullptr;
        else restored = false;
    }
    CloseClipboard();
    for (const auto& item : saved) if (item.data) freeData(item.format, item.data);
    saved.clear(); active = false;
    return restored;
}
}
