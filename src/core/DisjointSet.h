#pragma once

#include <cstddef>
#include <numeric>
#include <vector>

namespace winnow {

// Union-find over a fixed number of elements.
//
// Grouping near-duplicates produces facts of the form "photo 3 matches photo
// 17". Those facts arrive in arbitrary order and have to be merged into groups,
// which is exactly the problem union-find solves: keep a forest where every
// element points towards a representative, and merging two groups is one
// pointer write.
//
// Both standard optimisations are here. Union by size keeps trees shallow by
// always hanging the smaller tree under the larger. Path compression flattens
// the path on every lookup, so repeated queries get cheaper. Together they make
// each operation effectively constant time.
class DisjointSet {
public:
    explicit DisjointSet(size_t count)
        : parent_(count), size_(count, 1) {
        std::iota(parent_.begin(), parent_.end(), size_t{0});
    }

    size_t find(size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]]; // path halving
            x = parent_[x];
        }
        return x;
    }

    void unite(size_t a, size_t b) {
        size_t ra = find(a);
        size_t rb = find(b);
        if (ra == rb)
            return;
        if (size_[ra] < size_[rb])
            std::swap(ra, rb);
        parent_[rb] = ra;
        size_[ra] += size_[rb];
    }

private:
    std::vector<size_t> parent_;
    std::vector<size_t> size_;
};

} // namespace winnow
