// RGB colour type for the on-board addressable (WS2812) LED.
//
// Pure and hardware-free so it can be used and tested on the host. The driver
// turns a LedColor into an actual WS2812 write.

#pragma once

#include <cstdint>

namespace cbhal {

struct LedColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

constexpr bool operator==(const LedColor& a, const LedColor& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

constexpr bool operator!=(const LedColor& a, const LedColor& b) { return !(a == b); }

// A few convenient named colours.
namespace colors {
constexpr LedColor Off{0, 0, 0};
constexpr LedColor Red{255, 0, 0};
constexpr LedColor Green{0, 255, 0};
constexpr LedColor Blue{0, 0, 255};
constexpr LedColor White{255, 255, 255};
}        // namespace colors

}        // namespace cbhal
