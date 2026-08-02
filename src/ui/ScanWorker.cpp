#include "ui/ScanWorker.h"

#include "core/PhotoFinder.h"
#include "core/ThreadPool.h"

#include <algorithm>

namespace winnow {

ScanWorker::ScanWorker(QObject* parent) : QObject(parent) {}

void ScanWorker::configure(const QString& folder, int threshold) {
    folder_ = folder;
    threshold_ = threshold;
    cancelled_.store(false);
    result_ = ScanResult{};
}

void ScanWorker::run() {
    emit statusChanged(tr("Looking for photos…"));

    const std::vector<std::string> paths = findPhotos(folder_.toStdString());
    if (paths.empty() || cancelled_.load()) {
        emit finished();
        return;
    }

    emit statusChanged(tr("Analysing %1 photos…").arg(paths.size()));

    ThreadPool pool;

    // One progress signal per photo would queue tens of thousands of events for
    // a large library and leave the UI thread doing nothing but redrawing a
    // progress bar. Roughly two hundred updates is past the point where the eye
    // can tell the difference.
    const size_t step = std::max<size_t>(1, paths.size() / 200);

    const LibraryScanner scanner(decoder_, pool);
    result_ = scanner.scan(paths, threshold_,
                           [this, step](size_t done, size_t total) {
                               if (done % step == 0 || done == total)
                                   emit progressed(static_cast<int>(done), static_cast<int>(total));
                           },
                           &cancelled_);

    emit finished();
}

} // namespace winnow
