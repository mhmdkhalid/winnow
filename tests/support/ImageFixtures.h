#pragma once

#include "core/GrayImage.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace winnow {
namespace fixtures {

// ---------------------------------------------------------------------------
// Synthetic greyscale images
//
// Every image the tests use is generated, not loaded. That keeps binary files
// out of the repository, makes each case exactly reproducible, and lets a test
// state its intent directly -- "a checkerboard", "the same scene two stops
// brighter" -- instead of relying on what happens to be in a sample photo.
// ---------------------------------------------------------------------------

inline GrayImage solid(int width, int height, uint8_t value) {
    GrayImage image;
    image.resizeTo(width, height);
    std::fill(image.pixels.begin(), image.pixels.end(), value);
    return image;
}

// A diagonal gradient with a bright rectangle sitting on it.
//
// The rectangle matters. A pure gradient increases left-to-right, so every
// "is this sample brighter than the one to its right" comparison answers no and
// the hash comes out as all zeros -- which would make the tests pass for the
// wrong reason. The block creates edges in both directions.
//
// Peak value is kept at 220 so that tests can brighten the image without any
// pixel clipping at 255.
//
// `variant` slides the block sideways, producing a genuinely different picture
// rather than a noisy version of the same one.
inline GrayImage scene(int width, int height, int variant = 0) {
    GrayImage image;
    image.resizeTo(width, height);

    const int blockX0 = width / 8 + variant * (width / 6);
    const int blockX1 = blockX0 + width / 4;
    const int blockY0 = height / 8;
    const int blockY1 = blockY0 + height / 4;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int value = (x * 90) / std::max(1, width - 1)
                      + (y * 90) / std::max(1, height - 1);
            if (x >= blockX0 && x < blockX1 && y >= blockY0 && y < blockY1)
                value = 220;
            image.pixels[static_cast<size_t>(y) * width + x] =
                static_cast<uint8_t>(std::min(255, std::max(0, value)));
        }
    }
    return image;
}

// The same picture with every pixel raised, clipped at 255 -- what a stop of
// extra exposure looks like.
inline GrayImage brighter(const GrayImage& source, int amount) {
    GrayImage out = source;
    for (uint8_t& pixel : out.pixels)
        pixel = static_cast<uint8_t>(std::min(255, static_cast<int>(pixel) + amount));
    return out;
}

// A deterministic pseudo-random speckle, as a stand-in for sensor noise.
inline GrayImage withNoise(const GrayImage& source, int amplitude, uint32_t seed = 1) {
    GrayImage out = source;
    uint32_t state = seed;
    for (uint8_t& pixel : out.pixels) {
        state = state * 1664525u + 1013904223u; // numerical-recipes LCG
        const int delta = static_cast<int>((state >> 16) % (2 * amplitude + 1)) - amplitude;
        pixel = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(pixel) + delta)));
    }
    return out;
}

// Average each pixel with its neighbours, repeatedly. This is what being out of
// focus does: it removes high-frequency detail.
inline GrayImage blurred(const GrayImage& source, int passes = 1) {
    GrayImage current = source;
    for (int pass = 0; pass < passes; ++pass) {
        GrayImage next = current;
        for (int y = 1; y < current.height - 1; ++y) {
            for (int x = 1; x < current.width - 1; ++x) {
                const int sum = current.at(x - 1, y) + current.at(x + 1, y)
                              + current.at(x, y - 1) + current.at(x, y + 1)
                              + current.at(x, y);
                next.pixels[static_cast<size_t>(y) * current.width + x] =
                    static_cast<uint8_t>(sum / 5);
            }
        }
        current = next;
    }
    return current;
}

// ---------------------------------------------------------------------------
// A minimal but genuinely valid JPEG carrying an EXIF timestamp
//
// The EXIF parser reads real camera files, so testing it against a hand-rolled
// byte array proves more than testing it against one sample photo would: the
// layout is written out explicitly here, so a test failure points at exactly
// which field moved.
// ---------------------------------------------------------------------------

