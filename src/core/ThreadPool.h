#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace winnow {

// A fixed set of worker threads pulling from a shared task queue.
//
// A thread pool is three things: some threads, a queue, and a condition
// variable so idle threads sleep instead of spinning. The reason to have one
// here rather than starting a thread per photo is cost -- creating an OS thread
// costs far more than decoding a small JPEG, so a library of 10,000 photos
// would spend most of its time in the scheduler.
class ThreadPool {
public:
    // threads == 0 asks the runtime how many cores are available.
    explicit ThreadPool(unsigned threads = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    unsigned threadCount() const { return static_cast<unsigned>(workers_.size()); }

    // Run body(i) for every i in [0, count), and return once all have finished.
    //
    // Work is handed out dynamically: each worker atomically claims the next
    // index when it becomes free, rather than each being given a fixed slice up
    // front. That matters because the per-item cost here is wildly uneven -- a
    // 48-megapixel JPEG takes far longer to decode than a thumbnail. With fixed
    // slices, one worker dealt the large files would still be running long
    // after the others had finished and gone idle.
    void forEachIndex(size_t count, const std::function<void(size_t)>& body);

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable taskAvailable_;
    bool stopping_ = false;
};

} // namespace winnow
