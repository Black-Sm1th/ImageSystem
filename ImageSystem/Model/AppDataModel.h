#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>
#include <QString>
#include "Modules/CommonFunc.h"

struct HardwareScanResult {
    QString name;
    QString patientId;
    QString checkType;
    QString examDate;
    QString sex;
    int age = 0;
    QString seriesUid;
    int sliceCount = 0;
    QString status;
    QString outputPath;
    QString bidsPath;
};
Q_DECLARE_METATYPE(HardwareScanResult)

struct UnifiedRecord {
    enum Source { Completed = 0, Processing = 1 };
    Source source;
    QString name;
    QString patientId;
    QString examDate;
    QString status;
    QString sex;
    int age = 0;
    QString checkType;
    int sliceCount = 0;
    QString outputPath;
    QString bidsPath;
    QString seriesUid;
    int dbId = -1;
    double predictedBrainAge = -1.0;  // -1 表示未预测
};

class AppDataModel : public QAbstractListModel
{
    Q_OBJECT
    SINGLETON_CLASS(AppDataModel)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PatientIdRole,
        ExamDateRole,
        StatusRole,
        SourceRole,
        SexRole,
        AgeRole,
        CheckTypeRole,
        SliceCountRole,
        OutputPathRole,
        BidsPathRole,
        DbIdRole,
        PredictedBrainAgeRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantList records() const;

    void setPendingItems(const QList<HardwareScanResult> &items);
    void addPendingItem(const HardwareScanResult &item);
    void removePendingItem(const QString &name, const QString &examDate, const QString &seriesUid);
    void setPendingItemStatus(const QString &name, const QString &examDate, const QString &seriesUid, const QString &status);
    void clearPendingItems();

    void setCompletedItems(const QList<QVariantMap> &items);
    QString matchingRecordStatus(const QString &name, const QString &examDate, const QString &seriesUid) const;

    const QList<HardwareScanResult>& pendingItems() const { return m_pendingItems; }

private:
    void rebuildRecords();

    QList<UnifiedRecord> m_records;
    QList<HardwareScanResult> m_pendingItems;
    QList<QVariantMap> m_completedItems;
};
