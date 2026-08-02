#include "core/Downscale.h"
#include "core/Hamming.h"
#include "core/PerceptualHash.h"

#include "support/ImageFixtures.h"

#include <QtTest>

using namespace winnow;
using namespace winnow::fixtures;

class TestPerceptualHash : public QObject {
    Q_OBJECT

private slots:
    void emptyImageHashesToZero();
    void flatImageHashesToZero();
    void structuredImageProducesANonZeroHash();
    void identicalImagesHashIdentically();
    void uniformBrightnessShiftLeavesTheHashUntouched();
    void clippedHighlightsStayCloserThanADifferentPicture();
    void sensorNoiseMovesTheHashByAtMostAFewBits();
    void downscaleAveragesRatherThanSampling();
    void downscaleHandlesTargetsLargerThanTheSource();
};

void TestPerceptualHash::emptyImageHashesToZero() {
    QCOMPARE(differenceHash(GrayImage{}), uint64_t{0});
}

void TestPerceptualHash::flatImageHashesToZero() {
    // Every neighbour is equal, so no comparison is ever "brighter than".
    QCOMPARE(differenceHash(solid(64, 64, 128)), uint64_t{0});
}

void TestPerceptualHash::structuredImageProducesANonZeroHash() {
    // Guards against the whole suite passing because everything hashes to zero.
    QVERIFY(differenceHash(scene(64, 64)) != 0);
}

void TestPerceptualHash::identicalImagesHashIdentically() {
    const GrayImage a = scene(64, 64);
    const GrayImage b = scene(64, 64);
    QCOMPARE(differenceHash(a), differenceHash(b));
}

void TestPerceptualHash::uniformBrightnessShiftLeavesTheHashUntouched() {
    // This is the property that makes dHash the right choice, and it is exact
    // rather than approximate: adding a constant to every pixel adds the same
    // constant to every downscaled cell average, and "a + c > b + c" is true in
    // precisely the cases where "a > b" was. Not one bit of the hash can move.
    //
    // An average hash -- comparing each cell against the image mean -- has no
    // such guarantee, which is the reason this project does not use one.
    const GrayImage original = scene(64, 64);
    const GrayImage exposed = brighter(original, 30); // peaks at 250, no clipping
    QCOMPARE(differenceHash(original), differenceHash(exposed));
}

void TestPerceptualHash::clippedHighlightsStayCloserThanADifferentPicture() {
    // Brighten far enough and the highlights clip to 255 and flatten, so some
    // comparisons genuinely do change. The hash should still be much nearer to
    // the original than an unrelated photograph is.
    const GrayImage original = scene(64, 64);
    const GrayImage blownOut = brighter(original, 120);
    const GrayImage different = scene(64, 64, 2);

    const int drift = hammingDistance(differenceHash(original), differenceHash(blownOut));
    const int gap = hammingDistance(differenceHash(original), differenceHash(different));

    QVERIFY2(drift < gap,
             qPrintable(QString("clipped drift %1 should be below the %2 bit gap to a different scene")
                            .arg(drift).arg(gap)));
}

void TestPerceptualHash::sensorNoiseMovesTheHashByAtMostAFewBits() {
    // Averaging during the downscale is what absorbs this: each of the 72 cells
    // is the mean of hundreds of source pixels, so zero-mean noise largely
    // cancels before any comparison happens.
    const GrayImage original = scene(128, 128);
    const GrayImage noisy = withNoise(original, 12);

    const int distance = hammingDistance(differenceHash(original), differenceHash(noisy));
    QVERIFY2(distance <= 4, qPrintable(QString("noise moved the hash %1 bits").arg(distance)));
}

void TestPerceptualHash::downscaleAveragesRatherThanSampling() {
    GrayImage source;
    source.resizeTo(2, 2);
    source.pixels = {0, 100, 200, 255};

    const GrayImage single = downscaleBox(source, 1, 1);
    QCOMPARE(single.width, 1);
    QCOMPARE(single.height, 1);
    // Nearest-neighbour sampling would return one of the four corner values.
    QCOMPARE(static_cast<int>(single.pixels[0]), (0 + 100 + 200 + 255) / 4);
}

void TestPerceptualHash::downscaleHandlesTargetsLargerThanTheSource() {
    // A thumbnail smaller than the 9x8 hash grid must not divide by zero or
    // produce an empty region.
    const GrayImage tiny = solid(3, 2, 77);
    const GrayImage grid = downscaleBox(tiny, kHashGridWidth, kHashGridHeight);

    QCOMPARE(grid.width, kHashGridWidth);
    QCOMPARE(grid.height, kHashGridHeight);
    for (uint8_t pixel : grid.pixels)
        QCOMPARE(static_cast<int>(pixel), 77);
}

QTEST_APPLESS_MAIN(TestPerceptualHash)
#include "tst_perceptualhash.moc"
