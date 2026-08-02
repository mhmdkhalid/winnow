#pragma once

#include "core/GrayImage.h"

namespace winnow {

// How sharp an image looks, as the variance of its Laplacian response.
//
// The Laplacian is a second-derivative filter: it responds to places where
// brightness changes abruptly, which is what an in-focus edge is. A sharp photo
// produces a wide spread of strong positive and negative responses, so the
// variance is high. Blur is a low-pass filter -- it smooths those transitions
// away, the responses cluster near zero, and the variance collapses.
//
// The number has no absolute meaning. It depends on how much contrast and how
// much edge detail the scene happens to contain, so a sharp photo of a blank
// wall scores lower than a blurry photo of a bookshelf. It is only ever used to
// rank photographs *within one near-duplicate group* -- pictures of the same
// subject, seconds apart -- which is precisely the case where scene content is
// held constant and the comparison is meaningful.
double laplacianVariance(const GrayImage& image);

} // namespace winnow
