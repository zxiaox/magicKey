#pragma once
#include <array>

namespace mywarkey {
// Canonical physical key identity, shared by capture and remapping. Non-extended
// numpad navigation VKs must remain numpad keys even with Num Lock switched off.
inline int physicalKey(int vk, unsigned scan, bool extended) {
    if (!extended) {
        switch (scan) {
            case 0x52: return 0x60; case 0x4F: return 0x61;
            case 0x50: return 0x62; case 0x51: return 0x63;
            case 0x4B: return 0x64; case 0x4C: return 0x65;
            case 0x4D: return 0x66; case 0x47: return 0x67;
            case 0x48: return 0x68; case 0x49: return 0x69;
            case 0x53: return 0x6E;
        }
    }
    return vk;
}

enum class CaptureSlot { None, Source, Target };
struct CaptureResult {
    bool swallow = false;
    CaptureSlot slot = CaptureSlot::None;
    int key = 0;
};
class KeyCapture {
    CaptureSlot slot = CaptureSlot::None;
    int candidate = 0;
    std::array<bool, 256> swallowed{};
public:
    CaptureSlot activeSlot() const { return slot; }
    void cancel() { slot = CaptureSlot::None; candidate = 0; }
    void arm(CaptureSlot next) { cancel(); slot = next; }
    CaptureResult event(int key, bool down, bool wasDown) {
        if (key < 1 || key > 254) return {};
        if (swallowed[key]) {
            if (!down) {
                swallowed[key] = false;
                if (candidate == key && slot != CaptureSlot::None) {
                    CaptureResult result{true, slot, key};
                    cancel();
                    return result;
                }
            }
            return {true};
        }
        if (slot == CaptureSlot::None || !down) return {};
        // Ignore repeats of a key that was already down when capture was armed.
        if (wasDown) return {true};
        swallowed[key] = true;
        if (!candidate) candidate = key;
        return {true};
    }
};
}
