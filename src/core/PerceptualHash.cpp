#include "core/PerceptualHash.h"

#include "core/Downscale.h"

namespace winnow {

uint64_t differenceHash(const GrayImage& image) {
    if (image.empty())
        return 0;

    const GrayImage grid = downscaleBox(image, kHashGridWidth, kHashGridHeight);
    if (grid.empty())
        return 0;

    uint64_t hash = 0;
    int bit = 0;
    for (int y = 0; y < kHashGridHeight; ++y) {
        for (int x = 0; x < kHashGridWidth - 1; ++x) {
            if (grid.at(x, y) > grid.at(x + 1, y))
                hash |= (uint64_t{1} << bit);
            ++bit;
        }
    }
    return hash;
}

} // namespace winnow
