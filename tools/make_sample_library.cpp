// Generates a folder of synthetic photographs to demonstrate and test against.
//
// Real photo libraries cannot be committed to a repository -- they are large,
// and they are usually somebody's actual holiday. This produces a stand-in with
// a known answer: a set of "moments", each captured as several near-identical
// frames the way a burst or a few retries of the same shot would be, plus
// unrelated photographs that must not be grouped with anything.
//
// Because the content is generated, the expected result is known in advance,
// which is what makes the README screenshot reproducible rather than a lucky
// capture of somebody's camera roll.

#include "support/ImageFixtures.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <cmath>
#include <vector>

using namespace winnow::fixtures;

namespace {

uint32_t nextRandom(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

// A synthetic "photograph": bands of colour, shapes at several sizes, and an
// overall texture. Different seeds give visibly different pictures.
//
// The texture is not decoration. A perceptual hash asks "is this region
// brighter than the one beside it" 64 times, and across a large flat area the
// two answers being compared are nearly equal, so the bit is decided by
// whatever rounding falls out -- a near-tie that the smallest change can flip.
// Real photographs have detail everywhere and almost never produce those ties.
// A generator that emitted flat colour fields would therefore make the matching
// look far less reliable than it is on real input, so it models the texture.
QImage makeScene(uint32_t seed, int width = 900, int height = 675) {
    uint32_t state = seed * 2654435761u + 1u;

    QImage image(width, height, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor sky(60 + nextRandom(state) % 120, 90 + nextRandom(state) % 120,
                     150 + nextRandom(state) % 100);
    const QColor ground(40 + nextRandom(state) % 90, 90 + nextRandom(state) % 110,
                        40 + nextRandom(state) % 70);

    painter.fillRect(0, 0, width, height * 3 / 5, sky);
    painter.fillRect(0, height * 3 / 5, width, height, ground);

    // Large subjects, then progressively smaller detail on top.
    const int shapes = 14 + static_cast<int>(nextRandom(state) % 10);
    for (int i = 0; i < shapes; ++i) {
        const QColor colour(nextRandom(state) % 256, nextRandom(state) % 256,
                            nextRandom(state) % 256);
        painter.setBrush(colour);
        painter.setPen(Qt::NoPen);

        const int x = static_cast<int>(nextRandom(state) % width);
        const int y = static_cast<int>(nextRandom(state) % height);
        const int size = (i < 4 ? 120 : 20) + static_cast<int>(nextRandom(state) % (i < 4 ? 200 : 90));

        if (nextRandom(state) % 2)
            painter.drawEllipse(QPoint(x, y), size / 2, size / 2);
        else
            painter.drawRect(x, y, size, size * 2 / 3);
    }
    painter.end();

    // A smooth, spatially varying modulation, standing in for the fact that no
    // real scene is evenly lit. This is what stops neighbouring hash cells from
    // landing on identical averages.
    const uint32_t phase = seed * 7919u;
    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const double wave =
                18.0 * std::sin((x + phase % 97) / 55.0) * std::cos((y + phase % 53) / 47.0)
                + 10.0 * std::sin((x + y) / 23.0);
            const int delta = static_cast<int>(wave);
            line[x] = qRgb(qBound(0, qRed(line[x]) + delta, 255),
                           qBound(0, qGreen(line[x]) + delta, 255),
                           qBound(0, qBlue(line[x]) + delta, 255));
        }
    }

    return image;
}

// What a second frame of the same moment looks like: everything shifted by a
// pixel or two, a little sensor noise, and sometimes slightly out of focus.
QImage makeVariation(const QImage& original, uint32_t seed, bool soften) {
    uint32_t state = seed * 40503u + 7u;

    QImage shifted(original.size(), original.format());
    shifted.fill(Qt::black);
    {
        QPainter painter(&shifted);
        const int dx = static_cast<int>(nextRandom(state) % 7) - 3;
        const int dy = static_cast<int>(nextRandom(state) % 7) - 3;
        painter.drawImage(dx, dy, original);
    }

    for (int y = 0; y < shifted.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(shifted.scanLine(y));
        for (int x = 0; x < shifted.width(); ++x) {
            const int delta = static_cast<int>(nextRandom(state) % 13) - 6;
            line[x] = qRgb(qBound(0, qRed(line[x]) + delta, 255),
                           qBound(0, qGreen(line[x]) + delta, 255),
                           qBound(0, qBlue(line[x]) + delta, 255));
        }
    }

    if (!soften)
        return shifted;

    // A cheap box blur, standing in for a frame that missed focus.
    QImage blurredImage = shifted;
    for (int y = 1; y < shifted.height() - 1; ++y) {
        auto* out = reinterpret_cast<QRgb*>(blurredImage.scanLine(y));
        const auto* above = reinterpret_cast<const QRgb*>(shifted.constScanLine(y - 1));
        const auto* here = reinterpret_cast<const QRgb*>(shifted.constScanLine(y));
        const auto* below = reinterpret_cast<const QRgb*>(shifted.constScanLine(y + 1));
        for (int x = 1; x < shifted.width() - 1; ++x) {
            const int r = (qRed(above[x]) + qRed(below[x]) + qRed(here[x - 1])
                           + qRed(here[x + 1]) + qRed(here[x])) / 5;
            const int g = (qGreen(above[x]) + qGreen(below[x]) + qGreen(here[x - 1])
                           + qGreen(here[x + 1]) + qGreen(here[x])) / 5;
            const int b = (qBlue(above[x]) + qBlue(below[x]) + qBlue(here[x - 1])
                           + qBlue(here[x + 1]) + qBlue(here[x])) / 5;
            out[x] = qRgb(r, g, b);
        }
    }
    return blurredImage;
}

// Encode as JPEG, then splice in an EXIF segment carrying a chosen capture
// date. Qt's encoder will not write one, and the whole "organise by date"
// feature needs real metadata to read.
bool saveWithExif(const QImage& image, const QString& path,
                  int year, int month, int day, int hour, int minute) {
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", 92))
        return false;
    buffer.close();

    const std::vector<uint8_t> wrapper =
        ExifJpegBuilder().build(year, month, day, hour, minute, 0);
    const QByteArray app1(reinterpret_cast<const char*>(wrapper.data()) + 2,
                          static_cast<int>(wrapper.size()) - 4);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(encoded.left(2)); // SOI
    file.write(app1);            // our EXIF block
    file.write(encoded.mid(2));  // the rest of the JPEG
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QString target = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("sample-library");
    QDir().mkpath(target);

    struct Moment {
        int frames;
        bool softenLast;
        int month;
        int day;
    };
    const std::vector<Moment> moments = {
        {4, true,  3, 14}, // a burst, last frame out of focus
        {3, true,  3, 15},
        {2, false, 5, 2},
        {5, true,  7, 21},
        {2, false, 9, 8},
        {3, false, 12, 24},
    };

    int written = 0;
    uint32_t seed = 1;

    for (size_t m = 0; m < moments.size(); ++m) {
        const Moment& moment = moments[m];
        const QImage original = makeScene(seed++);

        for (int frame = 0; frame < moment.frames; ++frame) {
            const bool soften = moment.softenLast && frame == moment.frames - 1;
            const QImage image = frame == 0 ? original
                                            : makeVariation(original, seed + frame, soften);

            const QString name = QString("IMG_%1_%2.jpg")
                                     .arg(m + 1, 2, 10, QChar('0'))
                                     .arg(frame + 1);
            if (saveWithExif(image, QDir(target).filePath(name),
                             2026, moment.month, moment.day, 10 + static_cast<int>(m), frame * 3))
                ++written;
        }
    }

    // Unrelated photographs. None of these should end up grouped with anything.
    for (int i = 0; i < 8; ++i) {
        const QString name = QString("DSC_%1.jpg").arg(100 + i);
        if (saveWithExif(makeScene(1000 + i), QDir(target).filePath(name),
                         2026, 1 + (i % 12), 1 + i, 9, i))
            ++written;
    }

    // One file with a photo extension that is not an image, so the failure path
    // is visible in a demo rather than only in the tests.
    QFile broken(QDir(target).filePath("DSC_0999.jpg"));
    if (broken.open(QIODevice::WriteOnly)) {
        broken.write("not actually a photograph");
        broken.close();
        ++written;
    }

    out << "Wrote " << written << " files to " << QDir(target).absolutePath() << "\n";
    return 0;
}
