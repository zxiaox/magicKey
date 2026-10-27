#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace magickey {
enum class ThemeId { Ivory, Midnight, Arcane, Warcraft };
struct Palette {
    const wchar_t* name;
    std::uint32_t background, panel, control, hover, border, text, muted, accent, accentInk, selection, success;
    bool dark;
};
inline constexpr std::array<Palette, 4> Themes{{
    {L"Ivory", 0xF2F5FA, 0xFFFFFF, 0xF3F5FA, 0xE7ECF6, 0xDCE3EE, 0x17233B, 0x58677D, 0x5146D9, 0xFFFFFF, 0xE8EAFE, 0x18734B, false},
    {L"Midnight", 0x0D1520, 0x172334, 0x1D2D42, 0x293D56, 0x34445B, 0xE7EDF6, 0xA9B8CA, 0x7DB6FF, 0x0C1726, 0x263C58, 0x8AD7AE, true},
    {L"Arcane", 0x171426, 0x231E39, 0x2C2546, 0x3E335C, 0x514465, 0xF0EAF9, 0xBEB0D2, 0xC3A2FF, 0x21142D, 0x40315C, 0xA6D6B4, true},
    {L"Warcraft III", 0x141714, 0x22261F, 0x2C3026, 0x3C4133, 0x6D6243, 0xEDE7D5, 0xB8B39F, 0xD7B971, 0x1A1D15, 0x44432D, 0xA2C485, true}
}};
inline bool validTheme(int value) { return value >= 0 && value < int(Themes.size()); }
inline const Palette& palette(ThemeId theme) { return Themes.at(std::size_t(theme)); }
inline ThemeId readTheme(std::istream& input) {
    std::string magic;
    int version, theme;
    if (!(input >> magic >> version >> theme) || magic != "MAGICKEY_THEME" || version != 1 || !validTheme(theme))
        throw std::runtime_error("Invalid theme preferences");
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Invalid theme preferences");
    return ThemeId(theme);
}
inline void writeTheme(std::ostream& output, ThemeId theme) {
    if (!validTheme(int(theme))) throw std::runtime_error("Invalid theme preferences");
    output << "MAGICKEY_THEME 1 " << int(theme) << '\n';
    if (!output) throw std::runtime_error("Could not save theme");
}
}
