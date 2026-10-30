#include "db/DatabaseManager.h"
#include "domain/HabitValidation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QUuid>
#include <limits>

namespace PlasmaHabit {

namespace {

void enforceFilePermissions(const QString &filePath)
{
    if (QFile::exists(filePath)) {
        QFile::setPermissions(filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}

std::optional<HabitRecord> parseHabitRow(const QSqlQuery &query, int &rejectedRowsCounter)
{
    bool idOk = false;
    const qlonglong idVal = query.value(0).toLongLong(&idOk);
    if (!idOk || idVal <= 0 || idVal > std::numeric_limits<int>::max()) {
        rejectedRowsCounter++;
        return std::nullopt;
    }

    const auto nameOpt = Validation::sanitizeName(query.value(1).toString());
    if (!nameOpt) {
        rejectedRowsCounter++;
        return std::nullopt;
    }

    const auto freqOpt = Validation::parseFrequency(query.value(2).toString());
    if (!freqOpt) {
        rejectedRowsCounter++;
        return std::nullopt;
    }

    bool targetOk = false;
    const int targetVal = query.value(3).toInt(&targetOk);
    if (!targetOk || !Validation::isValidTarget(*freqOpt, targetVal)) {
        rejectedRowsCounter++;
        return std::nullopt;
    }

    const QString colorVal = Validation::sanitizeColor(query.value(4).toString());

    bool archivedOk = false;
    const int archivedVal = query.value(5).toInt(&archivedOk);
    if (!archivedOk || (archivedVal != 0 && archivedVal != 1)) {
        rejectedRowsCounter++;
        return std::nullopt;
    }

    HabitRecord rec;
    rec.id = static_cast<int>(idVal);
    rec.name = *nameOpt;
    rec.frequencyType = *freqOpt;
    rec.targetCount = targetVal;
    rec.colorHex = colorVal;
    rec.isArchived = (archivedVal == 1);
    rec.createdAt = query.value(6).toString();
    return rec;
}

} // namespace

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
    const bool isDefaultLocation = dbPath.isEmpty();

    if (isDefaultLocation) {
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(baseDir);
        QFile::setPermissions(baseDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
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

    if (isDefaultLocation) {
        enforceFilePermissions(dbPath);
        enforceFilePermissions(dbPath + QStringLiteral("-wal"));
        enforceFilePermissions(dbPath + QStringLiteral("-shm"));
    }

    // Enable WAL mode and foreign keys with strict verification
    QSqlQuery pragmaQuery(db);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL;"))) {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON;"))) {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }

    // Integrity checks on database
    if (pragmaQuery.exec(QStringLiteral("PRAGMA quick_check;"))) {
        if (pragmaQuery.next()) {
            const QString checkResult = pragmaQuery.value(0).toString();
            if (checkResult != QStringLiteral("ok")) {
                m_lastError = QStringLiteral("Database corruption detected: ") + checkResult;
                return false;
            }
        }
    } else {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }

    if (pragmaQuery.exec(QStringLiteral("PRAGMA foreign_key_check;"))) {
        if (pragmaQuery.next()) {
            m_lastError = QStringLiteral("Foreign key integrity check failed.");
            return false;
        }
    } else {
        m_lastError = pragmaQuery.lastError().text();
        return false;
    }

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

    const QString appSettingsTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "    key    TEXT PRIMARY KEY CHECK (length(trim(key)) > 0),"
        "    value  TEXT NOT NULL"
        ");"
    );

    if (!pragmaQuery.exec(habitsTableSql) ||
        !pragmaQuery.exec(completionsTableSql) ||
        !pragmaQuery.exec(idxDateSql) ||
        !pragmaQuery.exec(idxArchivedSql) ||
        !pragmaQuery.exec(appSettingsTableSql)) {
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
    m_lastError.clear();

    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return std::nullopt;
    }

    const auto sanitizedName = Validation::sanitizeName(name);
    if (!sanitizedName) {
        m_lastError = QStringLiteral("Invalid habit name");
        return std::nullopt;
    }

    const auto parsedFreq = Validation::parseFrequency(frequencyType);
    if (!parsedFreq) {
        m_lastError = QStringLiteral("Invalid frequency type");
        return std::nullopt;
    }

    if (!Validation::isValidTarget(*parsedFreq, targetCount)) {
        m_lastError = QStringLiteral("Invalid target count");
        return std::nullopt;
    }

