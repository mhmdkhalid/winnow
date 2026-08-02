#include "core/Sharpness.h"

#include "support/ImageFixtures.h"

#include <QtTest>

using namespace winnow;
using namespace winnow::fixtures;

class TestSharpness : public QObject {
    Q_OBJECT

private slots:
    void flatImageHasNoEdgesAndScoresZero();
    void blurringTheSameSceneLowersTheScore();
    void moreBlurLowersItFurther();
    void imagesTooSmallForTheKernelScoreZero();
    void emptyImageScoresZero();
};

void TestSharpness::flatImageHasNoEdgesAndScoresZero() {
    // Every Laplacian response is exactly 0, so their variance is 0.
    QCOMPARE(laplacianVariance(solid(32, 32, 128)), 0.0);
}

void TestSharpness::blurringTheSameSceneLowersTheScore() {
    // The comparison is only meaningful because both images show the same
    // subject -- which is exactly how the application uses it, ranking members
    // of one near-duplicate group against each other.
    const GrayImage sharp = scene(64, 64);
    const GrayImage soft = blurred(sharp, 2);

    QVERIFY2(laplacianVariance(sharp) > laplacianVariance(soft),
             qPrintable(QString("sharp %1 should beat blurred %2")
                            .arg(laplacianVariance(sharp)).arg(laplacianVariance(soft))));
}

void TestSharpness::moreBlurLowersItFurther() {
    const GrayImage sharp = scene(64, 64);
    QVERIFY(laplacianVariance(blurred(sharp, 1)) > laplacianVariance(blurred(sharp, 4)));
}

void TestSharpness::imagesTooSmallForTheKernelScoreZero() {
    // A 3x3 kernel needs a full ring of neighbours; a 2-pixel-wide image has no
    // interior pixel at all.
    QCOMPARE(laplacianVariance(solid(2, 2, 50)), 0.0);
    QCOMPARE(laplacianVariance(solid(1, 40, 50)), 0.0);
}

void TestSharpness::emptyImageScoresZero() {
    QCOMPARE(laplacianVariance(GrayImage{}), 0.0);
}

QTEST_APPLESS_MAIN(TestSharpness)
#include "tst_sharpness.moc"
