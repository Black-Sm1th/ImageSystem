#include "DatabaseManager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

namespace Demo {

void runDemo()
{
    DatabaseManager dbm;

    // choose a path inside user's temp directory for demo
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString dbPath = dataDir + QDir::separator() + "dicom_demo.db";

    qDebug() << "Database path:" << dbPath;

    if (!dbm.openDatabase(dbPath)) {
        qWarning() << "Open DB failed:" << dbm.lastError();
        return;
    }

    if (!dbm.createTable()) {
        qWarning() << "Create table failed:" << dbm.lastError();
        return;
    }

    // Insert sample record
    bool ok = dbm.insertRecord("C:/dicom/sample", "张三", "MR123456", "1985-07-12", "M", "SERIES-UID-001");
    qDebug() << "Insert sample record:" << ok << dbm.lastError();

    // Read back
    auto rows = dbm.readAllRecords();
    qDebug() << "Total rows:" << rows.size();
    for (const auto &r : rows) {
        qDebug() << r;
    }

    // Delete sample by seriesUID
    bool delOk = dbm.deleteBySeriesUID("SERIES-UID-001");
    qDebug() << "Delete by seriesUID:" << delOk << dbm.lastError();

    dbm.closeDatabase();
}

} // namespace Demo
