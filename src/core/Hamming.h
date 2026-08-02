#pragma once

#include <cstdint>

namespace winnow {

// Number of set bits in v.
//
// On x86-64 the builtin compiles to a single POPCNT instruction, which matters
// because grouping a large library evaluates this millions of times. The
// fallback is the classic SWAR bit-twiddle: sum bits pairwise, then in nibbles,
// then bytes, then multiply by 0x0101...01 so the top byte holds the total.
inline int popcount64(uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(v);
#else
    v = v - ((v >> 1) & 0x5555555555555555ULL);
    v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
    v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return static_cast<int>((v * 0x0101010101010101ULL) >> 56);
#endif
}

// How many of the 64 bits differ between two perceptual hashes.
//
// This is the whole similarity measure: XOR marks every position where the two
// images disagreed about a gradient, and popcount counts them. 0 means the
// hashes are identical; small values mean "the same picture".
inline int hammingDistance(uint64_t a, uint64_t b) {
    return popcount64(a ^ b);
}

} // namespace winnow
