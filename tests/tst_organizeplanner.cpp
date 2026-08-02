#include "core/OrganizePlanner.h"

#include <QtTest>

#include <set>

using namespace winnow;

namespace {

Photo dated(const std::string& path, int year, int month) {
    Photo photo;
    photo.path = path;
    photo.decoded = true;
    DateTaken when;
    when.year = year;
    when.month = month;
    when.day = 1;
    photo.dateTaken = when;
    return photo;
}

Photo undated(const std::string& path) {
    Photo photo;
    photo.path = path;
    photo.decoded = true;
    return photo;
}

std::vector<size_t> allOf(const std::vector<Photo>& photos) {
    std::vector<size_t> indices(photos.size());
    for (size_t i = 0; i < photos.size(); ++i)
        indices[i] = i;
    return indices;
}

} // namespace

class TestOrganizePlanner : public QObject {
    Q_OBJECT

private slots:
    void filesIntoYearAndMonthFolders();
    void photosWithoutADateGoToUndated();
    void neverOverwritesAFileAlreadyOnDisk();
    void neverOverwritesAFileClaimedEarlierInTheSameBatch();
    void suffixGoesBeforeTheExtension();
    void photoAlreadyInPlaceIsNotMoved();
    void outOfRangeSelectionIsIgnored();
    void trailingSeparatorOnTheRootIsHarmless();
};

void TestOrganizePlanner::filesIntoYearAndMonthFolders() {
    const std::vector<Photo> photos = {dated("C:/dump/IMG_1.jpg", 2026, 7)};

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos");

    QCOMPARE(plan.moves.size(), size_t{1});
    QCOMPARE(QString::fromStdString(plan.moves[0].destination),
             QString("D:/Photos/2026/2026-07/IMG_1.jpg"));
}

void TestOrganizePlanner::photosWithoutADateGoToUndated() {
    // Filed somewhere findable rather than guessed at from the file's
    // modification time, which is usually the date it was last copied.
    const std::vector<Photo> photos = {undated("C:/dump/scan.png")};

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos");

    QCOMPARE(plan.undated, size_t{1});
    QCOMPARE(QString::fromStdString(plan.moves[0].destination),
             QString("D:/Photos/Undated/scan.png"));
}

void TestOrganizePlanner::neverOverwritesAFileAlreadyOnDisk() {
    // Every camera restarts its numbering eventually, so two entirely different
    // photographs called IMG_0001.jpg is normal, not exotic.
    const std::vector<Photo> photos = {dated("C:/dump/IMG_0001.jpg", 2026, 7)};
    const std::set<std::string> occupied = {"D:/Photos/2026/2026-07/IMG_0001.jpg"};

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos",
                                           [&](const std::string& path) {
                                               return occupied.count(path) > 0;
                                           });

    QCOMPARE(QString::fromStdString(plan.moves[0].destination),
             QString("D:/Photos/2026/2026-07/IMG_0001 (2).jpg"));
}

void TestOrganizePlanner::neverOverwritesAFileClaimedEarlierInTheSameBatch() {
    // Nothing has been written yet, so asking the filesystem would say the path
    // is free for both. The plan has to remember what it already promised.
    const std::vector<Photo> photos = {
        dated("C:/a/IMG_0001.jpg", 2026, 7),
        dated("C:/b/IMG_0001.jpg", 2026, 7),
        dated("C:/c/IMG_0001.jpg", 2026, 7),
    };

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos");

    QCOMPARE(plan.moves.size(), size_t{3});
    QCOMPARE(QString::fromStdString(plan.moves[0].destination),
             QString("D:/Photos/2026/2026-07/IMG_0001.jpg"));
    QCOMPARE(QString::fromStdString(plan.moves[1].destination),
             QString("D:/Photos/2026/2026-07/IMG_0001 (2).jpg"));
    QCOMPARE(QString::fromStdString(plan.moves[2].destination),
             QString("D:/Photos/2026/2026-07/IMG_0001 (3).jpg"));
}

void TestOrganizePlanner::suffixGoesBeforeTheExtension() {
    // "IMG (2).jpg", not "IMG.jpg (2)" -- otherwise the file stops being a JPEG
    // as far as every other program is concerned.
    const std::vector<Photo> photos = {
        dated("C:/a/holiday.jpeg", 2026, 1),
        dated("C:/b/holiday.jpeg", 2026, 1),
    };

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos");

    QVERIFY(QString::fromStdString(plan.moves[1].destination).endsWith(".jpeg"));
    QCOMPARE(QString::fromStdString(plan.moves[1].destination),
             QString("D:/Photos/2026/2026-01/holiday (2).jpeg"));
}

void TestOrganizePlanner::photoAlreadyInPlaceIsNotMoved() {
    // Re-running the organiser over an already-organised library should be a
    // no-op, not a cascade of "(2)" copies.
    const std::vector<Photo> photos = {dated("D:/Photos/2026/2026-07/IMG_1.jpg", 2026, 7)};

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos");

    QVERIFY(plan.moves.empty());
    QCOMPARE(plan.unchanged, size_t{1});
}

void TestOrganizePlanner::outOfRangeSelectionIsIgnored() {
    const std::vector<Photo> photos = {dated("C:/a.jpg", 2026, 7)};
    const OrganizePlan plan = planOrganise(photos, {0, 99}, "D:/Photos");
    QCOMPARE(plan.moves.size(), size_t{1});
}

void TestOrganizePlanner::trailingSeparatorOnTheRootIsHarmless() {
    const std::vector<Photo> photos = {dated("C:/a.jpg", 2026, 7)};

    const OrganizePlan plan = planOrganise(photos, allOf(photos), "D:/Photos/");

    QCOMPARE(QString::fromStdString(plan.moves[0].destination),
             QString("D:/Photos/2026/2026-07/a.jpg"));
}

QTEST_APPLESS_MAIN(TestOrganizePlanner)
#include "tst_organizeplanner.moc"
