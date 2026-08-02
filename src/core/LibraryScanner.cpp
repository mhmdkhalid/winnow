#include "core/LibraryScanner.h"

#include "core/PerceptualHash.h"
#include "core/Sharpness.h"

#include <mutex>

namespace winnow {

size_t ScanResult::bestInGroup(const std::vector<size_t>& group) const {
    size_t best = 0;
    for (size_t i = 1; i < group.size(); ++i) {
        const Photo& candidate = photos[group[i]];
        const Photo& incumbent = photos[group[best]];

        const double candidateArea = static_cast<double>(candidate.width) * candidate.height;
        const double incumbentArea = static_cast<double>(incumbent.width) * incumbent.height;

        if (candidate.sharpness > incumbent.sharpness
            || (candidate.sharpness == incumbent.sharpness && candidateArea > incumbentArea))
            best = i;
    }
    return best;
}

LibraryScanner::LibraryScanner(const PhotoDecoder& decoder, ThreadPool& pool)
    : decoder_(decoder), pool_(pool) {}

ScanResult LibraryScanner::scan(const std::vector<std::string>& paths,
                                int threshold,
                                const ProgressFn& onProgress,
                                const std::atomic<bool>* cancelled) const {
    ScanResult result;
    result.photos.resize(paths.size());

    std::atomic<size_t> completed{0};
    std::atomic<size_t> failures{0};
    std::mutex progressMutex;

    // Every worker writes to photos[i] for its own i and never touches another
    // element, so no lock is needed around the vector itself. That is only true
    // because it was sized up front -- a push_back here would reallocate and
    // invalidate what the other threads are writing into.
    pool_.forEachIndex(paths.size(), [&](size_t i) {
        if (cancelled && cancelled->load(std::memory_order_relaxed))
            return;

        Photo& photo = result.photos[i];
        photo.path = paths[i];

        if (auto decoded = decoder_.decode(paths[i])) {
            photo.hash = differenceHash(decoded->gray);
            photo.sharpness = laplacianVariance(decoded->gray);
            photo.dateTaken = decoded->dateTaken;
            photo.width = decoded->width;
            photo.height = decoded->height;
            photo.decoded = true;
        } else {
            failures.fetch_add(1, std::memory_order_relaxed);
        }

        const size_t done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
        if (onProgress) {
            // Serialised because the callback ends up touching the UI. The lock
            // is held only for the notification, never across the decode.
            std::lock_guard<std::mutex> lock(progressMutex);
            onProgress(done, paths.size());
        }
    });

    result.failed = failures.load();

    if (cancelled && cancelled->load(std::memory_order_relaxed))
        return result;

    // Files that failed to decode have no meaningful hash, so they are left out
    // of grouping entirely rather than being allowed to collide on hash 0.
    std::vector<uint64_t> hashes;
    std::vector<size_t> originalIndex;
    hashes.reserve(result.photos.size());
    originalIndex.reserve(result.photos.size());
    for (size_t i = 0; i < result.photos.size(); ++i) {
        if (result.photos[i].decoded) {
            hashes.push_back(result.photos[i].hash);
            originalIndex.push_back(i);
        }
    }

    for (auto& group : groupNearDuplicates(hashes, threshold, &result.stats)) {
        std::vector<size_t> mapped;
        mapped.reserve(group.size());
        for (size_t local : group)
            mapped.push_back(originalIndex[local]);
        result.groups.push_back(std::move(mapped));
    }

    return result;
}

} // namespace winnow
