#include "core/ThreadPool.h"

#include <algorithm>

namespace winnow {

ThreadPool::ThreadPool(unsigned threads) {
    if (threads == 0)
        threads = std::max(1u, std::thread::hardware_concurrency());
    workers_.reserve(threads);
    for (unsigned i = 0; i < threads; ++i)
        workers_.emplace_back([this] { workerLoop(); });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    // Wake every worker so they all observe stopping_ and return. Notifying
    // outside the lock avoids waking a thread that would immediately block
    // trying to acquire the mutex this scope still holds.
    taskAvailable_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable())
            worker.join();
    }
}

void ThreadPool::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // The predicate form is essential: a condition variable may wake
            // spuriously, and re-checking under the lock is what makes that
            // harmless.
            taskAvailable_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

            if (stopping_ && tasks_.empty())
                return;

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task(); // run outside the lock, or the pool would be serial
    }
}

void ThreadPool::forEachIndex(size_t count, const std::function<void(size_t)>& body) {
    if (count == 0)
        return;

    const unsigned workers = threadCount();

    // With one worker, or one item, the handover is pure overhead.
    if (workers <= 1 || count == 1) {
        for (size_t i = 0; i < count; ++i)
            body(i);
        return;
    }

    std::atomic<size_t> nextIndex{0};
    std::atomic<size_t> remaining{workers};
    std::mutex doneMutex;
    std::condition_variable allDone;

    // One task per worker; each drains the shared counter. Queueing `count`
    // separate tasks would allocate a std::function per photo for no gain.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (unsigned w = 0; w < workers; ++w) {
            tasks_.push([&] {
                for (;;) {
                    const size_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (i >= count)
                        break;
                    body(i);
                }
                // Signal completion under the mutex the waiter uses, so it
                // cannot check the counter and start waiting in between.
                std::lock_guard<std::mutex> doneLock(doneMutex);
                if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    allDone.notify_one();
            });
        }
    }
    taskAvailable_.notify_all();

    std::unique_lock<std::mutex> doneLock(doneMutex);
    allDone.wait(doneLock, [&] { return remaining.load(std::memory_order_acquire) == 0; });
}

} // namespace winnow
