#pragma once

#include "core/ExifDate.h"
#include "core/GrayImage.h"

#include <cstdint>
#include <optional>
#include <string>

namespace winnow {

// One photograph in the library, after analysis.
struct Photo {
    std::string path;
    uint64_t fileSize = 0;
    uint64_t hash = 0;        // perceptual hash; meaningless unless decoded
    double sharpness = 0.0;   // Laplacian variance; comparable within a group only
    std::optional<DateTaken> dateTaken;
    int width = 0;
    int height = 0;
    bool decoded = false;     // false when the file could not be read or parsed
};

// What a decoder hands back: the greyscale pixels the algorithms need, plus the
// metadata that only the file itself can supply.
struct DecodedPhoto {
    GrayImage gray;
    std::optional<DateTaken> dateTaken;
    int width = 0;
    int height = 0;
};

// Turns a file on disk into pixels.
//
// Declared in core/ and deliberately mentions no image format, no Qt and no
// codec -- just "give me this path as greyscale". The Qt implementation lives
// in imaging/ and depends on this, not the other way round. That inversion is
// what lets every test in this project run against a fake decoder that
// synthesises images in memory: no photograph files in the repository, no
// codec, and a full scan test finishes in milliseconds.
//
// Implementations must be safe to call from several threads at once, because
// the scanner does exactly that.
class PhotoDecoder {
public:
    virtual ~PhotoDecoder() = default;
    virtual std::optional<DecodedPhoto> decode(const std::string& path) const = 0;
};

} // namespace winnow
