#include "../src/themes.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

void require(bool value) { if (!value) throw std::runtime_error("Theme test failed"); }
double luminance(std::uint32_t rgb) {
    auto channel = [](unsigned value) {
        double x = value / 255.0;
        return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
    };
    return .2126 * channel((rgb >> 16) & 255) + .7152 * channel((rgb >> 8) & 255) + .0722 * channel(rgb & 255);
}
double contrast(std::uint32_t a, std::uint32_t b) {
    double first = luminance(a), second = luminance(b);
    return (std::max(first, second) + .05) / (std::min(first, second) + .05);
}
int main() {
    using namespace magickey;
    for (int i = 0; i < int(Themes.size()); ++i) {
        const auto& p = palette(ThemeId(i));
        require(*p.name && contrast(p.text, p.panel) >= 4.5 && contrast(p.text, p.control) >= 4.5);
        require(contrast(p.muted, p.panel) >= 4.5 && contrast(p.accentInk, p.accent) >= 4.5);
        require(contrast(p.text, p.selection) >= 4.5);
        std::stringstream preference;
        writeTheme(preference, ThemeId(i));
        require(readTheme(preference) == ThemeId(i));
    }
    for (const auto* text : {"", "MAGICKEY_THEME 0 0", "MAGICKEY_THEME 1 -1", "MAGICKEY_THEME 1 4",
        "MAGICKEY_THEME 1 x", "OTHER 1 0", "MAGICKEY_THEME 1 0 extra"}) {
        bool rejected = false;
        try { std::istringstream input(text); (void)readTheme(input); }
        catch (const std::exception&) { rejected = true; }
        require(rejected);
    }
    std::istringstream whitespace("MAGICKEY_THEME 1 3\r\n\t");
    require(readTheme(whitespace) == ThemeId::Warcraft);
    require(palette(ThemeId::Warcraft).dark && !palette(ThemeId::Ivory).dark);
    std::cout << "Four theme preferences, invalid files, and text contrast ratios passed.\n";
}
