#pragma once

#include "core/GrayImage.h"

namespace winnow {

// Shrink an image to exactly targetW x targetH by averaging each source region.
//
// Box averaging rather than nearest-neighbour sampling, and that choice is the
// point. Nearest-neighbour picks one source pixel per output pixel, so sensor
// noise or a one-pixel shift can flip an output value and therefore flip a bit
// of the hash. Averaging is a low-pass filter: it throws away exactly the
// high-frequency detail that makes two photographs of the same scene differ,
// and keeps the coarse structure that makes them the same.
GrayImage downscaleBox(const GrayImage& src, int targetW, int targetH);

} // namespace winnow
