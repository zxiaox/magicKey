#pragma once
#include <cstdint>

namespace mywarkey {
struct KeyOutput {
    std::uint16_t vk, scan;
    std::uint32_t flags;
};
// Flags are Win32 KEYEVENTF_EXTENDEDKEY, KEYUP, and SCANCODE, respectively.
// Keeping this construction independent of Windows lets tests inspect actual output flags.
inline KeyOutput keyOutput(int vk, bool down, unsigned scan, bool isExtended, bool scanMode) {
    const bool useScan = scanMode && (scan & 0xFF) != 0;
    return {std::uint16_t(useScan ? 0 : vk), std::uint16_t(scan & 0xFF),
        std::uint32_t((isExtended ? 0x1 : 0) | (down ? 0 : 0x2) | (useScan ? 0x8 : 0))};
}
}
