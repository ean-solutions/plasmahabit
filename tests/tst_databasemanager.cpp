#include <QTest>
#include <QTemporaryDir>
#include <QDate>
#include "db/DatabaseManager.h"

using namespace PlasmaHabit;

class TestDatabaseManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInitializeCreatesSchema();
    void testCreateHabitAndFetch();
    void testArchiveHabitDoesNotDeleteCompletions();
    void testToggleCompletionIdempotent();
    void testToggleCompletionRejectsFutureDates();
    void testUpdateAndDeleteNonExistentHabitReturnsFalse();
};

void TestDatabaseManager::testInitializeCreatesSchema()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));
    QVERIFY(db.isOpen());
}

void TestDatabaseManager::testCreateHabitAndFetch()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    auto dailyId = db.createHabit(QStringLiteral("Morning Run"), QStringLiteral("daily"), 1, QStringLiteral("#e74c3c"));
    QVERIFY(dailyId.has_value());

    auto weeklyId = db.createHabit(QStringLiteral("Gym Session"), QStringLiteral("weekly"), 3, QStringLiteral("#3498db"));
    QVERIFY(weeklyId.has_value());

    auto habits = db.fetchHabits(false);
    QCOMPARE(habits.size(), 2);
    QCOMPARE(habits[0].name, QStringLiteral("Morning Run"));
    QCOMPARE(habits[0].frequencyType, FrequencyType::Daily);
    QCOMPARE(habits[1].name, QStringLiteral("Gym Session"));
    QCOMPARE(habits[1].targetCount, 3);
    QCOMPARE(habits[1].frequencyType, FrequencyType::Weekly);
}

void TestDatabaseManager::testArchiveHabitDoesNotDeleteCompletions()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    auto id = db.createHabit(QStringLiteral("Read Books"), QStringLiteral("daily"), 1, QStringLiteral("#2ecc71"));
    QVERIFY(id.has_value());

    const QDate today(2026, 10, 29);
    QVERIFY(db.toggleCompletion(*id, today, today));
    QVERIFY(db.toggleCompletion(*id, today.addDays(-1), today));

    auto completionsBefore = db.fetchCompletions(*id);
    QCOMPARE(completionsBefore.size(), 2);

    // Archive habit
    QVERIFY(db.updateHabitArchived(*id, true));

    // Active habits list should be empty
    auto activeHabits = db.fetchHabits(false);
    QCOMPARE(activeHabits.size(), 0);

    // All habits (including archived) should show it
    auto allHabits = db.fetchHabits(true);
    QCOMPARE(allHabits.size(), 1);
    QVERIFY(allHabits[0].isArchived);

    // Historical completions must still exist
    auto completionsAfter = db.fetchCompletions(*id);
    QCOMPARE(completionsAfter.size(), 2);
}

void TestDatabaseManager::testToggleCompletionIdempotent()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    auto id = db.createHabit(QStringLiteral("Meditate"), QStringLiteral("daily"), 1);
    QVERIFY(id.has_value());

    const QDate date(2026, 10, 29);
    QVERIFY(!db.isCompletedOn(*id, date));

    // Toggle on
    QVERIFY(db.toggleCompletion(*id, date, date));
    QVERIFY(db.isCompletedOn(*id, date));

    // Toggle off
    QVERIFY(db.toggleCompletion(*id, date, date));
    QVERIFY(!db.isCompletedOn(*id, date));
}

void TestDatabaseManager::testToggleCompletionRejectsFutureDates()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    auto id = db.createHabit(QStringLiteral("Write Journal"), QStringLiteral("daily"), 1);
    QVERIFY(id.has_value());

    const QDate today(2026, 10, 29);
    const QDate tomorrow = today.addDays(1);

    // Attempting to complete tomorrow must be rejected
    QVERIFY(!db.toggleCompletion(*id, tomorrow, today));
    QVERIFY(!db.isCompletedOn(*id, tomorrow));
}

void TestDatabaseManager::testUpdateAndDeleteNonExistentHabitReturnsFalse()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("test_plasmahabit.sqlite3"));

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    QVERIFY(!db.updateHabitArchived(424242, true));
    QCOMPARE(db.lastError(), QStringLiteral("Habit not found"));

    QVERIFY(!db.deleteHabit(424242));
    QCOMPARE(db.lastError(), QStringLiteral("Habit not found"));
}

QTEST_MAIN(TestDatabaseManager)
#include "tst_databasemanager.moc"