class ExifJpegBuilder {
public:
    explicit ExifJpegBuilder(bool littleEndian = true) : little_(littleEndian) {}

    std::vector<uint8_t> build(int year, int month, int day,
                               int hour, int minute, int second) const {
        // TIFF block layout, all offsets relative to its own start:
        //    0  byte-order mark ("II" or "MM")
        //    2  magic number 42
        //    4  offset of IFD0                        -> 8
        //    8  IFD0: one entry, the Exif IFD pointer -> 26
        //   26  Exif IFD: one entry, DateTimeOriginal -> 44
        //   44  "YYYY:MM:DD HH:MM:SS\0"               (20 bytes)
        constexpr uint32_t kIfd0Offset = 8;
        constexpr uint32_t kExifIfdOffset = 26;
        constexpr uint32_t kStringOffset = 44;

        std::vector<uint8_t> tiff;
        if (little_) { tiff.push_back('I'); tiff.push_back('I'); }
        else         { tiff.push_back('M'); tiff.push_back('M'); }
        put16(tiff, 42);
        put32(tiff, kIfd0Offset);

        // IFD0
        put16(tiff, 1);                                   // entry count
        putEntry(tiff, 0x8769, 4 /*LONG*/, 1, kExifIfdOffset);
        put32(tiff, 0);                                   // no next IFD

        // Exif sub-IFD
        put16(tiff, 1);
        putEntry(tiff, 0x9003, 2 /*ASCII*/, 20, kStringOffset);
        put32(tiff, 0);

        char stamp[24];
        std::snprintf(stamp, sizeof(stamp), "%04d:%02d:%02d %02d:%02d:%02d",
                      year, month, day, hour, minute, second);
        for (int i = 0; i < 19; ++i)
            tiff.push_back(static_cast<uint8_t>(stamp[i]));
        tiff.push_back(0); // the count of 20 includes the terminator

        // APP1 payload is the "Exif\0\0" tag followed by the whole TIFF block.
        std::vector<uint8_t> payload = {'E', 'x', 'i', 'f', 0, 0};
        payload.insert(payload.end(), tiff.begin(), tiff.end());

        std::vector<uint8_t> jpeg = {0xFF, 0xD8};         // SOI
        jpeg.push_back(0xFF);
        jpeg.push_back(0xE1);                             // APP1
        const uint16_t segmentLength = static_cast<uint16_t>(payload.size() + 2);
        jpeg.push_back(static_cast<uint8_t>(segmentLength >> 8)); // always big-endian
        jpeg.push_back(static_cast<uint8_t>(segmentLength & 0xFF));
        jpeg.insert(jpeg.end(), payload.begin(), payload.end());
        jpeg.push_back(0xFF);
        jpeg.push_back(0xD9);                             // EOI
        return jpeg;
    }

private:
    void put16(std::vector<uint8_t>& out, uint16_t value) const {
        if (little_) { out.push_back(value & 0xFF); out.push_back(value >> 8); }
        else         { out.push_back(value >> 8); out.push_back(value & 0xFF); }
    }

    void put32(std::vector<uint8_t>& out, uint32_t value) const {
        if (little_) {
            out.push_back(value & 0xFF);
            out.push_back((value >> 8) & 0xFF);
            out.push_back((value >> 16) & 0xFF);
            out.push_back((value >> 24) & 0xFF);
        } else {
            out.push_back((value >> 24) & 0xFF);
            out.push_back((value >> 16) & 0xFF);
            out.push_back((value >> 8) & 0xFF);
            out.push_back(value & 0xFF);
        }
    }

    void putEntry(std::vector<uint8_t>& out, uint16_t tag, uint16_t type,
                  uint32_t count, uint32_t value) const {
        put16(out, tag);
        put16(out, type);
        put32(out, count);
        put32(out, value);
    }

    bool little_;
};

// A structurally valid JPEG with no metadata at all.
inline std::vector<uint8_t> jpegWithoutExif() {
    return {0xFF, 0xD8, 0xFF, 0xD9};
}

} // namespace fixtures
} // namespace winnow
