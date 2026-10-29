#pragma once

#include <QDate>
#include <QSet>

namespace PlasmaHabit {

enum class FrequencyType {
    Daily,
    Weekly
};

struct StreakStats {
    int currentStreak = 0;
    int longestStreak = 0;
};

class StreakCalculator {
public:
    [[nodiscard]] static StreakStats calculate(
        FrequencyType type,
        int targetCount,
        const QSet<QDate> &completions,
        const QDate &today = QDate::currentDate());
};

} // namespace PlasmaHabit
