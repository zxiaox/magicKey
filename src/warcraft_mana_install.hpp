#pragma once
#include <windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace magickey {
struct ManaPlugin {
    const char* bytes = nullptr;
    DWORD size = 0;
    explicit operator bool() const { return bytes && size; }
};

inline ManaPlugin embeddedManaPlugin() {
    const auto app = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(app, MAKEINTRESOURCEW(103), MAKEINTRESOURCEW(10));
    if (!resource) return {};
    const auto data = LoadResource(app, resource);
    return {static_cast<const char*>(data ? LockResource(data) : nullptr), SizeofResource(app, resource)};
}

enum class ManaPluginState { Missing, Installed, Foreign };

inline ManaPluginState manaPluginState(const std::filesystem::path& path, ManaPlugin plugin) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? ManaPluginState::Missing : ManaPluginState::Foreign;
    if (!plugin || attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        return ManaPluginState::Foreign;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() != std::streamoff(plugin.size)) return ManaPluginState::Foreign;
    in.seekg(0);
    std::vector<char> content(plugin.size);
    if (!in.read(content.data(), content.size()) || std::memcmp(content.data(), plugin.bytes, plugin.size) != 0)
        return ManaPluginState::Foreign;
    return ManaPluginState::Installed;
}

inline bool installManaPlugin(const std::filesystem::path& path, ManaPlugin plugin) {
    if (!plugin) return false;
    wchar_t temp[MAX_PATH]{};
    if (!GetTempFileNameW(path.parent_path().c_str(), L"MKM", 0, temp)) return false;
    std::ofstream out(std::filesystem::path(temp), std::ios::binary | std::ios::trunc);
    out.write(plugin.bytes, plugin.size);
    out.close();
    const bool installed = out && MoveFileExW(temp, path.c_str(), MOVEFILE_WRITE_THROUGH);
    if (!installed) DeleteFileW(temp);
    return installed;
}
}
