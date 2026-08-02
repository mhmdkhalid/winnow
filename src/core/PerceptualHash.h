#pragma once

#include "core/GrayImage.h"

#include <cstdint>

namespace winnow {

// Width/height of the grid the hash is computed on. 9 wide because each row
// yields 8 comparisons between 9 adjacent samples: 8 rows x 8 comparisons = 64.
inline constexpr int kHashGridWidth = 9;
inline constexpr int kHashGridHeight = 8;

// A 64-bit "difference hash" (dHash) describing the coarse structure of an image.
//
// The image is reduced to a 9x8 grey grid, then each pixel is compared with the
// one to its right: the bit is 1 when the left sample is brighter. Sixty-four
// such comparisons make the hash.
//
// The key property is that it encodes *relative* brightness, not absolute. Two
// photographs of the same scene at different exposures have completely
// different pixel values but almost the same pattern of "is this bit brighter
// than its neighbour", so their hashes stay close. An average hash (compare
// every pixel to the image mean) does not survive that, because shifting the
// exposure shifts the mean and can flip many bits at once.
//
// This is deliberately NOT a cryptographic hash. MD5 or SHA are built so that a
// one-pixel change produces a completely unrelated output -- the exact opposite
// of what near-duplicate detection needs. Here, similar input must give similar
// output, and "similar" is measured with hammingDistance().
uint64_t differenceHash(const GrayImage& image);

} // namespace winnow
