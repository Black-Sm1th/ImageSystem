#include "DatabaseManager.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QDate>
#include <QDebug>

static const char* CONNECTION_NAME = "ImageSystemConnection";

DatabaseManager::DatabaseManager()
{
}

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
}

bool DatabaseManager::openDatabase(const QString &path)
{
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        m_db = QSqlDatabase::database(CONNECTION_NAME);
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", CONNECTION_NAME);
    }

    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "Failed to open database:" << m_lastError;
        return false;
    }
    return true;
}

void DatabaseManager::closeDatabase()
{
    if (m_db.isValid() && m_db.isOpen()) {
        m_db.close();
    }
    // remove the connection if exists
    if (QSqlDatabase::contains(CONNECTION_NAME)) {
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
}

bool DatabaseManager::createTable()
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }

    QSqlQuery q(m_db);
    const QString sql =
        "CREATE TABLE IF NOT EXISTS dicom_info ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "dicom_path TEXT,"
        "name TEXT,"
        "medical_record_id TEXT,"
        "birth_date DATE,"
        "gender TEXT,"
        "seriesUID TEXT UNIQUE"
        ")";

    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        qWarning() << "Create table failed:" << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseManager::insertRecord(const QString &dicomPath,
                                   const QString &name,
                                   const QString &medicalRecordId,
                                   const QString &birthDate,
                                   const QString &gender,
                                   const QString &seriesUID)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO dicom_info (dicom_path, name, medical_record_id, birth_date, gender, seriesUID) "
              "VALUES (:dicom_path, :name, :medical_record_id, :birth_date, :gender, :seriesUID)");
    q.bindValue(":dicom_path", dicomPath);
    q.bindValue(":name", name);
    q.bindValue(":medical_record_id", medicalRecordId);
    q.bindValue(":birth_date", birthDate);
    q.bindValue(":gender", gender);
    q.bindValue(":seriesUID", seriesUID);

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Insert failed:" << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteByMedicalRecordId(const QString &medicalRecordId)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM dicom_info WHERE medical_record_id = :medical_record_id");
    q.bindValue(":medical_record_id", medicalRecordId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Delete by medical_record_id failed:" << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteBySeriesUID(const QString &seriesUID)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM dicom_info WHERE seriesUID = :seriesUID");
    q.bindValue(":seriesUID", seriesUID);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Delete by seriesUID failed:" << m_lastError;
        return false;
    }
    return true;
}

QList<QVariantMap> DatabaseManager::readAllRecords()
{
    QList<QVariantMap> result;
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return result;
    }

    QSqlQuery q(m_db);
    if (!q.exec("SELECT * FROM dicom_info")) {
        m_lastError = q.lastError().text();
        qWarning() << "Select failed:" << m_lastError;
        return result;
    }

    QSqlRecord rec = q.record();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < rec.count(); ++i) {
            row.insert(rec.fieldName(i), q.value(i));
        }
        result.append(row);
    }
    return result;
}

// ================================================================
// completed_cases 表
// ================================================================

bool DatabaseManager::createCompletedCasesTable()
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    const QString sql =
        "CREATE TABLE IF NOT EXISTS completed_cases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "patient_id TEXT,"
        "exam_date TEXT,"
        "series_uid TEXT UNIQUE,"
        "age INTEGER,"
        "sex TEXT,"
        "check_type TEXT,"
        "status TEXT DEFAULT 'completed',"
        "bids_path TEXT,"
        "output_path TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")";
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        qWarning() << "Create completed_cases table failed:" << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseManager::insertCompletedCase(const QString &name,
                                          const QString &patientId,
                                          const QString &examDate,
                                          const QString &seriesUid,
                                          int age,
                                          const QString &sex,
                                          const QString &checkType,
                                          const QString &bidsPath,
                                          const QString &outputPath)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO completed_cases "
              "(name, patient_id, exam_date, series_uid, age, sex, check_type, bids_path, output_path) "
              "VALUES (:name, :pid, :edate, :suid, :age, :sex, :ctype, :bpath, :opath)");
    q.bindValue(":name", name);
    q.bindValue(":pid", patientId);
    q.bindValue(":edate", examDate);
    q.bindValue(":suid", seriesUid);
    q.bindValue(":age", age);
    q.bindValue(":sex", sex);
    q.bindValue(":ctype", checkType);
    q.bindValue(":bpath", bidsPath);
    q.bindValue(":opath", outputPath);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Insert completed_case failed:" << m_lastError;
        return false;
    }
    return true;
}

bool DatabaseManager::deleteCompletedCase(int id)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM completed_cases WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Delete completed_case failed:" << m_lastError;
        return false;
    }
    return true;
}

QList<QVariantMap> DatabaseManager::searchCompletedCases(const QString &keyword)
{
    QList<QVariantMap> result;
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return result;
    }
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM completed_cases WHERE "
              "patient_id LIKE :kw1 OR name LIKE :kw2 OR exam_date LIKE :kw3 "
              "ORDER BY created_at DESC");
    QString like = QStringLiteral("%%1%").arg(keyword);
    q.bindValue(":kw1", like);
    q.bindValue(":kw2", like);
    q.bindValue(":kw3", like);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Search completed_cases failed:" << m_lastError;
        return result;
    }
    QSqlRecord rec = q.record();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < rec.count(); ++i)
            row.insert(rec.fieldName(i), q.value(i));
        result.append(row);
    }
    return result;
}

QList<QVariantMap> DatabaseManager::getAllCompletedCases()
{
    QList<QVariantMap> result;
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return result;
    }
    QSqlQuery q(m_db);
    if (!q.exec("SELECT * FROM completed_cases ORDER BY created_at DESC")) {
        m_lastError = q.lastError().text();
        qWarning() << "Select completed_cases failed:" << m_lastError;
        return result;
    }
    QSqlRecord rec = q.record();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < rec.count(); ++i)
            row.insert(rec.fieldName(i), q.value(i));
        result.append(row);
    }
    return result;
}
