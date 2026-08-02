#include "core/ExifDate.h"

#include "support/ImageFixtures.h"

#include <QtTest>

using namespace winnow;
using namespace winnow::fixtures;

namespace {

std::optional<DateTaken> read(const std::vector<uint8_t>& bytes) {
    return readDateTakenFromJpeg(bytes.data(), bytes.size());
}

} // namespace

class TestExifDate : public QObject {
    Q_OBJECT

private slots:
    void readsDateTakenFromALittleEndianFile();
    void readsDateTakenFromABigEndianFile();
    void jpegWithoutMetadataHasNoDate();
    void nonJpegInputIsRejected();
    void truncationAtEveryLengthIsSurvivable();
    void corruptedByteAtEveryOffsetIsSurvivable();
    void impossibleTimestampsAreRejected();
    void formatsForDisplay();
};

void TestExifDate::readsDateTakenFromALittleEndianFile() {
    const auto jpeg = ExifJpegBuilder(/*littleEndian=*/true).build(2026, 7, 14, 18, 5, 30);
    const auto when = read(jpeg);

    QVERIFY(when.has_value());
    QCOMPARE(when->year, 2026);
    QCOMPARE(when->month, 7);
    QCOMPARE(when->day, 14);
    QCOMPARE(when->hour, 18);
    QCOMPARE(when->minute, 5);
    QCOMPARE(when->second, 30);
}

void TestExifDate::readsDateTakenFromABigEndianFile() {
    // EXIF genuinely ships in both byte orders -- the "II"/"MM" mark at the top
    // of the TIFF block decides it, and camera makers differ. Reading only one
    // would silently fail on half the world's photographs.
    const auto jpeg = ExifJpegBuilder(/*littleEndian=*/false).build(1999, 12, 31, 23, 59, 59);
    const auto when = read(jpeg);

    QVERIFY(when.has_value());
    QCOMPARE(when->year, 1999);
    QCOMPARE(when->month, 12);
    QCOMPARE(when->day, 31);
    QCOMPARE(when->hour, 23);
    QCOMPARE(when->second, 59);
}

void TestExifDate::jpegWithoutMetadataHasNoDate() {
    QVERIFY(!read(jpegWithoutExif()).has_value());
}

void TestExifDate::nonJpegInputIsRejected() {
    QVERIFY(!read({}).has_value());
    QVERIFY(!read({0x00}).has_value());
    QVERIFY(!read({'P', 'N', 'G', 0x0D, 0x0A}).has_value());
    QVERIFY(!readDateTakenFromJpeg(nullptr, 100).has_value());
}

void TestExifDate::truncationAtEveryLengthIsSurvivable() {
    // A half-copied file, an interrupted download, a photo recovered off a
    // failing card: all of them arrive as a valid prefix and nothing more.
    //
    // The boundary is exact and worth pinning down. Everything the timestamp
    // needs sits in the APP1 segment, which the builder places immediately
    // after the start-of-image marker and follows with nothing but the two-byte
    // end-of-image marker. So a prefix that reaches the end of that segment
    // still holds a complete, readable date -- recovering metadata from a file
    // whose pixel data was lost is correct, not a bug -- and every shorter
    // prefix cuts into the structure and must yield nothing.
    const auto complete = ExifJpegBuilder().build(2026, 3, 1, 9, 0, 0);
    const size_t segmentEnd = complete.size() - 2; // everything bar the EOI marker

    for (size_t length = 0; length <= complete.size(); ++length) {
        const std::vector<uint8_t> truncated(complete.begin(), complete.begin() + length);
        const auto when = readDateTakenFromJpeg(truncated.data(), truncated.size());

        if (length < segmentEnd) {
            QVERIFY2(!when.has_value(),
                     qPrintable(QString("a %1-byte prefix cuts into the EXIF segment and "
                                        "must not yield a date").arg(length)));
        } else {
            QVERIFY2(when.has_value(),
                     qPrintable(QString("a %1-byte prefix contains the whole EXIF segment "
                                        "and should still parse").arg(length)));
            QCOMPARE(when->year, 2026);
        }
    }
}

void TestExifDate::corruptedByteAtEveryOffsetIsSurvivable() {
    // Flip one byte at a time through the whole file. Some corruptions will
    // still produce a date and some will not -- neither is wrong. What is being
    // asserted is that none of them crashes, hangs, or walks off the buffer,
    // which is the property that matters when parsing untrusted input. Run
    // under a sanitiser this is a genuine memory-safety check.
    const auto original = ExifJpegBuilder().build(2026, 3, 1, 9, 0, 0);

    for (size_t i = 0; i < original.size(); ++i) {
        std::vector<uint8_t> damaged = original;
        damaged[i] = 0xFF;
        readDateTakenFromJpeg(damaged.data(), damaged.size());

        damaged[i] = 0x00;
        readDateTakenFromJpeg(damaged.data(), damaged.size());
    }

    QVERIFY(true); // reaching here without a crash is the assertion
}

void TestExifDate::impossibleTimestampsAreRejected() {
    // Cameras whose clock was never set write zeroes. Filing those under the
    // year 0 would be worse than admitting the date is unknown.
    QVERIFY(!read(ExifJpegBuilder().build(0, 0, 0, 0, 0, 0)).has_value());
    QVERIFY(!read(ExifJpegBuilder().build(2026, 13, 1, 0, 0, 0)).has_value());
    QVERIFY(!read(ExifJpegBuilder().build(2026, 1, 32, 0, 0, 0)).has_value());
    QVERIFY(!read(ExifJpegBuilder().build(2026, 1, 1, 25, 0, 0)).has_value());
}

void TestExifDate::formatsForDisplay() {
    QCOMPARE(QString::fromStdString(formatDateTaken({2026, 7, 4, 8, 3, 9})),
             QString("2026-07-04 08:03:09"));
}

QTEST_APPLESS_MAIN(TestExifDate)
#include "tst_exifdate.moc"
