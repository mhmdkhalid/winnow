#include "core/ExifDate.h"

#include <cstdio>
#include <cstring>

namespace winnow {
namespace {

// EXIF tag numbers, from the specification.
constexpr uint16_t kTagDateTime = 0x0132;         // in IFD0, "file changed" time
constexpr uint16_t kTagExifIfdPointer = 0x8769;   // in IFD0, offset of the Exif sub-IFD
constexpr uint16_t kTagDateTimeOriginal = 0x9003; // in Exif IFD, the shutter moment
constexpr uint16_t kTagDateTimeDigitized = 0x9004;

constexpr uint16_t kTypeAscii = 2;
constexpr size_t kExifDateLength = 19; // "YYYY:MM:DD HH:MM:SS"
constexpr size_t kIfdEntrySize = 12;

// A bounds-checked cursor over the TIFF block inside the EXIF segment.
//
// Every offset inside a TIFF is relative to the start of that block, not to the
// start of the file, which is the detail that trips people up when reading EXIF
// by hand. All reads go through this so a truncated file cannot walk off the end.
class TiffReader {
public:
    TiffReader(const uint8_t* data, size_t size, bool littleEndian)
        : data_(data), size_(size), little_(littleEndian) {}

    bool read16(size_t offset, uint16_t& out) const {
        if (offset + 2 > size_)
            return false;
        out = little_ ? static_cast<uint16_t>(data_[offset] | (data_[offset + 1] << 8))
                      : static_cast<uint16_t>((data_[offset] << 8) | data_[offset + 1]);
        return true;
    }

    bool read32(size_t offset, uint32_t& out) const {
        if (offset + 4 > size_)
            return false;
        if (little_) {
            out = static_cast<uint32_t>(data_[offset])
                | (static_cast<uint32_t>(data_[offset + 1]) << 8)
                | (static_cast<uint32_t>(data_[offset + 2]) << 16)
                | (static_cast<uint32_t>(data_[offset + 3]) << 24);
        } else {
            out = (static_cast<uint32_t>(data_[offset]) << 24)
                | (static_cast<uint32_t>(data_[offset + 1]) << 16)
                | (static_cast<uint32_t>(data_[offset + 2]) << 8)
                | static_cast<uint32_t>(data_[offset + 3]);
        }
        return true;
    }

    bool hasRange(size_t offset, size_t length) const {
        return offset <= size_ && length <= size_ - offset;
    }

