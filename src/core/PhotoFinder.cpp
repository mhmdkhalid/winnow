#include "core/PhotoFinder.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace winnow {
namespace {

constexpr std::array<const char*, 6> kExtensions = {
    ".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif"
};

std::string lowered(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

bool hasPhotoExtension(const std::string& path) {
    const std::string lower = lowered(path);
    for (const char* extension : kExtensions) {
        const size_t length = std::char_traits<char>::length(extension);
        if (lower.size() >= length && lower.compare(lower.size() - length, length, extension) == 0)
            return true;
    }
    return false;
}

std::vector<std::string> findPhotos(const std::string& root) {
    std::vector<std::string> found;

    std::error_code error;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
    if (error)
        return found;

    const fs::recursive_directory_iterator end;
    while (it != end) {
        // The error_code overload of increment() reports failures instead of
        // throwing, so one bad entry does not abandon the rest of the walk.
        const fs::directory_entry entry = *it;

        std::error_code entryError;
        if (entry.is_regular_file(entryError) && !entryError) {
            const std::string path = entry.path().string();
            if (hasPhotoExtension(path))
                found.push_back(path);
        }

        it.increment(error);
        if (error)
            break;
    }

    // A stable order makes scans reproducible, which matters for the tests and
    // for the screenshots in the README.
    std::sort(found.begin(), found.end());
    return found;
}

} // namespace winnow
