#include <QTest>
#include <QDate>
#include "domain/StreakCalculator.h"

using namespace PlasmaHabit;

class TestStreakCalculator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDailyStreakConsecutiveEndingToday();
    void testDailyStreakAliveTodayWhenYesterdayCompleted();
    void testDailyStreakResetsToZeroWhenYesterdayMissed();
    void testDailyStreakRetroactivePastDayFill();
    void testWeeklyStreakCountsConsecutiveWeeks();
    void testWeeklyStreakAliveMidWeekWhenReachable();
    void testWeeklyStreakFridayBoundary();
    void testWeeklyStreakResetsImmediatelyWhenUnreachable();
    void testWeeklyStreakResetsWhenPriorWeekMissed();
    void testWeeklyStreakSundayStartBasic();
    void testWeeklyStreakSundayStartEdgeCases();
};

void TestStreakCalculator::testDailyStreakConsecutiveEndingToday()
{
    const QDate today(2026, 10, 29); // Thursday
    QSet<QDate> completions = {
        today,
        today.addDays(-1),
        today.addDays(-2),
        today.addDays(-3),
        today.addDays(-4)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Daily, 1, completions, today);
    QCOMPARE(stats.currentStreak, 5);
    QCOMPARE(stats.longestStreak, 5);
}

void TestStreakCalculator::testDailyStreakAliveTodayWhenYesterdayCompleted()
{
    const QDate today(2026, 10, 29);
    // Today not done yet, but yesterday and 2 days ago are done
    QSet<QDate> completions = {
        today.addDays(-1),
        today.addDays(-2),
        today.addDays(-3)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Daily, 1, completions, today);
    QCOMPARE(stats.currentStreak, 3);
    QCOMPARE(stats.longestStreak, 3);
}

void TestStreakCalculator::testDailyStreakResetsToZeroWhenYesterdayMissed()
{
    const QDate today(2026, 10, 29);
    // Yesterday missed (today-1 absent), only today-2 and today-3 done, today not done
    QSet<QDate> completions = {
        today.addDays(-2),
        today.addDays(-3)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Daily, 1, completions, today);
    QCOMPARE(stats.currentStreak, 0);
    QCOMPARE(stats.longestStreak, 2);
}

void TestStreakCalculator::testDailyStreakRetroactivePastDayFill()
{
    const QDate today(2026, 10, 29);
    // Run 1: today to today-2 (3 days)
    // Missing day: today-3
    // Run 2: today-4 to today-7 (4 days)
    QSet<QDate> completions = {
        today,
        today.addDays(-1),
        today.addDays(-2),
        // today.addDays(-3) missed
        today.addDays(-4),
        today.addDays(-5),
        today.addDays(-6),
        today.addDays(-7)
    };

    auto stats1 = StreakCalculator::calculate(FrequencyType::Daily, 1, completions, today);
    QCOMPARE(stats1.currentStreak, 3);
    QCOMPARE(stats1.longestStreak, 4);

    // Retroactively toggle today-3
    completions.insert(today.addDays(-3));
    auto stats2 = StreakCalculator::calculate(FrequencyType::Daily, 1, completions, today);
    QCOMPARE(stats2.currentStreak, 8);
    QCOMPARE(stats2.longestStreak, 8);
}

void TestStreakCalculator::testWeeklyStreakCountsConsecutiveWeeks()
{
    // Monday of current week: 2026-10-26, today = 2026-10-29 (Thursday, day 4 of week)
    const QDate today(2026, 10, 29);
    const QDate currentMon(2026, 10, 26);
    const QDate prevMon = currentMon.addDays(-7);
    const QDate prevPrevMon = currentMon.addDays(-14);

    // Target: 3 times per week
    // PrevPrev week: Mon, Tue, Wed (3 completions) -> OK
    // Prev week: Tue, Wed, Fri (3 completions) -> OK
    // Current week: Mon, Tue, Thu (today) (3 completions) -> Met target early!
    QSet<QDate> completions = {
        prevPrevMon, prevPrevMon.addDays(1), prevPrevMon.addDays(2),
        prevMon.addDays(1), prevMon.addDays(2), prevMon.addDays(4),
        currentMon, currentMon.addDays(1), today
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 3, completions, today);
    QCOMPARE(stats.currentStreak, 3);
    QCOMPARE(stats.longestStreak, 3);
}

void TestStreakCalculator::testWeeklyStreakAliveMidWeekWhenReachable()
{
    // Today is Thursday 2026-10-29. Days remaining in week: Thu(today), Fri, Sat, Sun = 4 days possible.
    // Target: 3 times per week. Current completions this week: 0.
    // Since 0 + 4 >= 3, week is still reachable!
    // Prev week met target (3 completions).
    const QDate today(2026, 10, 29);
    const QDate currentMon(2026, 10, 26);
    const QDate prevMon = currentMon.addDays(-7);

    QSet<QDate> completions = {
        prevMon, prevMon.addDays(1), prevMon.addDays(2)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 3, completions, today);
    // Prior streak of 1 week remains alive
    QCOMPARE(stats.currentStreak, 1);
    QCOMPARE(stats.longestStreak, 1);
}

