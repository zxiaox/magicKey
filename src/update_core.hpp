#pragma once
#include <array>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
#include "app_config.hpp"

namespace magickey::updates {
constexpr std::uint64_t MaxExecutableSize = 128 * 1024 * 1024;
inline const std::string Repository = MAGICKEY_REPOSITORY;
inline const std::string ReleasesUrl = "https://github.com/" + Repository + "/releases";
inline const std::string LatestApi = "https://api.github.com/repos/" + Repository + "/releases/latest";
using Version = std::array<unsigned, 3>;
inline Version parseVersion(std::string_view text) {
    if (!text.empty() && text.front() == 'v') text.remove_prefix(1);
    Version result{};
    for (std::size_t part = 0; part < result.size(); ++part) {
        auto end = text.find('.');
        auto token = text.substr(0, end);
        if (token.empty() || token.size() > 6 || (token.size() > 1 && token.front() == '0'))
            throw std::runtime_error("Invalid stable version");
        for (char c : token) {
            if (c < '0' || c > '9') throw std::runtime_error("Invalid stable version");
            result[part] = result[part] * 10 + unsigned(c - '0');
        }
        if (part == 2) { if (end != text.npos) throw std::runtime_error("Invalid stable version"); }
        else {
            if (end == text.npos) throw std::runtime_error("Invalid stable version");
            text.remove_prefix(end + 1);
        }
    }
    return result;
}
inline std::string normalizeHash(std::string text) {
    if (text.size() != 64) throw std::runtime_error("Invalid SHA-256");
    for (char& c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            throw std::runtime_error("Invalid SHA-256");
        if (c >= 'A' && c <= 'F') c = char(c + ('a' - 'A'));
    }
    return text;
}
inline std::string parseChecksum(const std::string& text) {
    std::istringstream input(text);
    std::string hash, filename, extra;
    if (!(input >> hash >> filename) || (filename != "MagicKey.exe" && filename != "*MagicKey.exe") || (input >> extra))
        throw std::runtime_error("Invalid checksum file");
    return normalizeHash(hash);
}
struct Release {
    std::string tag, url, checksumUrl, digest;
    std::uint64_t size = 0;
};
inline Release parseRelease(const std::string& body) {
    const auto json = nlohmann::json::parse(body);
    if (!json.is_object() || json.at("draft").get<bool>() || json.at("prerelease").get<bool>())
        throw std::runtime_error("Not a stable release");
    Release result;
    result.tag = json.at("tag_name").get<std::string>();
    (void)parseVersion(result.tag);
    if (!json.at("assets").is_array()) throw std::runtime_error("Invalid assets");
    const auto prefix = ReleasesUrl + "/download/" + result.tag + "/";
    for (const auto& asset : json.at("assets")) {
        const auto name = asset.at("name").get<std::string>();
        if (name != "MagicKey.exe" && name != "MagicKey.exe.sha256") continue;
        const auto url = asset.at("browser_download_url").get<std::string>();
        if (url != prefix + name) throw std::runtime_error("Unexpected asset URL");
        if (name == "MagicKey.exe") {
            if (!result.url.empty()) throw std::runtime_error("Duplicate executable");
            result.url = url;
            const auto& size = asset.at("size");
            if (!size.is_number_unsigned() && !(size.is_number_integer() && size.get<std::int64_t>() > 0))
                throw std::runtime_error("Invalid asset size");
            result.size = size.get<std::uint64_t>();
            if (!result.size || result.size > MaxExecutableSize) throw std::runtime_error("Invalid asset size");
            if (asset.contains("digest") && !asset["digest"].is_null()) {
                auto digest = asset["digest"].get<std::string>();
                if (digest.rfind("sha256:", 0) != 0) throw std::runtime_error("Invalid asset digest");
                result.digest = normalizeHash(digest.substr(7));
            }
        } else {
            if (!result.checksumUrl.empty()) throw std::runtime_error("Duplicate checksum");
            result.checksumUrl = url;
        }
    }
    if (result.url.empty() || (result.digest.empty() && result.checksumUrl.empty()))
        throw std::runtime_error("Release needs executable and checksum");
    return result;
}

// Quote a single argument for the Windows CRT command-line parser. No shell is used.
inline std::wstring quoteArgument(std::wstring_view arg) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}
inline bool isX64Executable(const std::vector<unsigned char>& header) {
    if (header.size() < 64 || header[0] != 'M' || header[1] != 'Z') return false;
    std::uint32_t offset = std::uint32_t(header[60]) | (std::uint32_t(header[61]) << 8) |
        (std::uint32_t(header[62]) << 16) | (std::uint32_t(header[63]) << 24);
    if (offset > header.size() || header.size() - offset < 26) return false;
    return header[offset] == 'P' && header[offset + 1] == 'E' && header[offset + 2] == 0 &&
        header[offset + 3] == 0 && header[offset + 4] == 0x64 && header[offset + 5] == 0x86 &&
        header[offset + 24] == 0x0B && header[offset + 25] == 0x02;
}
} // namespace magickey::updates
