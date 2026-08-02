#include "core/Sharpness.h"

namespace winnow {

double laplacianVariance(const GrayImage& image) {
    // The kernel reads one pixel in each direction, so the outermost ring has no
    // complete neighbourhood and is skipped. Anything smaller than 3x3 has no
    // interior at all.
    if (image.empty() || image.width < 3 || image.height < 3)
        return 0.0;

    double sum = 0.0;
    double sumSquares = 0.0;
    size_t count = 0;

    for (int y = 1; y < image.height - 1; ++y) {
        for (int x = 1; x < image.width - 1; ++x) {
            // 4-neighbour Laplacian:   0  1  0
            //                          1 -4  1
            //                          0  1  0
            // Zero on any flat or evenly-sloping region; large in magnitude only
            // where the gradient itself changes, i.e. at an edge.
            const int response = static_cast<int>(image.at(x, y - 1))
                               + static_cast<int>(image.at(x, y + 1))
                               + static_cast<int>(image.at(x - 1, y))
                               + static_cast<int>(image.at(x + 1, y))
                               - 4 * static_cast<int>(image.at(x, y));

            sum += response;
            sumSquares += static_cast<double>(response) * response;
            ++count;
        }
    }

    if (count == 0)
        return 0.0;

    const double mean = sum / static_cast<double>(count);
    const double variance = sumSquares / static_cast<double>(count) - mean * mean;
    return variance > 0.0 ? variance : 0.0;
}

} // namespace winnow