void TestStreakCalculator::testWeeklyStreakFridayBoundary()
{
    // Friday 2026-10-30 (day 5 of week). Target: 3.
    // Completions this week: 0 (today not completed).
    // Remaining days: Fri(5), Sat(6), Sun(7) = 3 days.
    // 0 + 3 == 3 >= 3 -> Still reachable! Prior streak remains alive.
    const QDate friday(2026, 10, 30);
    const QDate currentMon(2026, 10, 26);
    const QDate prevMon = currentMon.addDays(-7);

    QSet<QDate> completions = {
        prevMon, prevMon.addDays(1), prevMon.addDays(2)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 3, completions, friday);
    QCOMPARE(stats.currentStreak, 1);
    QCOMPARE(stats.longestStreak, 1);
}

void TestStreakCalculator::testWeeklyStreakResetsImmediatelyWhenUnreachable()
{
    // Today is Saturday 2026-10-31 (day 6 of week).
    // Target: 3 times per week.
    // Completions this week so far: 0 (and today not done).
    // Remaining days: Sat(today), Sun = 2 days.
    // Maximum possible this week: 0 + 2 = 2 < 3. Impossible to reach target!
    // Streak must strictly reset to 0 immediately mid-week!
    const QDate today(2026, 10, 31);
    const QDate currentMon(2026, 10, 26);
    const QDate prevMon = currentMon.addDays(-7);

    QSet<QDate> completions = {
        prevMon, prevMon.addDays(1), prevMon.addDays(2)
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 3, completions, today);
    QCOMPARE(stats.currentStreak, 0);
    QCOMPARE(stats.longestStreak, 1);
}

void TestStreakCalculator::testWeeklyStreakResetsWhenPriorWeekMissed()
{
    // Current week met target (3 completions).
    // Prior week missed target (only 1 completion).
    // Prior-prior week met target (3 completions).
    const QDate today(2026, 10, 29);
    const QDate currentMon(2026, 10, 26);
    const QDate prevMon = currentMon.addDays(-7);
    const QDate prevPrevMon = currentMon.addDays(-14);

    QSet<QDate> completions = {
        prevPrevMon, prevPrevMon.addDays(1), prevPrevMon.addDays(2),
        prevMon, // only 1 in prev week (missed!)
        currentMon, currentMon.addDays(1), today // 3 in current week
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 3, completions, today);
    QCOMPARE(stats.currentStreak, 1); // Only current week counts, prior chain broken
    QCOMPARE(stats.longestStreak, 1);
}

void TestStreakCalculator::testWeeklyStreakSundayStartBasic()
{
    // Sunday 2026-10-25 to Saturday 2026-10-31 is one week when week starts on Sunday.
    // Sunday 2026-10-18 to Saturday 2026-10-24 is the previous week.
    // Let's test with today = Wednesday 2026-10-28.
    const QDate today(2026, 10, 28); // Wednesday
    const QDate prevSun(2026, 10, 18);
    const QDate curSun(2026, 10, 25);

    // Target = 2 / week.
    // Previous week completed: Sun 18, Tue 20 -> 2 completions (OK).
    // Current week completed: Sun 25, today Wed 28 -> 2 completions (Met target!).
    QSet<QDate> completions = {
        prevSun, prevSun.addDays(2),
        curSun, today
    };

    auto stats = StreakCalculator::calculate(FrequencyType::Weekly, 2, completions, today, Qt::Sunday);
    QCOMPARE(stats.currentStreak, 2);
    QCOMPARE(stats.longestStreak, 2);
}

void TestStreakCalculator::testWeeklyStreakSundayStartEdgeCases()
{
    // Edge case 1: A completion on Sunday 2026-10-25
    // Under Qt::Monday start, Sun 2026-10-25 belongs to the week starting Mon 2026-10-19.
    // Under Qt::Sunday start, Sun 2026-10-25 belongs to the week starting Sun 2026-10-25!
    const QDate sunDate(2026, 10, 25);
    QSet<QDate> completions = { sunDate };

    // With today = Sunday 2026-10-25 and Target = 1:
    // With Qt::Sunday start, current week starts today and is completed! Streak = 1.
    auto statsSun = StreakCalculator::calculate(FrequencyType::Weekly, 1, completions, sunDate, Qt::Sunday);
    QCOMPARE(statsSun.currentStreak, 1);

    // Edge case 2: Today not done yet on Sunday (start of week)
    // Target = 2. No completions this week yet.
    // 0 done, 7 days possible (Sun..Sat). 0 + 7 >= 2 -> week still reachable, prior streak preserved.
    const QDate prevSun(2026, 10, 18);
    QSet<QDate> prevCompletions = { prevSun, prevSun.addDays(1) }; // 2 completions in prev week
    auto statsSundayEmpty = StreakCalculator::calculate(FrequencyType::Weekly, 2, prevCompletions, sunDate, Qt::Sunday);
    QCOMPARE(statsSundayEmpty.currentStreak, 1);

    // Edge case 3: Missed day leading to impossibility on Saturday (last day of Sunday week)
    // Saturday 2026-10-31. Target = 2. Completions this week = 0.
    // Saturday is remaining day (1 day possible: Sat). 0 + 1 = 1 < 2 -> Impossible, resets to 0!
    const QDate satDate(2026, 10, 31);
    auto statsSatImpossible = StreakCalculator::calculate(FrequencyType::Weekly, 2, prevCompletions, satDate, Qt::Sunday);
    QCOMPARE(statsSatImpossible.currentStreak, 0);
}

QTEST_MAIN(TestStreakCalculator)
#include "tst_streakcalculator.moc"
