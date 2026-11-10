#pragma once
#include <windows.h>
#include <winver.h>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace magickey {
using WarcraftVersion = std::array<unsigned, 4>;

inline std::optional<WarcraftVersion> warcraftVersion(const std::filesystem::path& gameDll) {
    DWORD ignored = 0;
    DWORD size = GetFileVersionInfoSizeW(gameDll.c_str(), &ignored);
    if (!size) return std::nullopt;
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(gameDll.c_str(), 0, size, data.data())) return std::nullopt;
    void* value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", &value, &length) || length < sizeof(VS_FIXEDFILEINFO))
        return std::nullopt;
    auto info = static_cast<const VS_FIXEDFILEINFO*>(value);
    if (info->dwSignature != 0xFEEF04BD) return std::nullopt;
    return WarcraftVersion{HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                           HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS)};
}

inline bool supportsWarcraftMana(const WarcraftVersion& version) {
    return version == WarcraftVersion{1, 24, 4, 6387} ||
           version == WarcraftVersion{1, 26, 0, 6401} ||
           version == WarcraftVersion{1, 27, 0, 52240} ||
           version == WarcraftVersion{1, 27, 1, 7085};
}

inline std::optional<bool> wfeHeroManaEnabled(const std::filesystem::path& profile) {
    wchar_t bars[16]{}, mana[16]{}, heroes[16]{};
    GetPrivateProfileStringW(L"CUSTOMBARS", L"ISENABLED", L"", bars, 16, profile.c_str());
    GetPrivateProfileStringW(L"MANABAR", L"ISENABLED", L"", mana, 16, profile.c_str());
    GetPrivateProfileStringW(L"MANABAR", L"ISHEROONLY", L"", heroes, 16, profile.c_str());
    auto yesOrNo = [](const wchar_t* value) {
        return _wcsicmp(value, L"yes") == 0 || _wcsicmp(value, L"no") == 0;
    };
    if (!yesOrNo(bars) || !yesOrNo(mana) ||
        (_wcsicmp(heroes, L"On") != 0 && _wcsicmp(heroes, L"Off") != 0))
        return std::nullopt;
    return _wcsicmp(bars, L"yes") == 0 && _wcsicmp(mana, L"yes") == 0 && _wcsicmp(heroes, L"On") == 0;
}

inline bool saveWfeHeroMana(const std::filesystem::path& profile, bool enabled) {
    if (!wfeHeroManaEnabled(profile).has_value()) return false;
    wchar_t temp[MAX_PATH]{};
    if (!GetTempFileNameW(profile.parent_path().c_str(), L"MKH", 0, temp)) return false;
    bool saved = CopyFileW(profile.c_str(), temp, FALSE) &&
        WritePrivateProfileStringW(L"MANABAR", L"ISENABLED", enabled ? L"yes" : L"no", temp);
    if (saved && enabled) {
        saved = WritePrivateProfileStringW(L"CUSTOMBARS", L"ISENABLED", L"yes", temp) &&
                WritePrivateProfileStringW(L"MANABAR", L"ISHEROONLY", L"On", temp);
    }
    if (saved) saved = MoveFileExW(temp, profile.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!saved) DeleteFileW(temp);
    return saved;
}
}
