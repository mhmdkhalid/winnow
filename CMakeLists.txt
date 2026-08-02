cmake_minimum_required(VERSION 3.21)
project(winnow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# winnow_core -- pure C++, deliberately NO Qt dependency.
#
# The perceptual hash, the sharpness score, the EXIF parser, the grouping and
# the thread pool all live here and know nothing about image formats, widgets,
# or where the application puts its files. That is what allows every algorithm
# to be tested against images built in memory, with no photographs in the
# repository and no codec involved.
#
# The dependency direction is one way: everything added later depends on core,
# never the reverse.
# ---------------------------------------------------------------------------
add_library(winnow_core STATIC
    src/core/CandidateIndex.cpp
    src/core/Downscale.cpp
    src/core/DuplicateGrouper.cpp
    src/core/ExifDate.cpp
    src/core/LibraryScanner.cpp
    src/core/OrganizePlanner.cpp
    src/core/PerceptualHash.cpp
    src/core/PhotoFinder.cpp
    src/core/Sharpness.cpp
    src/core/ThreadPool.cpp
)
target_include_directories(winnow_core PUBLIC src)
target_link_libraries(winnow_core PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# winnow_imaging -- the Qt implementation of PhotoDecoder.
# The only part of the project that knows JPEG and PNG exist.
# ---------------------------------------------------------------------------
add_library(winnow_imaging STATIC
    src/imaging/QtPhotoDecoder.cpp
    src/imaging/QtPhotoDecoder.h
)
target_link_libraries(winnow_imaging PUBLIC winnow_core Qt6::Gui)

# ---------------------------------------------------------------------------
# Tests -- one executable per unit, registered with CTest
# ---------------------------------------------------------------------------
enable_testing()

function(winnow_add_test name link_target)
    add_executable(tst_${name} tests/tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE ${link_target} Qt6::Test)
    target_include_directories(tst_${name} PRIVATE tests)
    add_test(NAME ${name} COMMAND tst_${name})
endfunction()

# Every one of these links core only -- proof that the code under test needs
# neither a GUI nor an image codec to run.
winnow_add_test(perceptualhash   winnow_core)
winnow_add_test(sharpness        winnow_core)
winnow_add_test(exifdate         winnow_core)
winnow_add_test(duplicategrouper winnow_core)
winnow_add_test(organizeplanner  winnow_core)
winnow_add_test(threadpool       winnow_core)
winnow_add_test(libraryscanner   winnow_core)

# This one needs the real codec: it writes images to disk and reads them back.
winnow_add_test(qtphotodecoder   winnow_imaging)
