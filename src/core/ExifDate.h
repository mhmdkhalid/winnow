#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace winnow {

// The moment a photograph was taken, as recorded by the camera.
struct DateTaken {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool operator==(const DateTaken& other) const {
        return year == other.year && month == other.month && day == other.day
            && hour == other.hour && minute == other.minute && second == other.second;
    }
};

// Extract the capture time from a JPEG's embedded EXIF metadata.
//
// Why parse this by hand rather than trust the file's modification time: the
// modification time records when the *file* was last written, so copying a
// photo, restoring a backup or syncing a phone rewrites it. Sorting a library
// by it produces folders full of photographs that all appear to have been taken
// the day the drive was replaced. The EXIF timestamp is written by the camera
// at the moment of capture and is copied along with the bytes.
//
// The path through the file is: JPEG segment chain -> the APP1 segment marked
// "Exif\0\0" -> a complete little- or big-endian TIFF structure -> the IFD0
// directory -> a pointer to the Exif sub-directory -> tag 0x9003
// (DateTimeOriginal), an ASCII string "YYYY:MM:DD HH:MM:SS".
//
// This reads untrusted bytes from arbitrary files, so every single read is
// bounds-checked against the buffer and offsets are never followed blindly. A
// malformed or truncated file returns std::nullopt; it must not read out of
// bounds and must not loop.
std::optional<DateTaken> readDateTakenFromJpeg(const uint8_t* data, size_t size);

// Format as "YYYY-MM-DD HH:MM:SS", for display.
std::string formatDateTaken(const DateTaken& when);

} // namespace winnow
