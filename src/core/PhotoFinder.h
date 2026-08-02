#pragma once

#include <string>
#include <vector>

namespace winnow {

// True when the path ends in an extension the decoder is expected to handle.
// Case-insensitive, because ".JPG" off a camera is as common as ".jpg".
bool hasPhotoExtension(const std::string& path);

// Every photo file under `root`, walked recursively.
//
// Directories that cannot be opened -- permission denied, a symlink loop, a
// disconnected network drive -- are skipped rather than aborting the walk. A
// single unreadable folder somewhere in a large library should not lose the
// scan of everything else.
std::vector<std::string> findPhotos(const std::string& root);

} // namespace winnow