    const QString sanitizedColor = Validation::sanitizeColor(colorHex);

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO habits (name, frequency_type, target_count, color_hex) "
        "VALUES (:name, :type, :target, :color);"
    ));
    query.bindValue(QStringLiteral(":name"), *sanitizedName);
    query.bindValue(QStringLiteral(":type"), Validation::frequencyToString(*parsedFreq));
    query.bindValue(QStringLiteral(":target"), targetCount);
    query.bindValue(QStringLiteral(":color"), sanitizedColor);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return std::nullopt;
    }

    bool idOk = false;
    const qlonglong lastId = query.lastInsertId().toLongLong(&idOk);
    if (!idOk || lastId <= 0 || lastId > std::numeric_limits<int>::max()) {
        m_lastError = QStringLiteral("Invalid generated row id");
        return std::nullopt;
    }

    return static_cast<int>(lastId);
}

bool DatabaseManager::updateHabitArchived(int habitId, bool archived)
{
    m_lastError.clear();

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

    if (query.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("Habit not found");
        return false;
    }

    return true;
}

bool DatabaseManager::deleteHabit(int habitId)
{
    m_lastError.clear();

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

    if (query.numRowsAffected() == 0) {
        m_lastError = QStringLiteral("Habit not found");
        return false;
    }

    return true;
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
        auto recOpt = parseHabitRow(query, m_rejectedRows);
        if (recOpt) {
            result.append(*recOpt);
        }
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

    return parseHabitRow(query, m_rejectedRows);
}

bool DatabaseManager::toggleCompletion(int habitId, const QDate &date, const QDate &today)
{
    m_lastError.clear();

    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return false;
    }

    if (date > today) {
        m_lastError = QStringLiteral("Cannot complete future dates");
        return false;
    }

    if (date.year() < 2000) {
        m_lastError = QStringLiteral("Invalid date range");
        return false;
    }

    const QString isoDate = date.toString(Qt::ISODate);
    auto db = QSqlDatabase::database(m_connectionName);

    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }

    // Attempt to delete existing completion
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(QStringLiteral("DELETE FROM habit_completions WHERE habit_id = :id AND completed_date = :date;"));
    deleteQuery.bindValue(QStringLiteral(":id"), habitId);
    deleteQuery.bindValue(QStringLiteral(":date"), isoDate);

    if (!deleteQuery.exec()) {
        m_lastError = deleteQuery.lastError().text();
        db.rollback();
        return false;
    }

    if (deleteQuery.numRowsAffected() == 0) {
        // Was not present, insert it
        QSqlQuery insertQuery(db);
        insertQuery.prepare(QStringLiteral("INSERT INTO habit_completions (habit_id, completed_date) VALUES (:id, :date);"));
        insertQuery.bindValue(QStringLiteral(":id"), habitId);
        insertQuery.bindValue(QStringLiteral(":date"), isoDate);

        if (!insertQuery.exec()) {
            m_lastError = insertQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
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

    const QDate today = QDate::currentDate();
    while (query.next()) {
        const QString str = query.value(0).toString();
        const auto dOpt = Validation::parseCanonicalDate(str, today);
        if (dOpt.has_value()) {
            result.insert(*dOpt);
        } else {
            m_rejectedRows++;
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

int DatabaseManager::rejectedRowCount() const
{
    return m_rejectedRows;
}

void DatabaseManager::resetRejectedRowCount()
{
    m_rejectedRows = 0;
}

Qt::DayOfWeek DatabaseManager::weekStartDay() const
{
    if (!isOpen()) {
        return Qt::Monday;
    }

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = 'week_start_day' LIMIT 1;"));

    if (query.exec() && query.next()) {
        const QString val = query.value(0).toString().trimmed().toLower();
        if (val == QStringLiteral("sunday")) {
            return Qt::Sunday;
        }
    }

    return Qt::Monday;
}

bool DatabaseManager::setWeekStartDay(Qt::DayOfWeek day)
{
    m_lastError.clear();

    if (!isOpen()) {
        m_lastError = QStringLiteral("Database not open");
        return false;
    }

    if (day != Qt::Monday && day != Qt::Sunday) {
        m_lastError = QStringLiteral("Unsupported week start day");
        return false;
    }

    const QString val = (day == Qt::Sunday) ? QStringLiteral("sunday") : QStringLiteral("monday");

    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO app_settings (key, value) VALUES ('week_start_day', :val) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;"
    ));
    query.bindValue(QStringLiteral(":val"), val);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

} // namespace PlasmaHabit
