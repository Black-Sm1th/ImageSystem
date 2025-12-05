#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "Modules/BrainNetworkData.h"

class BrainRegionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum ColumnRoles {
        ChineseNameRole = Qt::UserRole + 1,
        EnglishNameRole,
        DegreeRole,
        ClusteringRole,
        LocalEfficiencyRole,
        ImagePathRole
    };

    explicit BrainRegionTableModel(QObject* parent = nullptr);

    // 加载数据
    void loadRegions(const QVector<BrainRegion>& regions, const QString& basePath);
    void clear();

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<BrainRegion> m_regions;
    QString m_basePath;  // 用于构建完整的图片路径
};

