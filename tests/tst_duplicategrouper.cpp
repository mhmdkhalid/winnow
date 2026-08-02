#include "core/CandidateIndex.h"
#include "core/DisjointSet.h"
#include "core/DuplicateGrouper.h"
#include "core/Hamming.h"

#include <QtTest>

#include <map>
#include <vector>

using namespace winnow;

namespace {

// The obvious, definitely-correct implementation: compare every pair. Far too
// slow to ship, but it is the yardstick the fast one is measured against.
std::vector<std::vector<size_t>> groupByBruteForce(const std::vector<uint64_t>& hashes,
                                                   int threshold) {
    DisjointSet sets(hashes.size());
    for (size_t i = 0; i < hashes.size(); ++i) {
        for (size_t j = i + 1; j < hashes.size(); ++j) {
            if (hammingDistance(hashes[i], hashes[j]) <= threshold)
                sets.unite(i, j);
        }
    }

    std::map<size_t, std::vector<size_t>> byRoot;
    for (size_t i = 0; i < hashes.size(); ++i)
        byRoot[sets.find(i)].push_back(i);

    std::vector<std::vector<size_t>> groups;
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

uint64_t nextRandom(uint64_t& state) {
    // xorshift64*, deterministic so a failure can be reproduced exactly.
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

uint64_t flipBits(uint64_t value, int count, uint64_t& state) {
    for (int i = 0; i < count; ++i)
        value ^= uint64_t{1} << (nextRandom(state) % 64);
    return value;
}

} // namespace

class TestDuplicateGrouper : public QObject {
    Q_OBJECT

private slots:
    void identicalHashesFormOneGroup();
    void unrelatedHashesFormNoGroups();
    void thresholdIsInclusive();
    void nearDuplicatesChainTransitively();
    void fewerThanTwoItemsProduceNothing();
    void bandIndexAgreesWithBruteForce_data();
    void bandIndexAgreesWithBruteForce();
    void bandIndexSkipsTheOverwhelmingMajorityOfComparisons();
    void everyPairWithinThresholdSharesABand();
};

void TestDuplicateGrouper::identicalHashesFormOneGroup() {
    const auto groups = groupNearDuplicates({0xABCDEF0123456789ULL, 0xABCDEF0123456789ULL}, 0);
    QCOMPARE(groups.size(), size_t{1});
    QCOMPARE(groups[0].size(), size_t{2});
}

void TestDuplicateGrouper::unrelatedHashesFormNoGroups() {
    QVERIFY(groupNearDuplicates({0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL}, 3).empty());
}

void TestDuplicateGrouper::thresholdIsInclusive() {
    const std::vector<uint64_t> hashes = {0b000ULL, 0b111ULL}; // exactly 3 bits apart

    QVERIFY(groupNearDuplicates(hashes, 2).empty());
    QCOMPARE(groupNearDuplicates(hashes, 3).size(), size_t{1});
}

void TestDuplicateGrouper::nearDuplicatesChainTransitively() {
    // A-B differ by 2 and B-C differ by 2, but A-C differ by 4 -- beyond the
    // threshold. All three still land in one group, because the groups are the
    // transitive closure of the match relation. That is the intended behaviour
    // for a burst of photographs drifting frame by frame, and it is documented
    // rather than accidental.
    const std::vector<uint64_t> hashes = {0b0000ULL, 0b0011ULL, 0b1111ULL};

    const auto groups = groupNearDuplicates(hashes, 2);
    QCOMPARE(groups.size(), size_t{1});
    QCOMPARE(groups[0].size(), size_t{3});
    QCOMPARE(hammingDistance(hashes[0], hashes[2]), 4); // still further apart than the threshold
}

void TestDuplicateGrouper::fewerThanTwoItemsProduceNothing() {
    QVERIFY(groupNearDuplicates({}, 3).empty());
    QVERIFY(groupNearDuplicates({42}, 3).empty());
}

void TestDuplicateGrouper::bandIndexAgreesWithBruteForce_data() {
    QTest::addColumn<int>("threshold");
    QTest::addColumn<int>("clusters");
    QTest::addColumn<int>("perCluster");
    QTest::addColumn<int>("loners");

    QTest::newRow("tight")   << 2 << 25 << 4 << 100;
    QTest::newRow("default") << 3 << 25 << 4 << 100;
    QTest::newRow("loose")   << 5 << 15 << 6 << 80;
    QTest::newRow("widest")  << 7 << 10 << 8 << 60;
}

void TestDuplicateGrouper::bandIndexAgreesWithBruteForce() {
    // The single most important test in the project.
    //
    // The band index exists to avoid the majority of comparisons, and the
    // argument that it is safe to do so is a pigeonhole proof rather than an
    // approximation. If that reasoning were wrong, the failure mode would be
    // silent: a few genuine duplicates would simply never be reported and
    // nothing would look broken. So the fast path is checked against the
    // exhaustive one on data built to stress it.
    QFETCH(int, threshold);
    QFETCH(int, clusters);
    QFETCH(int, perCluster);
    QFETCH(int, loners);

    uint64_t state = 0x9E3779B97F4A7C15ULL;
    std::vector<uint64_t> hashes;

    for (int c = 0; c < clusters; ++c) {
        const uint64_t base = nextRandom(state);
        hashes.push_back(base);
        for (int m = 1; m < perCluster; ++m) {
            // Spread members from just-inside to just-outside the threshold, so
            // the boundary itself is exercised rather than only easy cases.
            hashes.push_back(flipBits(base, static_cast<int>(nextRandom(state) % (threshold + 2)), state));
        }
    }
    for (int i = 0; i < loners; ++i)
        hashes.push_back(nextRandom(state));

    const auto fast = groupNearDuplicates(hashes, threshold);
    const auto exhaustive = groupByBruteForce(hashes, threshold);

    QCOMPARE(fast.size(), exhaustive.size());
    for (size_t g = 0; g < fast.size(); ++g)
        QCOMPARE(fast[g], exhaustive[g]);
}

void TestDuplicateGrouper::bandIndexSkipsTheOverwhelmingMajorityOfComparisons() {
    uint64_t state = 12345;
    std::vector<uint64_t> hashes;
    for (int c = 0; c < 40; ++c) {
        const uint64_t base = nextRandom(state);
        for (int m = 0; m < 5; ++m)
            hashes.push_back(flipBits(base, 2, state));
    }
    for (int i = 0; i < 800; ++i)
        hashes.push_back(nextRandom(state));

    GroupingStats stats;
    groupNearDuplicates(hashes, 3, &stats);

    QVERIFY2(stats.comparisons * 10 < stats.naiveComparisons,
             qPrintable(QString("evaluated %1 of %2 possible pairs")
                            .arg(stats.comparisons).arg(stats.naiveComparisons)));
}

void TestDuplicateGrouper::everyPairWithinThresholdSharesABand() {
    // The pigeonhole claim itself, stated directly: at most 7 differing bits
    // cannot reach into all 8 bands, so at least one band must match exactly.
    uint64_t state = 777;
    for (int trial = 0; trial < 2000; ++trial) {
        const uint64_t a = nextRandom(state);
        const int bits = static_cast<int>(nextRandom(state) % (kMaxDistanceThreshold + 1));
        const uint64_t b = flipBits(a, bits, state);

        bool shared = false;
        for (int band = 0; band < kHashBands; ++band) {
            if (CandidateIndex::bandValue(a, band) == CandidateIndex::bandValue(b, band)) {
                shared = true;
                break;
            }
        }
        QVERIFY2(shared, qPrintable(QString("no shared band at distance %1")
                                        .arg(hammingDistance(a, b))));
    }
}

QTEST_APPLESS_MAIN(TestDuplicateGrouper)
#include "tst_duplicategrouper.moc"
