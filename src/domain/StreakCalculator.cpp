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

// Helper to get Monday of the ISO week containing date
QDate mondayOfWeek(const QDate &date)
{
    // QDate::dayOfWeek(): 1 = Monday .. 7 = Sunday
    return date.addDays(-(date.dayOfWeek() - 1));
}

int completionsInWeek(const QDate &monday, const QSet<QDate> &completions, const QDate &limitDate = QDate())
{
    int count = 0;
    for (int d = 0; d < 7; ++d) {
        const QDate day = monday.addDays(d);
        if (limitDate.isValid() && day > limitDate) {
            continue;
        }
        if (completions.contains(day)) {
            count++;
        }
    }
    return count;
}

StreakStats calculateWeekly(int targetCount, const QSet<QDate> &completions, const QDate &today)
{
    if (targetCount <= 0) {
        return StreakStats{0, 0};
    }

    const QDate currentMon = mondayOfWeek(today);

    // Current week analysis (count only completions up to today)
    int c_cur = completionsInWeek(currentMon, completions, today);
    int r_cur = 0;
    if (completions.contains(today)) {
        r_cur = 7 - today.dayOfWeek();
    } else {
        r_cur = 7 - today.dayOfWeek() + 1;
    }

    int priorChain = 0;
    QDate wMon = currentMon.addDays(-7);
    while (completionsInWeek(wMon, completions) >= targetCount) {
        priorChain++;
        wMon = wMon.addDays(-7);
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

    // Find earliest and latest Monday represented in completions
    QDate earliestDate = *std::min_element(completions.begin(), completions.end());
    QDate latestDate = *std::max_element(completions.begin(), completions.end());

    QDate startMon = mondayOfWeek(earliestDate);
    QDate endMon = mondayOfWeek(latestDate);

    // Guard against unreasonable historical spans (DoS prevention)
    if (startMon.daysTo(endMon) > 366 * 150) {
        return StreakStats{currentStreak, currentStreak};
    }

    int longestStreak = 0;
    int currentRun = 0;

    for (QDate m = startMon; m <= endMon; m = m.addDays(7)) {
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
    const QDate &today)
{
    if (type == FrequencyType::Daily) {
        return calculateDaily(completions, today);
    }
    return calculateWeekly(targetCount, completions, today);
}

} // namespace PlasmaHabit
