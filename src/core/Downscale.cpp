#include "core/Downscale.h"

#include <algorithm>

namespace winnow {

GrayImage downscaleBox(const GrayImage& src, int targetW, int targetH) {
    GrayImage out;
    if (src.empty() || targetW <= 0 || targetH <= 0)
        return out;

    out.resizeTo(targetW, targetH);

    for (int ty = 0; ty < targetH; ++ty) {
        // The source rows this output row is responsible for. Integer maths
        // keeps the regions contiguous and gap-free; std::max guarantees at
        // least one row even when the source is smaller than the target.
        const int y0 = static_cast<int>(static_cast<int64_t>(ty) * src.height / targetH);
        const int y1 = std::max(y0 + 1,
                                static_cast<int>(static_cast<int64_t>(ty + 1) * src.height / targetH));

        for (int tx = 0; tx < targetW; ++tx) {
            const int x0 = static_cast<int>(static_cast<int64_t>(tx) * src.width / targetW);
            const int x1 = std::max(x0 + 1,
                                    static_cast<int>(static_cast<int64_t>(tx + 1) * src.width / targetW));

            uint32_t sum = 0;
            uint32_t count = 0;
            for (int y = y0; y < y1 && y < src.height; ++y) {
                for (int x = x0; x < x1 && x < src.width; ++x) {
                    sum += src.at(x, y);
                    ++count;
                }
            }

            const uint8_t value = count ? static_cast<uint8_t>(sum / count) : 0;
            out.pixels[static_cast<size_t>(ty) * targetW + tx] = value;
        }
    }

    return out;
}

} // namespace winnow
