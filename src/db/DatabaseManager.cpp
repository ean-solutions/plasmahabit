#include "db/DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QUuid>

namespace PlasmaHabit {

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString())
{
}

DatabaseManager::~DatabaseManager()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            auto db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DatabaseManager::initialize(const QString &customDbPath)
{
    QString dbPath = customDbPath;
    if (dbPath.isEmpty()) {
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(baseDir);
        dbPath = QDir(baseDir).filePath(QStringLiteral("plasmahabit.sqlite3"));
    } else {
        QFileInfo fi(dbPath);
        QDir().mkpath(fi.absolutePath());
    }

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    // Enable WAL mode and foreign keys
    QSqlQuery pragmaQuery(db);
    pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));

    // Create tables and indexes
    const QString habitsTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS habits ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name            TEXT    NOT NULL CHECK (length(trim(name)) > 0),"
        "    frequency_type  TEXT    NOT NULL CHECK (frequency_type IN ('daily', 'weekly')),"
        "    target_count    INTEGER NOT NULL CHECK ("
        "                        (frequency_type = 'daily'  AND target_count = 1) OR"
        "                        (frequency_type = 'weekly' AND target_count BETWEEN 1 AND 7)"
        "                    ),"
        "    color_hex       TEXT    NOT NULL DEFAULT '#2ecc71',"
        "    is_archived     INTEGER NOT NULL DEFAULT 0 CHECK (is_archived IN (0, 1)),"
        "    created_at      TEXT    NOT NULL DEFAULT (date('now', 'localtime'))"
        ");"
    );

    const QString completionsTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS habit_completions ("
        "    habit_id        INTEGER NOT NULL,"
        "    completed_date  TEXT    NOT NULL CHECK (completed_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),"
        "    PRIMARY KEY (habit_id, completed_date),"
        "    FOREIGN KEY (habit_id) REFERENCES habits(id) ON DELETE CASCADE"
        ");"
    );

    const QString idxDateSql = QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_completions_date ON habit_completions(completed_date);"
    );

    const QString idxArchivedSql = QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_habits_archived ON habits(is_archived);"
    );

    if (!pragmaQuery.exec(habitsTableSql) ||
        !pragmaQuery.exec(completionsTableSql) ||
        !pragmaQuery.exec(idxDateSql) ||
        !pragmaQuery.exec(idxArchivedSql)) {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::isOpen() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName).isOpen();
}

std::optional<int> DatabaseManager::createHabit(
    const QString &name,
    const QString &frequencyType,
    int targetCount,
    const QString &colorHex)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return std::nullopt;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO habits (name, frequency_type, target_count, color_hex) "
        "VALUES (:name, :type, :target, :color);"
    ));
    query.bindValue(QStringLiteral(":name"), name.trimmed());
    query.bindValue(QStringLiteral(":type"), frequencyType);
    query.bindValue(QStringLiteral(":target"), targetCount);
    query.bindValue(QStringLiteral(":color"), colorHex);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return std::nullopt;
    }

    return query.lastInsertId().toInt();
}

bool DatabaseManager::updateHabitArchived(int habitId, bool archived)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return false;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("UPDATE habits SET is_archived = :archived WHERE id = :id;"));
    query.bindValue(QStringLiteral(":archived"), archived ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), habitId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteHabit(int habitId)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return false;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM habits WHERE id = :id;"));
    query.bindValue(QStringLiteral(":id"), habitId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QList<HabitRecord> DatabaseManager::fetchHabits(bool includeArchived) const
{
    QList<HabitRecord> result;
    if (!isOpen()) {
        return result;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (includeArchived) {
        query.prepare(QStringLiteral("SELECT id, name, frequency_type, target_count, color_hex, is_archived, created_at FROM habits ORDER BY id ASC;"));
    } else {
        query.prepare(QStringLiteral("SELECT id, name, frequency_type, target_count, color_hex, is_archived, created_at FROM habits WHERE is_archived = 0 ORDER BY id ASC;"));
    }

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        HabitRecord rec;
        rec.id = query.value(0).toInt();
        rec.name = query.value(1).toString();
        rec.frequencyType = query.value(2).toString();
        rec.targetCount = query.value(3).toInt();
        rec.colorHex = query.value(4).toString();
        rec.isArchived = query.value(5).toInt() == 1;
        rec.createdAt = query.value(6).toString();
        result.append(rec);
    }

    return result;
}

std::optional<HabitRecord> DatabaseManager::fetchHabitById(int habitId) const
{
    if (!isOpen()) {
        return std::nullopt;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id, name, frequency_type, target_count, color_hex, is_archived, created_at FROM habits WHERE id = :id;"));
    query.bindValue(QStringLiteral(":id"), habitId);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    HabitRecord rec;
    rec.id = query.value(0).toInt();
    rec.name = query.value(1).toString();
    rec.frequencyType = query.value(2).toString();
    rec.targetCount = query.value(3).toInt();
    rec.colorHex = query.value(4).toString();
    rec.isArchived = query.value(5).toInt() == 1;
    rec.createdAt = query.value(6).toString();
    return rec;
}

bool DatabaseManager::toggleCompletion(int habitId, const QDate &date, const QDate &today)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return false;
    }

    if (date > today) {
        m_lastError = QStringLiteral("Cannot complete future dates");
        return false;
    }

    const QString isoDate = date.toString(Qt::ISODate);
    auto db = QSqlDatabase::database(m_connectionName);

    if (isCompletedOn(habitId, date)) {
        // Delete completion
        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(QStringLiteral("DELETE FROM habit_completions WHERE habit_id = :id AND completed_date = :date;"));
        deleteQuery.bindValue(QStringLiteral(":id"), habitId);
        deleteQuery.bindValue(QStringLiteral(":date"), isoDate);
        if (!deleteQuery.exec()) {
            m_lastError = deleteQuery.lastError().text();
            return false;
        }
        return true;
    } else {
        // Insert completion
        QSqlQuery insertQuery(db);
        insertQuery.prepare(QStringLiteral("INSERT INTO habit_completions (habit_id, completed_date) VALUES (:id, :date);"));
        insertQuery.bindValue(QStringLiteral(":id"), habitId);
        insertQuery.bindValue(QStringLiteral(":date"), isoDate);
        if (!insertQuery.exec()) {
            m_lastError = insertQuery.lastError().text();
            return false;
        }
        return true;
    }
}

QSet<QDate> DatabaseManager::fetchCompletions(int habitId) const
{
    QSet<QDate> result;
    if (!isOpen()) {
        return result;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT completed_date FROM habit_completions WHERE habit_id = :id;"));
    query.bindValue(QStringLiteral(":id"), habitId);

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        const QString str = query.value(0).toString();
        const QDate d = QDate::fromString(str, Qt::ISODate);
        if (d.isValid()) {
            result.insert(d);
        }
    }

    return result;
}

bool DatabaseManager::isCompletedOn(int habitId, const QDate &date) const
{
    if (!isOpen()) {
        return false;
    }

    const QString isoDate = date.toString(Qt::ISODate);
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1 FROM habit_completions WHERE habit_id = :id AND completed_date = :date LIMIT 1;"));
    query.bindValue(QStringLiteral(":id"), habitId);
    query.bindValue(QStringLiteral(":date"), isoDate);

    if (!query.exec()) {
        return false;
    }

    return query.next();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

} // namespace PlasmaHabit
