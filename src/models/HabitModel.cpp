#include "models/HabitModel.h"
#include "domain/HabitValidation.h"

namespace PlasmaHabit {

namespace {

QDate mondayOfWeek(const QDate &date)
{
    return date.addDays(-(date.dayOfWeek() - 1));
}

} // namespace

HabitModel::HabitModel(DatabaseManager *dbManager, QObject *parent)
    : QAbstractListModel(parent)
    , m_dbManager(dbManager)
{
    reload();
}

int HabitModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant HabitModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.record.id;
    case NameRole:
        return item.record.name;
    case FrequencyTypeRole:
        return Validation::frequencyToString(item.record.frequencyType);
    case TargetCountRole:
        return item.record.targetCount;
    case ColorHexRole:
        return item.record.colorHex;
    case IsArchivedRole:
        return item.record.isArchived;
    case CompletedTodayRole:
        return item.completedToday;
    case CurrentStreakRole:
        return item.stats.currentStreak;
    case LongestStreakRole:
        return item.stats.longestStreak;
    case WeeklyProgressRole:
        return QStringLiteral("%1/%2").arg(item.weeklyCurrentCompletions).arg(item.record.targetCount);
    case HeatmapCellsRole:
        return computeHeatmapCells(item);
    default:
        return {};
    }
}

QHash<int, QByteArray> HabitModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[FrequencyTypeRole] = "frequencyType";
    roles[TargetCountRole] = "targetCount";
    roles[ColorHexRole] = "colorHex";
    roles[IsArchivedRole] = "isArchived";
    roles[CompletedTodayRole] = "completedToday";
    roles[CurrentStreakRole] = "currentStreak";
    roles[LongestStreakRole] = "longestStreak";
    roles[WeeklyProgressRole] = "weeklyProgress";
    roles[HeatmapCellsRole] = "heatmapCells";
    return roles;
}

bool HabitModel::showArchived() const
{
    return m_showArchived;
}

void HabitModel::setShowArchived(bool show)
{
    if (m_showArchived != show) {
        m_showArchived = show;
        Q_EMIT showArchivedChanged();
        reload();
    }
}

int HabitModel::totalActiveCount() const
{
    if (!m_dbManager || !m_dbManager->isOpen()) {
        return 0;
    }
    return m_dbManager->fetchHabits(false).size();
}

QString HabitModel::lastError() const
{
    return m_lastError;
}

void HabitModel::setStartupError(const QString &error)
{
    m_lastError = error;
    Q_EMIT lastErrorChanged();
}

void HabitModel::reload()
{
    if (!m_dbManager || !m_dbManager->isOpen()) {
        m_lastError = QStringLiteral("Database unavailable");
        Q_EMIT lastErrorChanged();
        return;
    }

    m_dbManager->resetRejectedRowCount();

    beginResetModel();
    m_items.clear();

    const QDate today = QDate::currentDate();
    const QDate currentMon = mondayOfWeek(today);

    const auto records = m_dbManager->fetchHabits(m_showArchived);
    for (const auto &rec : records) {
        HabitItem item;
        item.record = rec;
        item.completions = m_dbManager->fetchCompletions(rec.id);
        item.stats = StreakCalculator::calculate(rec.frequencyType, rec.targetCount, item.completions, today);
        item.completedToday = item.completions.contains(today);

        int weeklyCount = 0;
        for (int d = 0; d < 7; ++d) {
            if (item.completions.contains(currentMon.addDays(d))) {
                weeklyCount++;
            }
        }
        item.weeklyCurrentCompletions = weeklyCount;

        m_items.append(item);
    }

    endResetModel();

    if (m_dbManager->rejectedRowCount() > 0) {
        m_lastError = QStringLiteral("%1 corrupted records were ignored").arg(m_dbManager->rejectedRowCount());
        Q_EMIT lastErrorChanged();
    }

    Q_EMIT countChanged();
    Q_EMIT totalActiveCountChanged();
    Q_EMIT globalHeatmapChanged();
}

bool HabitModel::addHabit(const QString &name, const QString &frequencyType, int targetCount, const QString &colorHex)
{
    m_lastError.clear();
    Q_EMIT lastErrorChanged();

    if (!m_dbManager) {
        return false;
    }

    const auto sanitizedName = Validation::sanitizeName(name);
    const auto parsedFreq = Validation::parseFrequency(frequencyType);
    if (!sanitizedName || !parsedFreq || !Validation::isValidTarget(*parsedFreq, targetCount)) {
        m_lastError = QStringLiteral("Invalid habit data");
        Q_EMIT lastErrorChanged();
        return false;
    }

    const QString sanitizedColor = Validation::sanitizeColor(colorHex);

    auto id = m_dbManager->createHabit(*sanitizedName, Validation::frequencyToString(*parsedFreq), targetCount, sanitizedColor);
    if (!id.has_value()) {
        m_lastError = m_dbManager->lastError();
        Q_EMIT lastErrorChanged();
        return false;
    }

    reload();
    return true;
}