    const uint8_t* at(size_t offset) const { return data_ + offset; }

private:
    const uint8_t* data_;
    size_t size_;
    bool little_;
};

// "YYYY:MM:DD HH:MM:SS" -> DateTaken. Cameras write all-zero or all-space
// strings when the clock was never set; those are rejected as not-a-date.
std::optional<DateTaken> parseExifTimestamp(const uint8_t* text) {
    DateTaken when;
    const int matched = std::sscanf(reinterpret_cast<const char*>(text),
                                    "%4d:%2d:%2d %2d:%2d:%2d",
                                    &when.year, &when.month, &when.day,
                                    &when.hour, &when.minute, &when.second);
    if (matched != 6)
        return std::nullopt;
    if (when.year < 1826 || when.month < 1 || when.month > 12 || when.day < 1 || when.day > 31)
        return std::nullopt; // 1826 is the oldest surviving photograph; anything before is corruption
    if (when.hour < 0 || when.hour > 23 || when.minute < 0 || when.minute > 59
        || when.second < 0 || when.second > 60)
        return std::nullopt;
    return when;
}

// Read an ASCII tag whose value is stored out-of-line (any string longer than
// four bytes is, which every timestamp is).
std::optional<DateTaken> readAsciiDate(const TiffReader& tiff, uint16_t type,
                                       uint32_t count, uint32_t valueOffset) {
    if (type != kTypeAscii || count < kExifDateLength)
        return std::nullopt;
    if (!tiff.hasRange(valueOffset, kExifDateLength))
        return std::nullopt;
    return parseExifTimestamp(tiff.at(valueOffset));
}

struct IfdScan {
    std::optional<DateTaken> date;
    uint32_t exifIfdOffset = 0;
};

// Walk one image file directory. An IFD is a 2-byte entry count followed by
// that many 12-byte entries: tag, type, value count, then either the value
// itself or an offset to it.
IfdScan scanIfd(const TiffReader& tiff, uint32_t ifdOffset, bool lookForOriginal) {
    IfdScan result;

    uint16_t entryCount = 0;
    if (!tiff.read16(ifdOffset, entryCount))
        return result;

    // A directory claiming more entries than the block could hold is corrupt.
    if (!tiff.hasRange(ifdOffset + 2, static_cast<size_t>(entryCount) * kIfdEntrySize))
        return result;

    std::optional<DateTaken> digitized;

    for (uint16_t i = 0; i < entryCount; ++i) {
        const size_t entry = ifdOffset + 2 + static_cast<size_t>(i) * kIfdEntrySize;

        uint16_t tag = 0;
        uint16_t type = 0;
        uint32_t count = 0;
        uint32_t value = 0;
        if (!tiff.read16(entry, tag) || !tiff.read16(entry + 2, type)
            || !tiff.read32(entry + 4, count) || !tiff.read32(entry + 8, value))
            break;

        if (tag == kTagExifIfdPointer) {
            result.exifIfdOffset = value;
        } else if (lookForOriginal && tag == kTagDateTimeOriginal) {
            if (auto when = readAsciiDate(tiff, type, count, value))
                result.date = when; // strongest signal, stop caring about the rest
        } else if (lookForOriginal && tag == kTagDateTimeDigitized) {
            digitized = readAsciiDate(tiff, type, count, value);
        } else if (!lookForOriginal && tag == kTagDateTime) {
            result.date = readAsciiDate(tiff, type, count, value);
        }
    }

    if (!result.date)
        result.date = digitized;
    return result;
}

std::optional<DateTaken> parseExifBlock(const uint8_t* exif, size_t size) {
    // TIFF header: byte-order mark, magic number 42, offset of the first IFD.
    if (size < 8)
        return std::nullopt;

    bool little = false;
    if (exif[0] == 'I' && exif[1] == 'I')
        little = true;
    else if (exif[0] == 'M' && exif[1] == 'M')
        little = false;
    else
        return std::nullopt;

    const TiffReader tiff(exif, size, little);

    uint16_t magic = 0;
    if (!tiff.read16(2, magic) || magic != 42)
        return std::nullopt;

    uint32_t ifd0 = 0;
    if (!tiff.read32(4, ifd0) || ifd0 >= size)
        return std::nullopt;

    const IfdScan root = scanIfd(tiff, ifd0, /*lookForOriginal=*/false);

    // Follow the pointer to the Exif sub-directory exactly once. Not recursing
    // is what makes a malicious file unable to send this into a cycle.
    if (root.exifIfdOffset != 0 && root.exifIfdOffset < size) {
        const IfdScan sub = scanIfd(tiff, root.exifIfdOffset, /*lookForOriginal=*/true);
        if (sub.date)
            return sub.date;
    }

    return root.date; // fall back to IFD0's DateTime
}

} // namespace

std::optional<DateTaken> readDateTakenFromJpeg(const uint8_t* data, size_t size) {
    if (!data || size < 4)
        return std::nullopt;
    if (data[0] != 0xFF || data[1] != 0xD8) // SOI: not a JPEG at all
        return std::nullopt;

    size_t pos = 2;
    while (pos + 4 <= size) {
        if (data[pos] != 0xFF) // out of step with the segment chain
            return std::nullopt;

        // Any number of 0xFF bytes may pad the gap before a marker.
        size_t markerPos = pos;
        while (markerPos < size && data[markerPos] == 0xFF)
            ++markerPos;
        if (markerPos >= size)
            return std::nullopt;

        const uint8_t marker = data[markerPos];

        // Start of scan / end of image: past here is compressed pixel data, and
        // metadata never appears after it.
        if (marker == 0xDA || marker == 0xD9)
            return std::nullopt;

        // Standalone markers carry no length field.
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            pos = markerPos + 1;
            continue;
        }

        if (markerPos + 3 > size)
            return std::nullopt;

        const size_t length = (static_cast<size_t>(data[markerPos + 1]) << 8)
                            | static_cast<size_t>(data[markerPos + 2]);
        if (length < 2)
            return std::nullopt; // the length includes its own two bytes

        const size_t payload = markerPos + 3;
        const size_t payloadLength = length - 2;
        if (payload + payloadLength > size)
            return std::nullopt; // truncated file

        // APP1 holding "Exif\0\0" is the one segment worth opening.
        if (marker == 0xE1 && payloadLength > 6
            && std::memcmp(data + payload, "Exif\0\0", 6) == 0) {
            return parseExifBlock(data + payload + 6, payloadLength - 6);
        }

        pos = payload + payloadLength;
    }

    return std::nullopt;
}

std::string formatDateTaken(const DateTaken& when) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                  when.year, when.month, when.day, when.hour, when.minute, when.second);
    return std::string(buffer);
}

} // namespace winnow
