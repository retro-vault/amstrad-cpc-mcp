//
// Minimal PNG writer for palette-indexed images.
//
// Images with at most sixteen colours use four-bit pixels. Larger hardware
// palettes, including the CPC Gate Array's 32 colours plus sync diagnostics,
// use eight-bit pixels. Both forms retain palette indices exactly and avoid
// expanding every pixel to RGB before the image enters an MCP response.
//
// only what PNG requires is implemented: signature, IHDR, PLTE, IDAT,
// IEND, no filtering, no interlacing, no ancillary chunks. DEFLATE and
// CRC come from the vendored miniz.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef PNG_ENCODER_H
#define PNG_ENCODER_H

#include <cstdint>
#include <span>
#include <vector>

namespace png {

//
// one palette colour.
//
struct colour {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

//
// Largest palette an eight-bit indexed image can carry.
//
inline constexpr std::size_t max_palette_size = 256;

//
// Encode an indexed image as a PNG file.
//
// Parameters:
//      pixels      - one byte per pixel holding a palette index, in row
//                    major order. must hold width * height entries.
//                    indices at or above the palette size are clamped.
//      width       - image width in pixels, must be positive.
//      height      - image height in pixels, must be positive.
//      palette     - up to 256 colours.
//      scale       - integer magnification, nearest neighbour. 1 leaves
//                    the image alone.
//
// Returns:
//      the complete PNG file, or an empty vector when the arguments do
//      not describe a valid image.
//
// Notes:
//      the result is a byte stream, not text. base64 encode it before
//      putting it in a JSON message.
//
// Sample call:
//      auto file = png::encode_indexed(fb.pixels(), fb.width(),
//                                      fb.height(), palette, 1);
//
std::vector<std::uint8_t> encode_indexed(
    std::span<const std::uint8_t> pixels, int width, int height,
    std::span<const colour> palette, int scale = 1);

} // namespace png

#endif // PNG_ENCODER_H
