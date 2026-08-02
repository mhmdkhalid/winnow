#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace winnow {

// Largest usable threshold. Above this the pigeonhole guarantee in
// CandidateIndex stops holding, so the index could miss a genuine match.
inline constexpr int kMaxDistanceThreshold = 7;

struct GroupingStats {
    size_t comparisons = 0;      // Hamming distances actually evaluated
    size_t naiveComparisons = 0; // what comparing every pair would have cost
};

// Partition photo hashes into groups of near-duplicates.
//
// Two photos belong together when their hashes differ in at most `threshold`
// bits. Groups are the transitive closure of that relation, built with
// union-find, and only groups of two or more are returned. Returned indices are
// ascending within a group, and groups are ordered by their first member, so
// the output is deterministic and testable.
//
// Note the transitivity, because it is a real behavioural consequence and worth
// stating before someone finds it: if A matches B and B matches C, all three
// land in one group even when A and C are further apart than the threshold. On
// a burst of photographs -- each frame slightly different from the last, the
// first and last quite different -- that chaining is usually what you want. On
// a large library of visually flat images it can over-merge.
std::vector<std::vector<size_t>> groupNearDuplicates(const std::vector<uint64_t>& hashes,
                                                     int threshold,
                                                     GroupingStats* stats = nullptr);

} // namespace winnow
