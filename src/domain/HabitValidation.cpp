#include "domain/HabitValidation.h"

#include <QRegularExpression>
#include <vector>

namespace PlasmaHabit::Validation {

namespace {

bool isForbiddenControlOrBidi(char32_t cp)
{
    // C0 controls
    if (cp <= 0x1F) {
        return true;
    }
    // DEL and C1 controls
    if (cp >= 0x7F && cp <= 0x9F) {
        return true;
    }
    // Bidi override/embedding characters: U+202A - U+202E
    if (cp >= 0x202A && cp <= 0x202E) {
        return true;
    }
    // Bidi isolate controls: U+2066 - U+2069
    if (cp >= 0x2066 && cp <= 0x2069) {
        return true;
    }
    return false;
}

} // namespace

std::optional<QString> sanitizeName(const QString &rawName)
{
    QString filtered;
    filtered.reserve(rawName.size());

    int codePointCount = 0;
    int i = 0;
    while (i < rawName.size()) {
        const QChar c1 = rawName.at(i);
        char32_t ucs4 = 0;
        int advance = 1;

        if (c1.isHighSurrogate() && (i + 1 < rawName.size()) && rawName.at(i + 1).isLowSurrogate()) {
            ucs4 = QChar::surrogateToUcs4(c1, rawName.at(i + 1));
            advance = 2;
        } else {
            ucs4 = c1.unicode();
            advance = 1;
        }

        i += advance;

        if (isForbiddenControlOrBidi(ucs4)) {
            continue;
        }

        if (codePointCount < kMaxHabitNameLength) {
            if (advance == 2) {
                filtered.append(c1);
                filtered.append(rawName.at(i - 1));
            } else {
                filtered.append(c1);
            }
            codePointCount++;
        }
    }

    const QString trimmed = filtered.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    return trimmed;
}

std::optional<FrequencyType> parseFrequency(const QString &rawType)
{
    if (rawType == QStringLiteral("daily")) {
        return FrequencyType::Daily;
    }
    if (rawType == QStringLiteral("weekly")) {
        return FrequencyType::Weekly;
    }
    return std::nullopt;
}

QString frequencyToString(FrequencyType type)
{
    switch (type) {
    case FrequencyType::Daily:
        return QStringLiteral("daily");
    case FrequencyType::Weekly:
        return QStringLiteral("weekly");
    }
    return QStringLiteral("daily");
}

bool isValidTarget(FrequencyType type, int targetCount)
{
    if (type == FrequencyType::Daily) {
        return targetCount == 1;
    }
    if (type == FrequencyType::Weekly) {
        return targetCount >= 1 && targetCount <= 7;
    }
    return false;
}

QString sanitizeColor(const QString &rawColor)
{
    static const QRegularExpression hexRegex(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    if (hexRegex.match(rawColor).hasMatch()) {
        return rawColor.toLower();
    }
    return QStringLiteral("#2ecc71");
}

std::optional<QDate> parseCanonicalDate(const QString &rawDate, const QDate &today)
{
    if (rawDate.size() != 10) {
        return std::nullopt;
    }

    const QDate d = QDate::fromString(rawDate, Qt::ISODate);
    if (!d.isValid()) {
        return std::nullopt;
    }

    if (d.toString(Qt::ISODate) != rawDate) {
        return std::nullopt;
    }

    if (d.year() < 2000) {
        return std::nullopt;
    }

    if (d > today) {
        return std::nullopt;
    }

    return d;
}

} // namespace PlasmaHabit::Validation
