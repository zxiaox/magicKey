#pragma once
#include <windows.h>
namespace magickey {
inline constexpr wchar_t AppUserModelId[] = L"zxiaox.MagicKey";
void initializeAppIdentity();
void setWindowIdentity(HWND window);
void clearWindowIdentity(HWND window);
HICON appIcon(bool compactIcon);
}
