#pragma once

#include <cstdint>
#include <vector>

namespace winnow {

// A greyscale image as a flat row-major byte buffer.
//
// Everything in core/ works on this type rather than on a Qt image, a file path
// or a decoder handle. That is deliberate: the perceptual hash, the sharpness
// score and the downscaler are then pure functions over plain memory, so they
// can be tested with hand-built pixel arrays and no image files on disk.
//
// Colour is discarded before this point. Both algorithms that consume a
// GrayImage care about *structure* (edges, gradients), and luminance carries
// that; chroma mostly adds noise and triples the work.
struct GrayImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // size() == width * height

    bool empty() const {
        return width <= 0 || height <= 0
            || pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height);
    }

    uint8_t at(int x, int y) const {
        return pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
    }

    void resizeTo(int w, int h) {
        width = w;
        height = h;
        pixels.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    }
};

} // namespace winnow
