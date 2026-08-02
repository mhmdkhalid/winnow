#include "core/DuplicateGrouper.h"

#include "core/CandidateIndex.h"
#include "core/DisjointSet.h"
#include "core/Hamming.h"

#include <algorithm>
#include <map>

namespace winnow {

std::vector<std::vector<size_t>> groupNearDuplicates(const std::vector<uint64_t>& hashes,
                                                     int threshold,
                                                     GroupingStats* stats) {
    std::vector<std::vector<size_t>> groups;
    if (hashes.size() < 2)
        return groups;

    threshold = std::max(0, std::min(threshold, kMaxDistanceThreshold));

    CandidateIndex index;
    for (size_t i = 0; i < hashes.size(); ++i)
        index.add(i, hashes[i]);

    DisjointSet sets(hashes.size());
    size_t comparisons = 0;

    for (size_t i = 0; i < hashes.size(); ++i) {
        for (size_t j : index.candidatesFor(hashes[i], i)) {
            // Each unordered pair surfaces from both sides; only evaluate it once.
            if (j < i)
                continue;
            ++comparisons;
            if (hammingDistance(hashes[i], hashes[j]) <= threshold)
                sets.unite(i, j);
        }
    }

    if (stats) {
        const size_t n = hashes.size();
        stats->comparisons = comparisons;
        stats->naiveComparisons = n * (n - 1) / 2;
    }

    // Collect members by representative. std::map keeps the output ordered by
    // the representative index, which makes the result reproducible run to run.
    std::map<size_t, std::vector<size_t>> byRoot;
    for (size_t i = 0; i < hashes.size(); ++i)
        byRoot[sets.find(i)].push_back(i);

    for (auto& entry : byRoot) {
        if (entry.second.size() >= 2)
            groups.push_back(std::move(entry.second));
    }

    std::sort(groups.begin(), groups.end(),
              [](const std::vector<size_t>& a, const std::vector<size_t>& b) {
                  return a.front() < b.front();
              });
    return groups;
}

} // namespace winnow
