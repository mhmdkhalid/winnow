#include "ui/MainWindow.h"

#include "core/ExifDate.h"
#include "core/OrganizePlanner.h"
#include "ui/ScanWorker.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

namespace winnow {
namespace {

constexpr int kThumbnailSide = 132;

QString shortSize(uint64_t bytes) {
    if (bytes >= 1024ull * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / 1024.0, 'f', 0) + " KB";
}

// Decode straight to thumbnail size. Asking the codec for a small image is far
// cheaper than decoding a full-size photograph and scaling it down afterwards.
QPixmap thumbnailFor(const QString& path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid())
        reader.setScaledSize(size.scaled(kThumbnailSide, kThumbnailSide, Qt::KeepAspectRatio));

    const QImage image = reader.read();
    if (image.isNull())
        return {};
    return QPixmap::fromImage(image);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();
    setWindowTitle(tr("Winnow — find near-duplicate photos"));
    resize(1180, 760);
}

MainWindow::~MainWindow() {
    // A running scan holds a thread that must be stopped before this object,
    // which it signals into, is destroyed.
    if (thread_) {
        if (worker_)
            worker_->requestCancel();
        thread_->quit();
        thread_->wait();
    }
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // --- top bar ------------------------------------------------------------
    auto* topBar = new QHBoxLayout;

    auto* chooseButton = new QPushButton(tr("Choose folder…"), central);
    connect(chooseButton, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    topBar->addWidget(chooseButton);

    folderLabel_ = new QLabel(tr("No folder selected"), central);
    folderLabel_->setStyleSheet("color: #555;");
    topBar->addWidget(folderLabel_, 1);

    topBar->addWidget(new QLabel(tr("Match:"), central));
    strictness_ = new QComboBox(central);
    strictness_->addItem(tr("Only near-identical"), 1);
    strictness_->addItem(tr("Balanced (recommended)"), 3);
    strictness_->addItem(tr("Catch more variations"), 5);
    strictness_->setCurrentIndex(1);
    topBar->addWidget(strictness_);

    scanButton_ = new QPushButton(tr("Scan"), central);
    scanButton_->setDefault(true);
    connect(scanButton_, &QPushButton::clicked, this, &MainWindow::startScan);
    topBar->addWidget(scanButton_);

    stopButton_ = new QPushButton(tr("Stop"), central);
    stopButton_->setEnabled(false);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopScan);
    topBar->addWidget(stopButton_);

    layout->addLayout(topBar);

    progress_ = new QProgressBar(central);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(true);
    progress_->setFormat(tr("Ready"));
    layout->addWidget(progress_);

    // --- results ------------------------------------------------------------
    auto* splitter = new QSplitter(Qt::Horizontal, central);

    groupList_ = new QListWidget(splitter);
    groupList_->setMinimumWidth(240);
    connect(groupList_, &QListWidget::currentRowChanged, this, &MainWindow::onGroupSelected);
    splitter->addWidget(groupList_);

    photoList_ = new QListWidget(splitter);
    photoList_->setViewMode(QListView::IconMode);
    photoList_->setIconSize(QSize(kThumbnailSide, kThumbnailSide));
    photoList_->setGridSize(QSize(kThumbnailSide + 44, kThumbnailSide + 76));
    photoList_->setResizeMode(QListView::Adjust);
    photoList_->setMovement(QListView::Static);
    photoList_->setWordWrap(true);
    photoList_->setSpacing(6);
    connect(photoList_, &QListWidget::itemChanged, this, &MainWindow::onPhotoToggled);
    splitter->addWidget(photoList_);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    // --- bottom bar ---------------------------------------------------------
    auto* bottomBar = new QHBoxLayout;

    summary_ = new QLabel(tr("Choose a folder and press Scan."), central);
    bottomBar->addWidget(summary_, 1);

    organiseButton_ = new QPushButton(tr("Organise by date…"), central);
    organiseButton_->setEnabled(false);
    connect(organiseButton_, &QPushButton::clicked, this, &MainWindow::organiseByDate);
    bottomBar->addWidget(organiseButton_);

    deleteButton_ = new QPushButton(tr("Move ticked to Recycle Bin"), central);
    deleteButton_->setEnabled(false);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::deleteMarked);
    bottomBar->addWidget(deleteButton_);

    layout->addLayout(bottomBar);

    setCentralWidget(central);
}

int MainWindow::selectedThreshold() const {
    return strictness_ ? strictness_->currentData().toInt() : 3;
}

void MainWindow::chooseFolder() {
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Choose a photo folder"), folder_.isEmpty() ? QDir::homePath() : folder_);
    if (chosen.isEmpty())
        return;

    folder_ = chosen;
    folderLabel_->setText(QDir::toNativeSeparators(folder_));
}

void MainWindow::beginScan(const QString& folder) {
    folder_ = folder;
    folderLabel_->setText(QDir::toNativeSeparators(folder_));
    startScan();
}

