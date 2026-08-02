#include "imaging/QtPhotoDecoder.h"

#include "core/ExifDate.h"

#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QSize>
#include <QString>

#include <cstring>

namespace winnow {

std::optional<DecodedPhoto> QtPhotoDecoder::decode(const std::string& path) const {
    const QString filePath = QString::fromStdString(path);

    DecodedPhoto out;

    // --- metadata -----------------------------------------------------------
    // Read only the opening chunk. Everything the timestamp needs lives there.
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray head = file.read(kExifScanBytes);
            out.dateTaken = readDateTakenFromJpeg(
                reinterpret_cast<const uint8_t*>(head.constData()),
                static_cast<size_t>(head.size()));
        }
        // A missing date is not a failure: PNGs have none, and plenty of JPEGs
        // have had their metadata stripped by messaging apps. Only an
        // undecodable image counts as a failure.
    }

    // --- pixels -------------------------------------------------------------
    QImageReader reader(filePath);

    // Cameras record orientation as a metadata flag rather than by rotating the
    // stored pixels. Without applying it, a portrait photograph and the same
    // photograph after rotation would produce completely different hashes and
    // never be recognised as the same picture.
    reader.setAutoTransform(true);

    const QSize fullSize = reader.size();
    if (fullSize.isValid()) {
        out.width = fullSize.width();
        out.height = fullSize.height();

        // Ask the codec for a smaller image rather than shrinking afterwards.
        if (fullSize.width() > kDecodeLongestSide || fullSize.height() > kDecodeLongestSide) {
            reader.setScaledSize(fullSize.scaled(kDecodeLongestSide, kDecodeLongestSide,
                                                 Qt::KeepAspectRatio));
        }
    }

    const QImage image = reader.read();
    if (image.isNull())
        return std::nullopt; // unreadable, unsupported, or not an image at all

    if (!fullSize.isValid()) {
        // Some formats will not report a size before decoding.
        out.width = image.width();
        out.height = image.height();
    }

    const QImage grey = image.convertToFormat(QImage::Format_Grayscale8);
    if (grey.isNull() || grey.width() <= 0 || grey.height() <= 0)
        return std::nullopt;

    out.gray.resizeTo(grey.width(), grey.height());
    for (int y = 0; y < grey.height(); ++y) {
        // Qt pads each scanline to a 4-byte boundary, so rows must be copied
        // one at a time rather than as one contiguous block.
        std::memcpy(out.gray.pixels.data() + static_cast<size_t>(y) * grey.width(),
                    grey.constScanLine(y),
                    static_cast<size_t>(grey.width()));
    }

    return out;
}

} // namespace winnow
