#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QList>
#include <QSet>
#include <QVariantMap>
#include "db/DatabaseManager.h"
#include "domain/StreakCalculator.h"

namespace PlasmaHabit {

struct HabitItem {
    HabitRecord record;
    StreakStats stats;
    QSet<QDate> completions;
    bool completedToday = false;
    int weeklyCurrentCompletions = 0;
};

class HabitModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool showArchived READ showArchived WRITE setShowArchived NOTIFY showArchivedChanged)
    Q_PROPERTY(int totalActiveCount READ totalActiveCount NOTIFY totalActiveCountChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList globalHeatmap READ globalHeatmap NOTIFY globalHeatmapChanged)

public:
    enum HabitRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        FrequencyTypeRole,
        TargetCountRole,
        ColorHexRole,
        IsArchivedRole,
        CompletedTodayRole,
        CurrentStreakRole,
        LongestStreakRole,
        WeeklyProgressRole,
        HeatmapCellsRole
    };
    Q_ENUM(HabitRoles)

    explicit HabitModel(DatabaseManager *dbManager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool showArchived() const;
    void setShowArchived(bool show);

    [[nodiscard]] int totalActiveCount() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QVariantList globalHeatmap() const;

    void setStartupError(const QString &error);

    Q_INVOKABLE bool addHabit(const QString &name, const QString &frequencyType, int targetCount, const QString &colorHex);
    Q_INVOKABLE bool setHabitArchived(int habitId, bool archived);
    Q_INVOKABLE bool removeHabit(int habitId);
    Q_INVOKABLE bool toggleCompletion(int habitId, const QString &isoDate);
    Q_INVOKABLE bool toggleToday(int habitId);
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void countChanged();
    void showArchivedChanged();
    void totalActiveCountChanged();
    void lastErrorChanged();
    void globalHeatmapChanged();

private:
    DatabaseManager *m_dbManager;
    bool m_showArchived = false;
    QString m_lastError;
    QList<HabitItem> m_items;

    [[nodiscard]] QVariantList computeHeatmapCells(const HabitItem &item) const;
};

} // namespace PlasmaHabit
