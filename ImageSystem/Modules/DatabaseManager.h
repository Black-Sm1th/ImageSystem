#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QList>
#include <QVariantMap>
#include <QtSql/QSqlDatabase>

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool openDatabase(const QString &path);
    void closeDatabase();

    // ========== dicom_info 表（原有） ==========
    bool createTable();
    bool insertRecord(const QString &dicomPath,
                      const QString &name,
                      const QString &medicalRecordId,
                      const QString &birthDate,
                      const QString &gender,
                      const QString &seriesUID);
    bool deleteByMedicalRecordId(const QString &medicalRecordId);
    bool deleteBySeriesUID(const QString &seriesUID);
    QList<QVariantMap> readAllRecords();

    // ========== completed_cases 表（新增） ==========
    bool createCompletedCasesTable();

    bool insertCompletedCase(const QString &name,
                             const QString &patientId,
                             const QString &examDate,
                             const QString &seriesUid,
                             int age,
                             const QString &sex,
                             const QString &checkType,
                             const QString &bidsPath,
                             const QString &outputPath);

    bool deleteCompletedCase(int id);

    // 模糊搜索：按 patient_id / name / exam_date
    QList<QVariantMap> searchCompletedCases(const QString &keyword);

    QList<QVariantMap> getAllCompletedCases();

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    QString m_lastError;
};

#endif // DATABASEMANAGER_H