void MainWindow::startScan() {
    if (thread_)
        return; // already running
    if (folder_.isEmpty()) {
        QMessageBox::information(this, tr("Winnow"), tr("Choose a folder to scan first."));
        return;
    }

    groupList_->clear();
    photoList_->clear();
    marked_.clear();
    result_ = ScanResult{};

    thread_ = new QThread(this);
    worker_ = new ScanWorker; // no parent: it is about to change thread
    worker_->configure(folder_, selectedThreshold());
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &ScanWorker::run);
    connect(worker_, &ScanWorker::progressed, this, &MainWindow::onProgress);
    connect(worker_, &ScanWorker::statusChanged, this,
            [this](const QString& message) { progress_->setFormat(message); });
    connect(worker_, &ScanWorker::finished, this, &MainWindow::onWorkerFinished);

    setBusy(true);
    progress_->setRange(0, 0); // indeterminate until the file count is known
    thread_->start();
}

void MainWindow::stopScan() {
    if (worker_)
        worker_->requestCancel();
    progress_->setFormat(tr("Stopping…"));
}

void MainWindow::onProgress(int done, int total) {
    if (progress_->maximum() != total) {
        progress_->setRange(0, total);
    }
    progress_->setValue(done);
    progress_->setFormat(tr("Analysed %1 of %2 photos").arg(done).arg(total));
}

void MainWindow::onWorkerFinished() {
    result_ = worker_->result();
    const bool cancelled = worker_->wasCancelled();

    thread_->quit();
    thread_->wait();
    delete worker_;
    worker_ = nullptr;
    thread_->deleteLater();
    thread_ = nullptr;

    setBusy(false);
    progress_->setRange(0, 100);
    progress_->setValue(cancelled ? 0 : 100);
    progress_->setFormat(cancelled ? tr("Stopped") : tr("Done"));

    populateGroups();
    emit scanFinished();
}

void MainWindow::populateGroups() {
    groupList_->clear();

    // Every group starts with everything ticked except the keeper, because that
    // is what the user almost always wants -- but nothing is acted on until
    // they press the button, and they can untick anything first.
    for (size_t g = 0; g < result_.groups.size(); ++g) {
        const std::vector<size_t>& group = result_.groups[g];
        const size_t keeper = group[result_.bestInGroup(group)];
        for (size_t index : group) {
            if (index != keeper)
                marked_.insert(static_cast<int>(index));
        }

        auto* item = new QListWidgetItem(
            tr("Group %1  —  %2 photos, %3 to remove")
                .arg(g + 1).arg(group.size()).arg(group.size() - 1),
            groupList_);
        item->setData(Qt::UserRole, static_cast<int>(g));
    }

    if (result_.groups.empty()) {
        auto* item = new QListWidgetItem(tr("No near-duplicates found"), groupList_);
        item->setFlags(Qt::NoItemFlags);
    } else {
        groupList_->setCurrentRow(0);
    }

    updateSummary();
}

void MainWindow::showGroup(int groupIndex) {
    populating_ = true;
    photoList_->clear();

    if (groupIndex >= 0 && static_cast<size_t>(groupIndex) < result_.groups.size()) {
        const std::vector<size_t>& group = result_.groups[groupIndex];
        const size_t keeper = group[result_.bestInGroup(group)];

        for (size_t index : group) {
            const Photo& photo = result_.photos[index];
            const QString path = QString::fromStdString(photo.path);
            const QFileInfo info(path);

            QString label = info.fileName();
            label += QString("\n%1 x %2  ·  %3")
                         .arg(photo.width).arg(photo.height)
                         .arg(shortSize(static_cast<uint64_t>(info.size())));
            if (photo.dateTaken)
                label += "\n" + QString::fromStdString(formatDateTaken(*photo.dateTaken));
            if (index == keeper)
                label += tr("\n★ sharpest — keep");

            auto* item = new QListWidgetItem(QIcon(thumbnailFor(path)), label, photoList_);
            item->setData(Qt::UserRole, static_cast<int>(index));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(marked_.contains(static_cast<int>(index)) ? Qt::Checked
                                                                          : Qt::Unchecked);
            item->setToolTip(path);
        }
    }

    populating_ = false;
}

void MainWindow::onGroupSelected(int row) {
    if (row < 0 || !groupList_->item(row)) {
        photoList_->clear();
        return;
    }
    const QVariant data = groupList_->item(row)->data(Qt::UserRole);
    showGroup(data.isValid() ? data.toInt() : -1);
}

void MainWindow::onPhotoToggled(QListWidgetItem* item) {
    if (populating_ || !item)
        return; // the program is filling the list, not the user clicking in it

    const int index = item->data(Qt::UserRole).toInt();
    if (item->checkState() == Qt::Checked)
        marked_.insert(index);
    else
        marked_.remove(index);

    updateSummary();
}