bool HabitModel::setHabitArchived(int habitId, bool archived)
{
    m_lastError.clear();
    Q_EMIT lastErrorChanged();

    if (!m_dbManager) {
        return false;
    }

    if (!m_dbManager->updateHabitArchived(habitId, archived)) {
        m_lastError = m_dbManager->lastError();
        Q_EMIT lastErrorChanged();
        return false;
    }

    reload();
    return true;
}

bool HabitModel::removeHabit(int habitId)
{
    m_lastError.clear();
    Q_EMIT lastErrorChanged();

    if (!m_dbManager) {
        return false;
    }

    if (!m_dbManager->deleteHabit(habitId)) {
        m_lastError = m_dbManager->lastError();
        Q_EMIT lastErrorChanged();
        return false;
    }

    reload();
    return true;
}

bool HabitModel::toggleCompletion(int habitId, const QString &isoDate)
{
    m_lastError.clear();
    Q_EMIT lastErrorChanged();

    if (!m_dbManager) {
        return false;
    }

    const QDate today = QDate::currentDate();
    const auto parsedDate = Validation::parseCanonicalDate(isoDate, today);
    if (!parsedDate.has_value()) {
        m_lastError = QStringLiteral("Cannot complete future dates or invalid format");
        Q_EMIT lastErrorChanged();
        return false;
    }

    if (!m_dbManager->toggleCompletion(habitId, *parsedDate, today)) {
        m_lastError = m_dbManager->lastError();
        Q_EMIT lastErrorChanged();
        return false;
    }

    reload();
    return true;
}

bool HabitModel::toggleToday(int habitId)
{
    return toggleCompletion(habitId, QDate::currentDate().toString(Qt::ISODate));
}

QVariantList HabitModel::computeHeatmapCells(const HabitItem &item) const
{
    QVariantList cells;
    const QDate today = QDate::currentDate();
    const QDate currentMon = mondayOfWeek(today);
    const QDate startMon = currentMon.addDays(-51 * 7);

    for (int week = 0; week < 52; ++week) {
        const QDate weekMon = startMon.addDays(week * 7);
        for (int dayOfWeek = 0; dayOfWeek < 7; ++dayOfWeek) {
            const QDate cellDate = weekMon.addDays(dayOfWeek);
            QVariantMap cell;
            cell[QStringLiteral("date")] = cellDate.toString(Qt::ISODate);
            cell[QStringLiteral("dayOfWeek")] = dayOfWeek; // 0=Mon .. 6=Sun
            cell[QStringLiteral("weekIndex")] = week;
            cell[QStringLiteral("isCompleted")] = item.completions.contains(cellDate);
            cell[QStringLiteral("isFuture")] = (cellDate > today);
            cell[QStringLiteral("isToday")] = (cellDate == today);
            cells.append(cell);
        }
    }

    return cells;
}

QVariantList HabitModel::globalHeatmap() const
{
    QVariantList cells;
    if (!m_dbManager || !m_dbManager->isOpen()) {
        return cells;
    }

    QList<QSet<QDate>> activeCompletions;
    if (!m_showArchived) {
        for (const auto &item : m_items) {
            activeCompletions.append(item.completions);
        }
    } else {
        const auto activeHabits = m_dbManager->fetchHabits(false);
        for (const auto &habit : activeHabits) {
            activeCompletions.append(m_dbManager->fetchCompletions(habit.id));
        }
    }

    const int totalActive = activeCompletions.size();

    QHash<QDate, int> dateCompletionCount;
    for (const auto &completions : activeCompletions) {
        for (const auto &d : completions) {
            dateCompletionCount[d]++;
        }
    }

    const QDate today = QDate::currentDate();
    const QDate currentMon = mondayOfWeek(today);
    const QDate startMon = currentMon.addDays(-51 * 7);

    for (int week = 0; week < 52; ++week) {
        const QDate weekMon = startMon.addDays(week * 7);
        for (int dayOfWeek = 0; dayOfWeek < 7; ++dayOfWeek) {
            const QDate cellDate = weekMon.addDays(dayOfWeek);
            const int count = dateCompletionCount.value(cellDate, 0);

            int intensity = 0;
            if (totalActive > 0 && count > 0) {
                const double ratio = static_cast<double>(count) / totalActive;
                if (ratio > 0.75) {
                    intensity = 4;
                } else if (ratio > 0.50) {
                    intensity = 3;
                } else if (ratio > 0.25) {
                    intensity = 2;
                } else {
                    intensity = 1;
                }
            }

            QVariantMap cell;
            cell[QStringLiteral("date")] = cellDate.toString(Qt::ISODate);
            cell[QStringLiteral("dayOfWeek")] = dayOfWeek;
            cell[QStringLiteral("weekIndex")] = week;
            cell[QStringLiteral("completedCount")] = count;
            cell[QStringLiteral("intensity")] = intensity; // 0..4
            cell[QStringLiteral("isFuture")] = (cellDate > today);
            cell[QStringLiteral("isToday")] = (cellDate == today);
            cells.append(cell);
        }
    }

    return cells;
}

} // namespace PlasmaHabit
