#pragma once

#include "core/Photo.h"

#include <functional>
#include <string>
#include <vector>

namespace winnow {

struct PlannedMove {
    std::string source;
    std::string destination;
};

struct OrganizePlan {
    std::vector<PlannedMove> moves;
    size_t undated = 0;    // photos with no EXIF date, filed under Undated/
    size_t unchanged = 0;  // already in the right place
};

// Decide where each photo should live, without touching the disk.
//
// Layout is destinationRoot/YYYY/YYYY-MM/originalName.ext, and photos whose
// capture date could not be read go to destinationRoot/Undated/ rather than
// being guessed at or silently skipped.
//
// This function computes a plan and performs none of it. That separation is
// what makes the dangerous half of the feature testable: every naming rule and
// every collision case is checked here against plain strings, with no files
// created and nothing that can be lost if a test is wrong. The caller executes
// the plan afterwards, and can show it to the user first.
//
// `exists` reports whether a destination path is already occupied. Passing it
// in rather than calling the filesystem directly is the same inversion used for
// the decoder: tests supply a fake set of occupied names and check that
// collisions are resolved, without needing those files to exist.
OrganizePlan planOrganise(const std::vector<Photo>& photos,
                          const std::vector<size_t>& selected,
                          const std::string& destinationRoot,
                          const std::function<bool(const std::string&)>& exists = {});

// Split a path into the directory part and the final component.
std::string fileNameOf(const std::string& path);

} // namespace winnow
