#include "core/LibraryScanner.h"

#include "support/ImageFixtures.h"

#include <QtTest>

#include <map>

using namespace winnow;
using namespace winnow::fixtures;

namespace {

// A decoder with no codec, no files and no Qt behind it.
//
// This is the payoff of PhotoDecoder being an interface declared in core/. The
// entire scanning pipeline -- threading, hashing, scoring, grouping -- is
// exercised against images built in memory, so the suite needs no photographs
// checked into the repository and runs in milliseconds. It also allows a
// failure to be injected on demand, which is awkward to arrange with real files.
class FakeDecoder : public PhotoDecoder {
public:
    void add(const std::string& path, const GrayImage& image,
             std::optional<DateTaken> when = std::nullopt) {
        DecodedPhoto decoded;
        decoded.gray = image;
        decoded.width = image.width;
        decoded.height = image.height;
        decoded.dateTaken = when;
        byPath_[path] = std::move(decoded);
    }

    // Anything not registered behaves like an unreadable or corrupt file.
    std::optional<DecodedPhoto> decode(const std::string& path) const override {
        const auto it = byPath_.find(path);
        if (it == byPath_.end())
            return std::nullopt;
        return it->second;
    }

    std::vector<std::string> paths() const {
        std::vector<std::string> out;
        for (const auto& entry : byPath_)
            out.push_back(entry.first);
        return out;
    }

private:
    std::map<std::string, DecodedPhoto> byPath_;
};

} // namespace

class TestLibraryScanner : public QObject {
    Q_OBJECT

private slots:
    void groupsVariationsOfTheSameShot();
    void keepsUnrelatedPhotographsApart();
    void filesThatFailToDecodeAreCountedAndExcluded();
    void progressIsReportedOncePerFile();
    void cancellationStopsBeforeGrouping();
    void bestInGroupPrefersTheSharperFrame();
    void bestInGroupBreaksTiesOnResolution();
};

void TestLibraryScanner::groupsVariationsOfTheSameShot() {
    // Three frames of one moment: the original, a noisier one, and a softer one.
    FakeDecoder decoder;
    const GrayImage original = scene(128, 128);
    decoder.add("burst_1.jpg", original);
    decoder.add("burst_2.jpg", withNoise(original, 10, 7));
    decoder.add("burst_3.jpg", blurred(original, 1));

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);
    const ScanResult result = scanner.scan(decoder.paths(), 5);

    QCOMPARE(result.failed, size_t{0});
    QCOMPARE(result.groups.size(), size_t{1});
    QCOMPARE(result.groups[0].size(), size_t{3});
}

void TestLibraryScanner::keepsUnrelatedPhotographsApart() {
    FakeDecoder decoder;
    decoder.add("a.jpg", scene(128, 128, 0));
    decoder.add("b.jpg", scene(128, 128, 3));

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);
    const ScanResult result = scanner.scan(decoder.paths(), 3);

    QVERIFY(result.groups.empty());
}

void TestLibraryScanner::filesThatFailToDecodeAreCountedAndExcluded() {
    // A broken file has no hash. Left in, several of them would share the value
    // 0 and be reported as duplicates of each other -- a confident, entirely
    // wrong answer. They are kept out of grouping instead.
    FakeDecoder decoder;
    decoder.add("good.jpg", scene(64, 64));

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);
    const ScanResult result = scanner.scan({"good.jpg", "broken_1.jpg", "broken_2.jpg"}, 3);

    QCOMPARE(result.failed, size_t{2});
    QCOMPARE(result.photos.size(), size_t{3});
    QVERIFY(result.groups.empty());

    // The failures still appear in the results, flagged, rather than silently
    // disappearing from the report.
    size_t undecoded = 0;
    for (const Photo& photo : result.photos) {
        if (!photo.decoded)
            ++undecoded;
    }
    QCOMPARE(undecoded, size_t{2});
}

void TestLibraryScanner::progressIsReportedOncePerFile() {
    FakeDecoder decoder;
    for (int i = 0; i < 50; ++i)
        decoder.add("photo_" + std::to_string(i) + ".jpg", scene(32, 32, i % 4));

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);

    std::vector<size_t> reported;
    std::mutex mutex;
    scanner.scan(decoder.paths(), 3, [&](size_t done, size_t total) {
        std::lock_guard<std::mutex> lock(mutex);
        QCOMPARE(total, size_t{50});
        reported.push_back(done);
    });

    QCOMPARE(reported.size(), size_t{50});

    // Reports arrive from several threads, so they need not be ordered -- but
    // between them they must cover 1..50 exactly once.
    std::sort(reported.begin(), reported.end());
    for (size_t i = 0; i < reported.size(); ++i)
        QCOMPARE(reported[i], i + 1);
}

void TestLibraryScanner::cancellationStopsBeforeGrouping() {
    FakeDecoder decoder;
    const GrayImage original = scene(64, 64);
    decoder.add("a.jpg", original);
    decoder.add("b.jpg", original);

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);

    std::atomic<bool> cancelled{true};
    const ScanResult result = scanner.scan(decoder.paths(), 3, {}, &cancelled);

    // Identical images would certainly have grouped; returning nothing proves
    // the flag was honoured rather than the work merely finishing quickly.
    QVERIFY(result.groups.empty());
}

void TestLibraryScanner::bestInGroupPrefersTheSharperFrame() {
    FakeDecoder decoder;
    const GrayImage sharp = scene(128, 128);
    decoder.add("a_sharp.jpg", sharp);
    decoder.add("b_soft.jpg", blurred(sharp, 3));

    ThreadPool pool(4);
    const LibraryScanner scanner(decoder, pool);
    const ScanResult result = scanner.scan(decoder.paths(), 6);

    QCOMPARE(result.groups.size(), size_t{1});
    const std::vector<size_t>& group = result.groups[0];
    const size_t keeper = group[result.bestInGroup(group)];
    QCOMPARE(QString::fromStdString(result.photos[keeper].path), QString("a_sharp.jpg"));
}

void TestLibraryScanner::bestInGroupBreaksTiesOnResolution() {
    // Two copies of one photograph at different sizes score identically for
    // sharpness after the downscale, so the larger file wins.
    ScanResult result;
    Photo small;
    small.path = "small.jpg";
    small.sharpness = 100.0;
    small.width = 800;
    small.height = 600;

    Photo large = small;
    large.path = "large.jpg";
    large.width = 4000;
    large.height = 3000;

    result.photos = {small, large};
    QCOMPARE(result.bestInGroup({0, 1}), size_t{1});
}

QTEST_APPLESS_MAIN(TestLibraryScanner)
#include "tst_libraryscanner.moc"
