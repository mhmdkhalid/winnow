#pragma once

#include "core/Photo.h"

#include <QtGlobal>

namespace winnow {

// How large a decoded image is allowed to be, on its longest side.
//
// The perceptual hash reduces everything to a 9x8 grid, so decoding a 48
// megapixel photograph at full size and then throwing away 99.99% of the pixels
// is pure waste. JPEG can be decoded at a reduced scale directly by the codec,
// which is dramatically cheaper than decoding fully and shrinking afterwards.
//
// It is not taken lower than this because the sharpness score still needs real
// detail to measure -- below roughly this size, blur and focus start to look
// alike.
inline constexpr int kDecodeLongestSide = 256;

// How much of the file to read when hunting for the EXIF block. The APP1
// segment sits at the very start of a JPEG, immediately after the two-byte
// start-of-image marker, so reading the opening chunk is enough and avoids
// pulling a 20 MB file through memory to read 20 bytes of it.
inline constexpr qint64 kExifScanBytes = 128 * 1024;

// The real decoder: the single file in this project that knows JPEG and PNG
// exist. Everything above it works through the PhotoDecoder interface, which is
// why the whole pipeline can be tested without a codec.
class QtPhotoDecoder : public PhotoDecoder {
public:
    std::optional<DecodedPhoto> decode(const std::string& path) const override;
};

} // namespace winnow
