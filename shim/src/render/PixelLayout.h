#pragma once

#include <cstdint>

namespace amm_shim {

// the byte order of one 32-bit pixel in a back buffer. The sRGB and UNORM variants of a format share a
// layout: both hold the bytes the display shows
enum class PixelLayout : uint8_t {
    Bgra8,
    Rgba8,
    // 10 bits per colour, red in the low bits; the top 8 bits of each colour are kept
    Rgb10A2,
};

} // namespace amm_shim
