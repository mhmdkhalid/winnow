#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace winnow {

// Number of equal slices the 64-bit hash is cut into. Eight bands of eight bits.
inline constexpr int kHashBands = 8;
inline constexpr int kHashBits = 64;
inline constexpr int kBitsPerBand = kHashBits / kHashBands;

// Finds which hashes are *worth* comparing, so that most pairs never are.
//
// The naive way to group near-duplicates is to compare every hash with every
// other hash: for 20,000 photos that is 200 million Hamming distances. This
// index removes almost all of them using a pigeonhole argument.
//
// Cut each 64-bit hash into 8 disjoint bands of 8 bits. If two hashes differ in
// at most T bits and T < 8, then those differing bits cannot possibly touch all
// eight bands -- there are not enough of them. So at least one band must be
// *bit-for-bit identical*.
//
// That turns an approximate-match problem into an exact-match one. Each band is
// a hash-table key; two photos are candidates only if they collide in at least
// one band. Everything else is provably not a near-duplicate and is never
// looked at.
//
// The guarantee holds only while the threshold stays below the band count,
// which is why the threshold is capped at kHashBands - 1 (7). The index never
// misses a true match -- it is exact, not heuristic -- but it does return false
// candidates, which the caller filters with a real Hamming distance check.
class CandidateIndex {
public:
    void add(size_t item, uint64_t hash);

    // Items sharing at least one identical band with `hash`, excluding `self`.
    // May contain duplicates removed by the caller; never omits a true match.
    std::vector<size_t> candidatesFor(uint64_t hash, size_t self) const;

    static uint64_t bandValue(uint64_t hash, int band) {
        return (hash >> (band * kBitsPerBand)) & ((uint64_t{1} << kBitsPerBand) - 1);
    }

private:
    // One hash table per band: band value -> items carrying that value there.
    std::unordered_map<uint64_t, std::vector<size_t>> buckets_[kHashBands];
};

} // namespace winnow
