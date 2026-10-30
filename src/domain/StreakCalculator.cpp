#include "domain/StreakCalculator.h"
#include <algorithm>
#include <vector>

namespace PlasmaHabit {

namespace {

StreakStats calculateDaily(const QSet<QDate> &completions, const QDate &today)
{
    if (completions.isEmpty()) {
        return StreakStats{0, 0};
    }

    // 1. Current streak
    int currentStreak = 0;
    QDate checkDate = today;

    if (completions.contains(today)) {
        while (completions.contains(checkDate)) {
            currentStreak++;
            checkDate = checkDate.addDays(-1);
        }
    } else if (completions.contains(today.addDays(-1))) {
        checkDate = today.addDays(-1);
        while (completions.contains(checkDate)) {
            currentStreak++;
            checkDate = checkDate.addDays(-1);
        }
    } else {
        currentStreak = 0;
    }

    // 2. Longest streak across all history
    std::vector<QDate> sortedDates(completions.begin(), completions.end());
    std::sort(sortedDates.begin(), sortedDates.end());

    int longestStreak = 0;
    int run = 0;
    for (size_t i = 0; i < sortedDates.size(); ++i) {
        if (i == 0 || sortedDates[i] == sortedDates[i - 1].addDays(1)) {
            run++;
        } else {
            run = 1;
        }
        longestStreak = std::max(longestStreak, run);
    }

    return StreakStats{currentStreak, longestStreak};
}

// Helper to get day offset (0..6) from weekStartDay
int dayOffsetInWeek(const QDate &date, Qt::DayOfWeek weekStartDay)
{
    if (weekStartDay == Qt::Sunday) {
        // Qt::Sunday is 7. If date is Sunday (7), offset is 0. If Monday (1), offset is 1.
        return date.dayOfWeek() % 7;
    }
    // Qt::Monday is 1. If date is Monday (1), offset is 0. If Sunday (7), offset is 6.
    return date.dayOfWeek() - 1;
}

// Helper to get start of the week containing date
QDate startOfWeek(const QDate &date, Qt::DayOfWeek weekStartDay)
{
    return date.addDays(-dayOffsetInWeek(date, weekStartDay));
}

int completionsInWeek(const QDate &weekStart, const QSet<QDate> &completions, const QDate &limitDate = QDate())
{
    int count = 0;
    for (int d = 0; d < 7; ++d) {
        const QDate day = weekStart.addDays(d);
        if (limitDate.isValid() && day > limitDate) {
            continue;
        }
        if (completions.contains(day)) {
            count++;
        }
    }
    return count;
}

StreakStats calculateWeekly(int targetCount, const QSet<QDate> &completions, const QDate &today, Qt::DayOfWeek weekStartDay)
{
    if (targetCount <= 0) {
        return StreakStats{0, 0};
    }

    const QDate curWeekStart = startOfWeek(today, weekStartDay);
    const int todayOffset = dayOffsetInWeek(today, weekStartDay);

    // Current week analysis (count only completions up to today)
    int c_cur = completionsInWeek(curWeekStart, completions, today);
    int r_cur = 0;
    if (completions.contains(today)) {
        r_cur = 6 - todayOffset;
    } else {
        r_cur = 7 - todayOffset;
    }

    int priorChain = 0;
    QDate wStart = curWeekStart.addDays(-7);
    while (completionsInWeek(wStart, completions) >= targetCount) {
        priorChain++;
        wStart = wStart.addDays(-7);
    }

    int currentStreak = 0;
    if (c_cur >= targetCount) {
        // Current week met target -> counts as 1 + chain of consecutive prior weeks
        currentStreak = 1 + priorChain;
    } else if (c_cur + r_cur >= targetCount) {
        // Still reachable! Prior streak is kept alive
        currentStreak = priorChain;
    } else {
        // Unreachable! Reset strictly to 0 immediately
        currentStreak = 0;
    }

    // Longest weekly streak calculation across all history
    if (completions.isEmpty()) {
        return StreakStats{currentStreak, 0};
    }

    // Find earliest and latest start of week represented in completions
    QDate earliestDate = *std::min_element(completions.begin(), completions.end());
    QDate latestDate = *std::max_element(completions.begin(), completions.end());

    QDate startW = startOfWeek(earliestDate, weekStartDay);
    QDate endW = startOfWeek(latestDate, weekStartDay);

    // Guard against unreasonable historical spans (DoS prevention)
    if (startW.daysTo(endW) > 366 * 150) {
        return StreakStats{currentStreak, currentStreak};
    }

    int longestStreak = 0;
    int currentRun = 0;

    for (QDate m = startW; m <= endW; m = m.addDays(7)) {
        if (completionsInWeek(m, completions) >= targetCount) {
            currentRun++;
            longestStreak = std::max(longestStreak, currentRun);
        } else {
            currentRun = 0;
        }
    }

    longestStreak = std::max(longestStreak, currentStreak);

    return StreakStats{currentStreak, longestStreak};
}

} // namespace

StreakStats StreakCalculator::calculate(
    FrequencyType type,
    int targetCount,
    const QSet<QDate> &completions,
    const QDate &today,
    Qt::DayOfWeek weekStartDay)
{
    if (type == FrequencyType::Daily) {
        return calculateDaily(completions, today);
    }
    return calculateWeekly(targetCount, completions, today, weekStartDay);
}

} // namespace PlasmaHabit
