#include <QTest>
#include <QTemporaryDir>
#include <QDate>
#include "db/DatabaseManager.h"
#include "models/HabitModel.h"

using namespace PlasmaHabit;

class TestHabitModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testModelCrudAndFilter();
    void testModelStreakRecalculationOnToggle();
    void testModelHeatmapCellsGeneration();
};

void TestHabitModel::testModelCrudAndFilter()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_model.sqlite3"))));

    HabitModel model(&db);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.totalActiveCount(), 0);

    // Add daily habit
    QVERIFY(model.addHabit(QStringLiteral("Coding"), QStringLiteral("daily"), 1, QStringLiteral("#3498db")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.totalActiveCount(), 1);

    int habitId = model.data(model.index(0, 0), HabitModel::IdRole).toInt();
    QVERIFY(habitId > 0);

    // Archive it
    QVERIFY(model.setHabitArchived(habitId, true));
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.totalActiveCount(), 0);

    // Toggle showArchived
    model.setShowArchived(true);
    QCOMPARE(model.rowCount(), 1);

    // Unarchive
    QVERIFY(model.setHabitArchived(habitId, false));
    model.setShowArchived(false);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.totalActiveCount(), 1);
}

void TestHabitModel::testModelStreakRecalculationOnToggle()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_streak.sqlite3"))));

    HabitModel model(&db);
    QVERIFY(model.addHabit(QStringLiteral("Reading"), QStringLiteral("daily"), 1, QStringLiteral("#2ecc71")));

    int habitId = model.data(model.index(0, 0), HabitModel::IdRole).toInt();
    const QString todayStr = QDate::currentDate().toString(Qt::ISODate);

    // Initially streak is 0
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CurrentStreakRole).toInt(), 0);
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), false);

    // Toggle today
    QVERIFY(model.toggleCompletion(habitId, todayStr));
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CurrentStreakRole).toInt(), 1);
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), true);

    // Toggle today off
    QVERIFY(model.toggleCompletion(habitId, todayStr));
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CurrentStreakRole).toInt(), 0);
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), false);
}

void TestHabitModel::testModelHeatmapCellsGeneration()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_heatmap.sqlite3"))));

    HabitModel model(&db);
    QVERIFY(model.addHabit(QStringLiteral("Exercise"), QStringLiteral("weekly"), 3, QStringLiteral("#e67e22")));

    QVariant cellsVar = model.data(model.index(0, 0), HabitModel::HeatmapCellsRole);
    QVariantList cells = cellsVar.toList();
    // 52 weeks * 7 days = 364 cells
    QCOMPARE(cells.size(), 52 * 7);

    QVariantList globalCells = model.globalHeatmap();
    QCOMPARE(globalCells.size(), 52 * 7);
}

QTEST_MAIN(TestHabitModel)
#include "tst_habitmodel.moc"
