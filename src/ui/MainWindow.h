#pragma once

#include "core/LibraryScanner.h"

#include <QMainWindow>
#include <QSet>
#include <QString>

class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QPushButton;
class QThread;

namespace winnow {

class ScanWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Start a scan directly, without going through the folder dialog. Exists so
    // the screenshot tool can drive the real window rather than a mock-up.
    void beginScan(const QString& folder);

signals:
    void scanFinished();

private slots:
    void chooseFolder();
    void startScan();
    void stopScan();
    void onProgress(int done, int total);
    void onWorkerFinished();
    void onGroupSelected(int row);
    void onPhotoToggled(QListWidgetItem* item);
    void deleteMarked();
    void organiseByDate();

private:
    void buildUi();
    void populateGroups();
    void showGroup(int groupIndex);
    void updateSummary();
    void setBusy(bool busy);
    int selectedThreshold() const;

    QLabel* folderLabel_ = nullptr;
    QComboBox* strictness_ = nullptr;
    QPushButton* scanButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* organiseButton_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QListWidget* groupList_ = nullptr;
    QListWidget* photoList_ = nullptr;
    QLabel* summary_ = nullptr;

    QThread* thread_ = nullptr;
    ScanWorker* worker_ = nullptr;

    QString folder_;
    ScanResult result_;

    // Photo indices the user has ticked for removal, kept here rather than in
    // the list widget because the widget only ever holds one group at a time.
    QSet<int> marked_;

    // Filling the list programmatically emits the same itemChanged signal that
    // a user click does, which would immediately overwrite the selection being
    // set up. Same class of feedback loop as an editor that saves whenever its
    // text changes, including when the program is the one changing it.
    bool populating_ = false;
};

} // namespace winnow
