#include "core/OrganizePlanner.h"

#include <cstdio>
#include <set>

namespace winnow {
namespace {

std::string trimTrailingSeparator(std::string path) {
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    return path;
}

// Split "photo.jpg" into "photo" and ".jpg". A leading dot is part of the name,
// not an extension, so ".hidden" stays whole.
void splitExtension(const std::string& name, std::string& stem, std::string& extension) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        stem = name;
        extension.clear();
    } else {
        stem = name.substr(0, dot);
        extension = name.substr(dot);
    }
}

std::string twoDigits(int value) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d", value);
    return std::string(buffer);
}

std::string fourDigits(int value) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%04d", value);
    return std::string(buffer);
}

} // namespace

std::string fileNameOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

OrganizePlan planOrganise(const std::vector<Photo>& photos,
                          const std::vector<size_t>& selected,
                          const std::string& destinationRoot,
                          const std::function<bool(const std::string&)>& exists) {
    OrganizePlan plan;
    const std::string root = trimTrailingSeparator(destinationRoot);

    // Two photos in the same batch can want the same destination even when
    // nothing is on disk yet, so names claimed earlier in this plan count as
    // taken too.
    std::set<std::string> claimed;

    for (size_t index : selected) {
        if (index >= photos.size())
            continue;

        const Photo& photo = photos[index];
        const std::string name = fileNameOf(photo.path);

        std::string folder;
        if (photo.dateTaken) {
            const std::string year = fourDigits(photo.dateTaken->year);
            folder = root + "/" + year + "/" + year + "-" + twoDigits(photo.dateTaken->month);
        } else {
            folder = root + "/Undated";
            ++plan.undated;
        }

        std::string stem;
        std::string extension;
        splitExtension(name, stem, extension);

        std::string destination = folder + "/" + name;

        // Never overwrite. Two different photographs can easily share a name --
        // every camera restarts at IMG_0001 eventually -- so a collision means
        // finding a free variant, not replacing what is there.
        int suffix = 2;
        while (claimed.count(destination) || (exists && exists(destination))) {
            destination = folder + "/" + stem + " (" + std::to_string(suffix) + ")" + extension;
            ++suffix;
        }

        claimed.insert(destination);

        if (destination == photo.path) {
            ++plan.unchanged;
            continue;
        }

        plan.moves.push_back({photo.path, destination});
    }

    return plan;
}

} // namespace winnow
