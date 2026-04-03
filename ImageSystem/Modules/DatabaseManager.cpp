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
        "has_brain_age INTEGER DEFAULT 0,"
        "has_preprocessing INTEGER DEFAULT 0,"
        "preprocess_method TEXT DEFAULT '',"
        "status TEXT DEFAULT 'completed',"
        "bids_path TEXT,"
        "output_path TEXT,"
        "predicted_brain_age REAL DEFAULT -1.0,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")";
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        qWarning() << "Create completed_cases table failed:" << m_lastError;
        return false;
    }

    // 尝试添加 predicted_brain_age 列（如果表已存在但列不存在）
    q.prepare("PRAGMA table_info(completed_cases)");
    if (q.exec()) {
        bool hasPredictedBrainAge = false;
        bool hasBrainAgeFlag = false;
        bool hasPreprocessingFlag = false;
        bool hasPreprocessMethod = false;
        while (q.next()) {
            const QString columnName = q.value(1).toString();
            if (columnName == "predicted_brain_age")
                hasPredictedBrainAge = true;
            else if (columnName == "has_brain_age")
                hasBrainAgeFlag = true;
            else if (columnName == "has_preprocessing")
                hasPreprocessingFlag = true;
            else if (columnName == "preprocess_method")
                hasPreprocessMethod = true;
        }
        if (!hasPredictedBrainAge) {
            QSqlQuery alterQ(m_db);
            if (!alterQ.exec("ALTER TABLE completed_cases ADD COLUMN predicted_brain_age REAL DEFAULT -1.0")) {
                qWarning() << "Add predicted_brain_age column failed:" << alterQ.lastError().text();
            }
        }
        if (!hasBrainAgeFlag) {
            QSqlQuery alterQ(m_db);
            if (!alterQ.exec("ALTER TABLE completed_cases ADD COLUMN has_brain_age INTEGER DEFAULT 0")) {
                qWarning() << "Add has_brain_age column failed:" << alterQ.lastError().text();
            }
        }
        if (!hasPreprocessingFlag) {
            QSqlQuery alterQ(m_db);
            if (!alterQ.exec("ALTER TABLE completed_cases ADD COLUMN has_preprocessing INTEGER DEFAULT 0")) {
                qWarning() << "Add has_preprocessing column failed:" << alterQ.lastError().text();
            }
        }
        if (!hasPreprocessMethod) {
            QSqlQuery alterQ(m_db);
            if (!alterQ.exec("ALTER TABLE completed_cases ADD COLUMN preprocess_method TEXT DEFAULT ''")) {
                qWarning() << "Add preprocess_method column failed:" << alterQ.lastError().text();
            }
        }
    }

    return true;
}

bool DatabaseManager::insertCompletedCase(const QString &name,
                                          const QString &patientId,
                                          const QString &examDate,
                                          const QString &seriesUid,
                                          int age,
                                          const QString &sex,
                                          const QString &bidsPath,
                                          const QString &outputPath,
                                          double predictedBrainAge,
                                          bool hasBrainAge,
                                          bool hasPreprocessing,
                                          const QString &preprocessMethod)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO completed_cases "
              "(name, patient_id, exam_date, series_uid, age, sex, check_type, has_brain_age, has_preprocessing, preprocess_method, bids_path, output_path, predicted_brain_age) "
              "VALUES (:name, :pid, :edate, :suid, :age, :sex, "
              "CASE "
              "WHEN :has_brain_age = 1 AND :has_preprocessing = 1 THEN 'FullPipeline' "
              "WHEN :has_brain_age = 1 THEN 'BrainAgeOnly' "
              "WHEN :has_preprocessing = 1 THEN 'PrepOnly' "
              "ELSE 'Unknown' "
              "END, "
              ":has_brain_age, :has_preprocessing, :preprocess_method, :bpath, :opath, :predicted_age) "
              "ON CONFLICT(series_uid) DO UPDATE SET "
              "name = excluded.name, "
              "patient_id = excluded.patient_id, "
              "exam_date = excluded.exam_date, "
              "age = excluded.age, "
              "sex = excluded.sex, "
              "has_brain_age = MAX(completed_cases.has_brain_age, excluded.has_brain_age), "
              "has_preprocessing = MAX(completed_cases.has_preprocessing, excluded.has_preprocessing), "
              "preprocess_method = CASE "
              "WHEN excluded.preprocess_method != '' THEN excluded.preprocess_method "
              "ELSE completed_cases.preprocess_method "
              "END, "
              "check_type = CASE "
              "WHEN MAX(completed_cases.has_brain_age, excluded.has_brain_age) = 1 "
              " AND MAX(completed_cases.has_preprocessing, excluded.has_preprocessing) = 1 THEN 'FullPipeline' "
              "WHEN MAX(completed_cases.has_brain_age, excluded.has_brain_age) = 1 THEN 'BrainAgeOnly' "
              "WHEN MAX(completed_cases.has_preprocessing, excluded.has_preprocessing) = 1 THEN 'PrepOnly' "
              "ELSE completed_cases.check_type "
              "END, "
              "bids_path = excluded.bids_path, "
              "output_path = excluded.output_path, "
              "predicted_brain_age = CASE "
              "WHEN excluded.predicted_brain_age >= 0 THEN excluded.predicted_brain_age "
              "ELSE completed_cases.predicted_brain_age "
              "END");
    q.bindValue(":name", name);
    q.bindValue(":pid", patientId);
    q.bindValue(":edate", examDate);
    q.bindValue(":suid", seriesUid);
    q.bindValue(":age", age);
    q.bindValue(":sex", sex);
    q.bindValue(":has_brain_age", hasBrainAge ? 1 : 0);
    q.bindValue(":has_preprocessing", hasPreprocessing ? 1 : 0);
    q.bindValue(":preprocess_method", preprocessMethod);
    q.bindValue(":bpath", bidsPath);
    q.bindValue(":opath", outputPath);
    q.bindValue(":predicted_age", predictedBrainAge);
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

bool DatabaseManager::updatePredictedBrainAge(const QString &seriesUid, double predictedAge)
{
    if (!m_db.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare("UPDATE completed_cases SET predicted_brain_age = :age WHERE series_uid = :suid");
    q.bindValue(":age", predictedAge);
    q.bindValue(":suid", seriesUid);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        qWarning() << "Update predicted_brain_age failed:" << m_lastError;
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
