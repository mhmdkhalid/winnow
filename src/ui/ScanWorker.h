#pragma once

#include "core/LibraryScanner.h"
#include "imaging/QtPhotoDecoder.h"

#include <QObject>
#include <QString>

#include <atomic>

namespace winnow {

// Runs a scan away from the user interface thread.
//
// Scanning a real photo folder takes seconds to minutes. Doing that on the
// thread that paints the window would freeze the application solidly for the
// duration -- no progress bar, no cancel button, and Windows eventually
// offering to close the "unresponsive" program. So the work is moved to its own
// thread and reports back through signals, which Qt delivers to the UI thread
// safely.
//
// Note that this is a second, separate layer of threading. This object gets one
// thread so the window keeps painting; inside it, the scanner's own pool
// spreads the decoding across every core.
class ScanWorker : public QObject {
    Q_OBJECT

public:
    explicit ScanWorker(QObject* parent = nullptr);

    void configure(const QString& folder, int threshold);
    const ScanResult& result() const { return result_; }
    bool wasCancelled() const { return cancelled_.load(); }

    // Safe to call from the UI thread while run() is in progress: the flag is
    // atomic, and the scanner checks it between photos.
    void requestCancel() { cancelled_.store(true); }

public slots:
    void run();

signals:
    void progressed(int done, int total);
    void statusChanged(const QString& message);
    void finished();

private:
    QString folder_;
    int threshold_ = 3;
    ScanResult result_;
    std::atomic<bool> cancelled_{false};
    QtPhotoDecoder decoder_;
};

} // namespace winnow
