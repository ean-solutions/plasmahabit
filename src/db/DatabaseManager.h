#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace PlasmaHabit {

struct HabitRecord {
    int id = -1;
    QString name;
    QString frequencyType; // "daily" or "weekly"
    int targetCount = 1;
    QString colorHex = QStringLiteral("#2ecc71");
    bool isArchived = false;
    QString createdAt;
};

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager() override;

    bool initialize(const QString &customDbPath = QString());
    [[nodiscard]] bool isOpen() const;

    // Habits CRUD
    std::optional<int> createHabit(const QString &name, const QString &frequencyType, int targetCount, const QString &colorHex = QStringLiteral("#2ecc71"));
    bool updateHabitArchived(int habitId, bool archived);
    bool deleteHabit(int habitId);
    [[nodiscard]] QList<HabitRecord> fetchHabits(bool includeArchived = false) const;
    [[nodiscard]] std::optional<HabitRecord> fetchHabitById(int habitId) const;

    // Completions CRUD
    bool toggleCompletion(int habitId, const QDate &date, const QDate &today = QDate::currentDate());
    [[nodiscard]] QSet<QDate> fetchCompletions(int habitId) const;
    [[nodiscard]] bool isCompletedOn(int habitId, const QDate &date) const;

    [[nodiscard]] QString lastError() const;

private:
    QString m_connectionName;
    QString m_lastError;
};

} // namespace PlasmaHabit
