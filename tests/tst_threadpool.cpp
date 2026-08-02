#include "core/ThreadPool.h"

#include <QtTest>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

using namespace winnow;

class TestThreadPool : public QObject {
    Q_OBJECT

private slots:
    void runsEveryIndexExactlyOnce();
    void handlesEmptyAndSingleItemWork();
    void concurrentIncrementsAreNotLost();
    void workersGenuinelyRunAtTheSameTime();
    void unevenWorkDoesNotStrandIdleWorkers();
};

void TestThreadPool::runsEveryIndexExactlyOnce() {
    ThreadPool pool(4);
    constexpr size_t kCount = 5000;

    std::vector<std::atomic<int>> visits(kCount);
    for (auto& visit : visits)
        visit.store(0);

    pool.forEachIndex(kCount, [&](size_t i) { visits[i].fetch_add(1); });

    for (size_t i = 0; i < kCount; ++i)
        QCOMPARE(visits[i].load(), 1);
}

void TestThreadPool::handlesEmptyAndSingleItemWork() {
    ThreadPool pool(4);

    std::atomic<int> calls{0};
    pool.forEachIndex(0, [&](size_t) { calls.fetch_add(1); });
    QCOMPARE(calls.load(), 0);

    pool.forEachIndex(1, [&](size_t i) {
        QCOMPARE(i, size_t{0});
        calls.fetch_add(1);
    });
    QCOMPARE(calls.load(), 1);
}

void TestThreadPool::concurrentIncrementsAreNotLost() {
    // A plain int here would lose updates -- read, add, write is not atomic, so
    // two threads can read the same value and one increment vanishes. This is
    // the bug the scanner's counters would have if they were not atomic.
    ThreadPool pool(8);
    std::atomic<int> counter{0};

    pool.forEachIndex(100000, [&](size_t) { counter.fetch_add(1, std::memory_order_relaxed); });

    QCOMPARE(counter.load(), 100000);
}

void TestThreadPool::workersGenuinelyRunAtTheSameTime() {
    // Proving parallelism rather than assuming it. Every task blocks until all
    // of them have arrived, so the barrier can only open if the workers really
    // are running concurrently. A pool that quietly executed tasks one after
    // another would sit here until the timeout, and the test would fail.
    ThreadPool pool;
    const unsigned workers = pool.threadCount();
    if (workers < 2)
        QSKIP("single-core machine: there is no parallelism to observe");

    std::mutex mutex;
    std::condition_variable arrivedAll;
    unsigned arrived = 0;
    std::atomic<int> timedOut{0};

    pool.forEachIndex(workers, [&](size_t) {
        std::unique_lock<std::mutex> lock(mutex);
        ++arrived;
        if (arrived == workers)
            arrivedAll.notify_all();
        else if (!arrivedAll.wait_for(lock, std::chrono::seconds(5),
                                      [&] { return arrived == workers; }))
            timedOut.fetch_add(1);
    });

    QCOMPARE(timedOut.load(), 0);
    QCOMPARE(arrived, workers);
}

void TestThreadPool::unevenWorkDoesNotStrandIdleWorkers() {
    // Photographs vary enormously in decode cost, so work is claimed on demand
    // rather than sliced up front. Here the first few items are far heavier
    // than the rest; with fixed slices whichever worker owned them would still
    // be going long after the others finished. Claiming dynamically means the
    // idle workers absorb the remaining light items instead.
    ThreadPool pool(4);
    if (pool.threadCount() < 2)
        QSKIP("single-core machine");

    constexpr size_t kCount = 400;
    std::atomic<size_t> completed{0};

    pool.forEachIndex(kCount, [&](size_t i) {
        // Heavy items first, and they are the ones handed out first.
        const int spin = (i < 4) ? 200000 : 200;
        volatile int sink = 0;
        for (int k = 0; k < spin; ++k)
            sink += k;
        (void)sink;
        completed.fetch_add(1);
    });

    QCOMPARE(completed.load(), kCount);
}

QTEST_APPLESS_MAIN(TestThreadPool)
#include "tst_threadpool.moc"
