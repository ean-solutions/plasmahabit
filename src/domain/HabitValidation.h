#pragma once

#include <QDate>
#include <QString>
#include <optional>
#include "domain/StreakCalculator.h"

namespace PlasmaHabit::Validation {

constexpr int kMaxHabitNameLength = 120;

[[nodiscard]] std::optional<QString> sanitizeName(const QString &rawName);
[[nodiscard]] std::optional<FrequencyType> parseFrequency(const QString &rawType);
[[nodiscard]] QString frequencyToString(FrequencyType type);
[[nodiscard]] bool isValidTarget(FrequencyType type, int targetCount);
[[nodiscard]] QString sanitizeColor(const QString &rawColor);
[[nodiscard]] std::optional<QDate> parseCanonicalDate(const QString &rawDate, const QDate &today = QDate::currentDate());

} // namespace PlasmaHabit::Validation
