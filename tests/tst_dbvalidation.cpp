#include <QTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDate>
#include "db/DatabaseManager.h"
#include "domain/HabitValidation.h"
#include "models/HabitModel.h"

using namespace PlasmaHabit;

class TestDbValidation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSanitizeNameRules();
    void testParseCanonicalDateRules();
    void testTamperedDatabaseFiltering();
    void testWriteValidationPreventsBadData();
};

void TestDbValidation::testSanitizeNameRules()
{
    // Empty or spaces
    QCOMPARE(Validation::sanitizeName(QStringLiteral("")), std::nullopt);
    QCOMPARE(Validation::sanitizeName(QStringLiteral("   \t\n  ")), std::nullopt);

    // Bidi override characters stripped
    const QString bidiAttack = QStringLiteral("\u202Ereversed\u202C");
    auto sanitizedBidi = Validation::sanitizeName(bidiAttack);
    QVERIFY(sanitizedBidi.has_value());
    QCOMPARE(*sanitizedBidi, QStringLiteral("reversed"));

    // ZWJ preserved (e.g. family or complex emoji)
    const QString emojiWithZwj = QStringLiteral("Family \U0001F468\u200D\U0001F469");
    auto sanitizedEmoji = Validation::sanitizeName(emojiWithZwj);
    QVERIFY(sanitizedEmoji.has_value());
    QVERIFY(sanitizedEmoji->contains(QChar(0x200D)));

    // Max length 120 truncation
    const QString longString(200, QLatin1Char('A'));
    auto sanitizedLong = Validation::sanitizeName(longString);
    QVERIFY(sanitizedLong.has_value());
    QCOMPARE(sanitizedLong->size(), 120);
}

void TestDbValidation::testParseCanonicalDateRules()
{
    const QDate today(2026, 10, 29);

    // Valid canonical
    auto d1 = Validation::parseCanonicalDate(QStringLiteral("2026-10-29"), today);
    QVERIFY(d1.has_value());
    QCOMPARE(*d1, today);

    // Non-canonical lengths or trailing junk
    QCOMPARE(Validation::parseCanonicalDate(QStringLiteral("2026-10-29junk"), today), std::nullopt);
    QCOMPARE(Validation::parseCanonicalDate(QStringLiteral("2026-1-1"), today), std::nullopt);

    // Future date
    QCOMPARE(Validation::parseCanonicalDate(QStringLiteral("2026-10-30"), today), std::nullopt);

    // Pre-2000
    QCOMPARE(Validation::parseCanonicalDate(QStringLiteral("1999-12-31"), today), std::nullopt);
    QCOMPARE(Validation::parseCanonicalDate(QStringLiteral("0001-01-01"), today), std::nullopt);
}

void TestDbValidation::testTamperedDatabaseFiltering()
{
    QTemporaryDir tempDir;
    const QString dbPath = tempDir.filePath(QStringLiteral("tampered.sqlite3"));
    const QString connName = QStringLiteral("evil_creator");

    const QDate today = QDate::currentDate();
    const QString todayStr = today.toString(Qt::ISODate);
    const QString tomorrowStr = today.addDays(1).toString(Qt::ISODate);

    {
        auto evilDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        evilDb.setDatabaseName(dbPath);
        QVERIFY(evilDb.open());

        QSqlQuery q(evilDb);
        // Table WITHOUT check constraints
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE habits ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    name TEXT,"
            "    frequency_type TEXT,"
            "    target_count INTEGER,"
            "    color_hex TEXT,"
            "    is_archived INTEGER,"
            "    created_at TEXT"
            ");"
        )));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE habit_completions ("
            "    habit_id INTEGER,"
            "    completed_date TEXT,"
            "    PRIMARY KEY (habit_id, completed_date)"
            ");"
        )));

        // Insert corrupt / tampered rows
        // 1. Invalid frequency "hourly"
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habits VALUES (1, 'Beacon', 'hourly', 1, '#111111', 0, '2026-01-01');")));
        // 2. Empty name, weekly target 99
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habits VALUES (2, '   ', 'weekly', 99, '#222222', 0, '2026-01-01');")));
        // 3. Negative target, is_archived = 7
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habits VALUES (3, 'BadArchived', 'weekly', -5, '#333333', 7, '2026-01-01');")));
        // 4. Valid habit with invalid color
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habits VALUES (4, 'Valid Habit', 'daily', 1, 'bad-color', 0, '2026-01-01');")));

        // Completions for habit 4:
        // Future date, ancient date, junk date, and valid today date
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habit_completions VALUES (4, '0001-01-01');")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habit_completions VALUES (4, '9999-12-31');")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habit_completions VALUES (4, '%1');").arg(tomorrowStr)));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habit_completions VALUES (4, '%1junk');").arg(todayStr)));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO habit_completions VALUES (4, '%1');").arg(todayStr)));

        evilDb.close();
    }
    QSqlDatabase::removeDatabase(connName);

    DatabaseManager db;
    QVERIFY(db.initialize(dbPath));

    auto habits = db.fetchHabits(true);
    // Only habit 4 should survive
    QCOMPARE(habits.size(), 1);
    QCOMPARE(habits[0].id, 4);
    QCOMPARE(habits[0].name, QStringLiteral("Valid Habit"));
    QCOMPARE(habits[0].colorHex, QStringLiteral("#2ecc71")); // normalized fallback
    QVERIFY(db.rejectedRowCount() > 0);

    auto completions = db.fetchCompletions(4);
    // Only today should survive
    QCOMPARE(completions.size(), 1);
    QVERIFY(completions.contains(today));
}

void TestDbValidation::testWriteValidationPreventsBadData()
{
    QTemporaryDir tempDir;
    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("clean.sqlite3"))));

    HabitModel model(&db);

    // Empty name rejected
    QVERIFY(!model.addHabit(QStringLiteral("   "), QStringLiteral("daily"), 1, QStringLiteral("#2ecc71")));
    // Invalid frequency rejected
    QVERIFY(!model.addHabit(QStringLiteral("Test"), QStringLiteral("monthly"), 1, QStringLiteral("#2ecc71")));
    // Invalid weekly target rejected
    QVERIFY(!model.addHabit(QStringLiteral("Test"), QStringLiteral("weekly"), 10, QStringLiteral("#2ecc71")));

    // Valid habit with invalid color is sanitized
    QVERIFY(model.addHabit(QStringLiteral("Clean Test"), QStringLiteral("daily"), 1, QStringLiteral("url(evil)")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), HabitModel::ColorHexRole).toString(), QStringLiteral("#2ecc71"));
}

QTEST_MAIN(TestDbValidation)
#include "tst_dbvalidation.moc"