void MainWindow::updateSummary() {
    size_t inGroups = 0;
    for (const auto& group : result_.groups)
        inGroups += group.size();

    QString text = tr("%1 photos scanned").arg(result_.photos.size());
    if (result_.failed > 0)
        text += tr("  ·  %1 could not be read").arg(result_.failed);
    text += tr("  ·  %1 groups covering %2 photos").arg(result_.groups.size()).arg(inGroups);
    text += tr("  ·  %1 ticked for removal").arg(marked_.size());

    if (result_.stats.naiveComparisons > 0) {
        text += tr("  ·  %1 of %2 possible comparisons evaluated")
                    .arg(result_.stats.comparisons)
                    .arg(result_.stats.naiveComparisons);
    }

    summary_->setText(text);
    deleteButton_->setEnabled(!marked_.isEmpty());
    organiseButton_->setEnabled(!result_.photos.empty());
}

void MainWindow::setBusy(bool busy) {
    scanButton_->setEnabled(!busy);
    stopButton_->setEnabled(busy);
    strictness_->setEnabled(!busy);
    if (busy) {
        deleteButton_->setEnabled(false);
        organiseButton_->setEnabled(false);
    }
}

void MainWindow::deleteMarked() {
    if (marked_.isEmpty())
        return;

    const int answer = QMessageBox::question(
        this, tr("Move to Recycle Bin"),
        tr("Move %1 photos to the Recycle Bin?\n\n"
           "They are not deleted permanently — you can restore them from the "
           "Recycle Bin if this was a mistake.")
            .arg(marked_.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QSet<int> removed;
    QStringList failures;
    for (int index : marked_) {
        const QString path = QString::fromStdString(result_.photos[index].path);
        // Never a permanent delete. The whole feature rests on a judgement call
        // the program is not qualified to make -- which of two near-identical
        // photographs matters to the person who took them -- so the action has
        // to stay reversible.
        if (QFile::moveToTrash(path))
            removed.insert(index);
        else
            failures << QFileInfo(path).fileName();
    }

    // Drop the removed photos out of their groups, and drop any group that no
    // longer has at least two members left to compare.
    std::vector<std::vector<size_t>> survivingGroups;
    for (const auto& group : result_.groups) {
        std::vector<size_t> remaining;
        for (size_t index : group) {
            if (!removed.contains(static_cast<int>(index)))
                remaining.push_back(index);
        }
        if (remaining.size() >= 2)
            survivingGroups.push_back(std::move(remaining));
    }
    result_.groups = std::move(survivingGroups);

    for (int index : removed)
        marked_.remove(index);

    populateGroups();

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, tr("Some photos could not be moved"),
                             tr("%1 file(s) could not be moved to the Recycle Bin:\n\n%2")
                                 .arg(failures.size())
                                 .arg(failures.mid(0, 10).join("\n")));
    }
}

void MainWindow::organiseByDate() {
    if (result_.photos.empty())
        return;

    const QString destination = QFileDialog::getExistingDirectory(
        this, tr("Where should the organised photos go?"), folder_);
    if (destination.isEmpty())
        return;

    std::vector<size_t> selected;
    for (size_t i = 0; i < result_.photos.size(); ++i) {
        if (result_.photos[i].decoded && !marked_.contains(static_cast<int>(i)))
            selected.push_back(i);
    }

    const OrganizePlan plan = planOrganise(
        result_.photos, selected, destination.toStdString(),
        [](const std::string& path) { return QFile::exists(QString::fromStdString(path)); });

    if (plan.moves.empty()) {
        QMessageBox::information(this, tr("Nothing to do"),
                                 tr("Every photo is already where it should be."));
        return;
    }

    // The plan is computed first and shown before anything is touched, so the
    // user agrees to a specific number of moves rather than to a vague promise.
    const int answer = QMessageBox::question(
        this, tr("Organise by date"),
        tr("Move %1 photos into dated folders under:\n%2\n\n"
           "%3 have no date recorded and will go into an “Undated” folder.\n"
           "Nothing is overwritten — a name that is already taken gets a number added.")
            .arg(plan.moves.size())
            .arg(QDir::toNativeSeparators(destination))
            .arg(plan.undated),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    int moved = 0;
    QStringList failures;
    for (const PlannedMove& move : plan.moves) {
        const QString from = QString::fromStdString(move.source);
        const QString to = QString::fromStdString(move.destination);

        if (!QDir().mkpath(QFileInfo(to).absolutePath())) {
            failures << QFileInfo(from).fileName();
            continue;
        }
        if (QFile::rename(from, to)) {
            ++moved;
            // Keep the in-memory library pointing at where the file now is, so
            // a second action in the same session does not chase a stale path.
            for (Photo& photo : result_.photos) {
                if (photo.path == move.source) {
                    photo.path = move.destination;
                    break;
                }
            }
        } else {
            failures << QFileInfo(from).fileName();
        }
    }

    showGroup(groupList_->currentRow());

    QString message = tr("Moved %1 photos.").arg(moved);
    if (!failures.isEmpty())
        message += tr("\n\n%1 could not be moved.").arg(failures.size());
    QMessageBox::information(this, tr("Organise by date"), message);
}

} // namespace winnow
