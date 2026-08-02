#include "imaging/QtPhotoDecoder.h"

#include "support/ImageFixtures.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

using namespace winnow;
using namespace winnow::fixtures;

namespace {

// Splice a hand-built EXIF segment into a JPEG that Qt actually encoded.
//
// The fixture builder produces a structurally valid EXIF block but no
// compressed pixel data, so a decoder cannot open it. Qt's encoder produces
// real pixel data but will not write a DateTimeOriginal. Joining them -- our
// APP1 segment inserted directly after the start-of-image marker, which is
// exactly where a camera puts it -- gives a file that is both decodable and
// carries a known timestamp, so the two halves of the decoder can be checked at
// once against a genuine JPEG.
QByteArray jpegWithSplicedExif(const QImage& image, int year, int month, int day) {
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", 95))
        return {};
    buffer.close();

    const std::vector<uint8_t> wrapper = ExifJpegBuilder().build(year, month, day, 12, 30, 0);
    // Strip the wrapper's own SOI (2 bytes) and EOI (2 bytes) to leave the bare
    // APP1 segment.
    const QByteArray app1(reinterpret_cast<const char*>(wrapper.data()) + 2,
                          static_cast<int>(wrapper.size()) - 4);

    QByteArray out;
    out.append(encoded.left(2)); // SOI
    out.append(app1);
    out.append(encoded.mid(2));
    return out;
}

QImage colourfulTestImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, qRgb((x * 255) / width, (y * 255) / height, 128));
    }
    return image;
}

} // namespace

class TestQtPhotoDecoder : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void decodesAPngFromDisk();
    void reportsOriginalDimensionsDespiteDecodingSmaller();
    void limitsTheDecodedImageToTheWorkingSize();
    void readsTheExifDateOutOfARealJpeg();
    void missingFileFails();
    void garbageContentFails();
    void imageWithoutMetadataDecodesWithNoDate();

private:
    QTemporaryDir dir_;
    QString pathFor(const QString& name) const { return dir_.filePath(name); }
};

void TestQtPhotoDecoder::initTestCase() {
    QVERIFY2(dir_.isValid(), "could not create a temporary directory");
}

void TestQtPhotoDecoder::decodesAPngFromDisk() {
    const QString path = pathFor("plain.png");
    QVERIFY(colourfulTestImage(120, 80).save(path));

    const QtPhotoDecoder decoder;
    const auto decoded = decoder.decode(path.toStdString());

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->gray.width, 120);
    QCOMPARE(decoded->gray.height, 80);
    QCOMPARE(decoded->gray.pixels.size(), size_t{120 * 80});
    QVERIFY(!decoded->dateTaken.has_value()); // PNGs carry no EXIF here
}

void TestQtPhotoDecoder::reportsOriginalDimensionsDespiteDecodingSmaller() {
    // The reported size has to stay the real one: it is what breaks ties
    // between two copies of the same photograph at different resolutions.
    const QString path = pathFor("large.png");
    QVERIFY(colourfulTestImage(900, 600).save(path));

    const QtPhotoDecoder decoder;
    const auto decoded = decoder.decode(path.toStdString());

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->width, 900);
    QCOMPARE(decoded->height, 600);
}

void TestQtPhotoDecoder::limitsTheDecodedImageToTheWorkingSize() {
    // Proof the scaled-decode path is actually taken rather than silently
    // decoding at full resolution -- the difference is most of the runtime of a
    // scan.
    const QString path = pathFor("huge.png");
    QVERIFY(colourfulTestImage(1600, 1200).save(path));

    const QtPhotoDecoder decoder;
    const auto decoded = decoder.decode(path.toStdString());

    QVERIFY(decoded.has_value());
    QVERIFY2(decoded->gray.width <= kDecodeLongestSide && decoded->gray.height <= kDecodeLongestSide,
             qPrintable(QString("decoded at %1x%2, expected no larger than %3")
                            .arg(decoded->gray.width).arg(decoded->gray.height)
                            .arg(kDecodeLongestSide)));
    // Aspect ratio preserved: a squashed image would hash differently.
    QCOMPARE(decoded->gray.width > decoded->gray.height, true);
}

void TestQtPhotoDecoder::readsTheExifDateOutOfARealJpeg() {
    const QByteArray jpeg = jpegWithSplicedExif(colourfulTestImage(200, 150), 2026, 5, 9);
    QVERIFY(!jpeg.isEmpty());

    const QString path = pathFor("holiday.jpg");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(jpeg), static_cast<qint64>(jpeg.size()));
    file.close();

    const QtPhotoDecoder decoder;
    const auto decoded = decoder.decode(path.toStdString());

    QVERIFY(decoded.has_value());
    QVERIFY(!decoded->gray.empty());          // the pixels still decode
    QVERIFY(decoded->dateTaken.has_value());  // and the metadata came through
    QCOMPARE(decoded->dateTaken->year, 2026);
    QCOMPARE(decoded->dateTaken->month, 5);
    QCOMPARE(decoded->dateTaken->day, 9);
}

void TestQtPhotoDecoder::missingFileFails() {
    const QtPhotoDecoder decoder;
    QVERIFY(!decoder.decode(pathFor("does_not_exist.jpg").toStdString()).has_value());
}

void TestQtPhotoDecoder::garbageContentFails() {
    // A file with a photo extension whose contents are not an image at all.
    // This must fail cleanly, because a scan of a real folder will meet one.
    const QString path = pathFor("corrupt.jpg");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is definitely not a JPEG");
    file.close();

    const QtPhotoDecoder decoder;
    QVERIFY(!decoder.decode(path.toStdString()).has_value());
}

void TestQtPhotoDecoder::imageWithoutMetadataDecodesWithNoDate() {
    const QString path = pathFor("stripped.jpg");
    QVERIFY(colourfulTestImage(100, 100).save(path, "JPEG", 90));

    const QtPhotoDecoder decoder;
    const auto decoded = decoder.decode(path.toStdString());

    // Missing metadata is normal, not an error: it means "file under Undated",
    // not "this photo is broken".
    QVERIFY(decoded.has_value());
    QVERIFY(!decoded->gray.empty());
    QVERIFY(!decoded->dateTaken.has_value());
}

QTEST_GUILESS_MAIN(TestQtPhotoDecoder)
#include "tst_qtphotodecoder.moc"
