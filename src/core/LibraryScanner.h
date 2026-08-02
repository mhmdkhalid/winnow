#pragma once

#include "core/DuplicateGrouper.h"
#include "core/Photo.h"
#include "core/ThreadPool.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace winnow {

struct ScanResult {
    std::vector<Photo> photos;
    std::vector<std::vector<size_t>> groups; // indices into photos, each size >= 2
    GroupingStats stats;
    size_t failed = 0; // files that could not be decoded

    // Index within `group` of the photo worth keeping: the sharpest, with the
    // larger pixel count breaking ties.
    size_t bestInGroup(const std::vector<size_t>& group) const;
};

// Runs the whole analysis: decode every file, hash it, score it, group the
// results.
//
// Decoding dominates the runtime by a wide margin -- reading and decompressing a
// JPEG is thousands of times more expensive than the arithmetic performed on it
// afterwards -- so that stage is the one spread across the thread pool. The
// grouping stage that follows is single-threaded on purpose: it is already fast
// because of the band index, and it mutates shared union-find state, so
// parallelising it would add locking to save microseconds.
class LibraryScanner {
public:
    using ProgressFn = std::function<void(size_t done, size_t total)>;

    LibraryScanner(const PhotoDecoder& decoder, ThreadPool& pool);

    ScanResult scan(const std::vector<std::string>& paths,
                    int threshold,
                    const ProgressFn& onProgress = {},
                    const std::atomic<bool>* cancelled = nullptr) const;

private:
    const PhotoDecoder& decoder_;
    ThreadPool& pool_;
};

} // namespace winnow
