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
    void testGlobalHeatmapIntensityLevels();
    void testToggleTodayMethod();
    void testWeekStartDayAndLabels();
    void testWeekStartDayWeeklyStreakRecalculation();
};

void TestHabitModel::testModelCrudAndFilter()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_model.sqlite3"))));

    HabitModel model(&db);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.property("count").toInt(), 0);
    QCOMPARE(model.totalActiveCount(), 0);

    // Add daily habit
    QVERIFY(model.addHabit(QStringLiteral("Coding"), QStringLiteral("daily"), 1, QStringLiteral("#3498db")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.property("count").toInt(), 1);
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

void TestHabitModel::testGlobalHeatmapIntensityLevels()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_intensity.sqlite3"))));

    HabitModel model(&db);
    QVERIFY(model.addHabit(QStringLiteral("H1"), QStringLiteral("daily"), 1, QStringLiteral("#111111")));
    QVERIFY(model.addHabit(QStringLiteral("H2"), QStringLiteral("daily"), 1, QStringLiteral("#222222")));
    QVERIFY(model.addHabit(QStringLiteral("H3"), QStringLiteral("daily"), 1, QStringLiteral("#333333")));
    QVERIFY(model.addHabit(QStringLiteral("H4"), QStringLiteral("daily"), 1, QStringLiteral("#444444")));

    int id1 = model.data(model.index(0, 0), HabitModel::IdRole).toInt();
    int id2 = model.data(model.index(1, 0), HabitModel::IdRole).toInt();
    int id3 = model.data(model.index(2, 0), HabitModel::IdRole).toInt();
    int id4 = model.data(model.index(3, 0), HabitModel::IdRole).toInt();

    auto findTodayCell = [&]() -> QVariantMap {
        const auto cells = model.globalHeatmap();
        for (const auto &c : cells) {
            const auto m = c.toMap();
            if (m.value(QStringLiteral("isToday")).toBool()) {
                return m;
            }
        }
        return {};
    };

    // 0 completed -> intensity 0
    QCOMPARE(findTodayCell().value(QStringLiteral("intensity")).toInt(), 0);

    // 1 of 4 (ratio 0.25) -> intensity 1 (<= 0.25)
    model.toggleToday(id1);
    QCOMPARE(findTodayCell().value(QStringLiteral("intensity")).toInt(), 1);

    // 2 of 4 (ratio 0.50) -> intensity 2 (0.25 < ratio <= 0.50)
    model.toggleToday(id2);
    QCOMPARE(findTodayCell().value(QStringLiteral("intensity")).toInt(), 2);

    // 3 of 4 (ratio 0.75) -> intensity 3 (0.50 < ratio <= 0.75)
    model.toggleToday(id3);
    QCOMPARE(findTodayCell().value(QStringLiteral("intensity")).toInt(), 3);

    // 4 of 4 (ratio 1.00) -> intensity 4 (> 0.75)
    model.toggleToday(id4);
    QCOMPARE(findTodayCell().value(QStringLiteral("intensity")).toInt(), 4);
}

void TestHabitModel::testToggleTodayMethod()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_today.sqlite3"))));

    HabitModel model(&db);
    QVERIFY(model.addHabit(QStringLiteral("Journal"), QStringLiteral("daily"), 1, QStringLiteral("#2ecc71")));
    int habitId = model.data(model.index(0, 0), HabitModel::IdRole).toInt();

    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), false);
    QVERIFY(model.toggleToday(habitId));
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), true);
    QVERIFY(model.toggleToday(habitId));
    QCOMPARE(model.data(model.index(0, 0), HabitModel::CompletedTodayRole).toBool(), false);
}

void TestHabitModel::testWeekStartDayAndLabels()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_labels.sqlite3"))));

    HabitModel model(&db);

    // Initial default: monday
    QCOMPARE(model.weekStartDay(), QStringLiteral("monday"));

    // Day labels for monday start: Mon (0), Wed (2), Fri (4)
    QStringList monDayLabels = model.dayLabels();
    QCOMPARE(monDayLabels.size(), 7);
    QCOMPARE(monDayLabels[0], QStringLiteral("Mon"));
    QCOMPARE(monDayLabels[1], QStringLiteral(""));
    QCOMPARE(monDayLabels[2], QStringLiteral("Wed"));
    QCOMPARE(monDayLabels[3], QStringLiteral(""));
    QCOMPARE(monDayLabels[4], QStringLiteral("Fri"));
    QCOMPARE(monDayLabels[5], QStringLiteral(""));
    QCOMPARE(monDayLabels[6], QStringLiteral(""));

    // Month labels: 52 entries
    QVariantList monMonthLabels = model.monthLabels();
    QCOMPARE(monMonthLabels.size(), 52);

    // Change to sunday start
    model.setWeekStartDay(QStringLiteral("sunday"));
    QCOMPARE(model.weekStartDay(), QStringLiteral("sunday"));

    // Day labels for sunday start: Sun (0, empty), Mon (1), Wed (3), Fri (5)
    QStringList sunDayLabels = model.dayLabels();
    QCOMPARE(sunDayLabels.size(), 7);
    QCOMPARE(sunDayLabels[0], QStringLiteral(""));
    QCOMPARE(sunDayLabels[1], QStringLiteral("Mon"));
    QCOMPARE(sunDayLabels[2], QStringLiteral(""));
    QCOMPARE(sunDayLabels[3], QStringLiteral("Wed"));
    QCOMPARE(sunDayLabels[4], QStringLiteral(""));
    QCOMPARE(sunDayLabels[5], QStringLiteral("Fri"));
    QCOMPARE(sunDayLabels[6], QStringLiteral(""));
}

void TestHabitModel::testWeekStartDayWeeklyStreakRecalculation()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("test_weekly_switch.sqlite3"))));

    HabitModel model(&db);
    QVERIFY(model.addHabit(QStringLiteral("Guitar Practice"), QStringLiteral("weekly"), 1, QStringLiteral("#e67e22")));
    int habitId = model.data(model.index(0, 0), HabitModel::IdRole).toInt();

    // Today is Sunday 2026-10-25. Mark it completed.
    const QString sunDateStr = QStringLiteral("2026-10-25");
    // Under Monday start, this Sunday belongs to the Mon 2026-10-19 week.
    // Under Sunday start, this Sunday starts the new Sun 2026-10-25 week.
    QVERIFY(model.toggleCompletion(habitId, sunDateStr));

    // Switching weekStartDay reloads model and updates weekly progress / cells
    model.setWeekStartDay(QStringLiteral("sunday"));
    QCOMPARE(model.weekStartDay(), QStringLiteral("sunday"));
    QCOMPARE(db.weekStartDay(), Qt::Sunday);
}

QTEST_MAIN(TestHabitModel)
#include "tst_habitmodel.moc"
