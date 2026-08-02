#include "core/CandidateIndex.h"

#include <algorithm>

namespace winnow {

void CandidateIndex::add(size_t item, uint64_t hash) {
    for (int band = 0; band < kHashBands; ++band)
        buckets_[band][bandValue(hash, band)].push_back(item);
}

std::vector<size_t> CandidateIndex::candidatesFor(uint64_t hash, size_t self) const {
    std::vector<size_t> out;

    for (int band = 0; band < kHashBands; ++band) {
        const auto it = buckets_[band].find(bandValue(hash, band));
        if (it == buckets_[band].end())
            continue;
        for (size_t item : it->second) {
            if (item != self)
                out.push_back(item);
        }
    }

    // The same item can collide in several bands at once, so the raw list has
    // repeats. Sorting and uniquing is cheaper than checking a set on insert
    // for the small lists this produces.
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace winnow
