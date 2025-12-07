#include "BrainSegmentationTableModel.h"
#include <QDebug>

BrainSegmentationTableModel::BrainSegmentationTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void BrainSegmentationTableModel::loadRegions(const QVector<SegmentationRegion>& regions)
{
    beginResetModel();
    m_regions = regions;
    endResetModel();
    
    qDebug() << QStringLiteral("脑区分割表格模型加载了") << m_regions.size() << QStringLiteral("个脑区");
}

void BrainSegmentationTableModel::clear()
{
    beginResetModel();
    m_regions.clear();
    endResetModel();
}

int BrainSegmentationTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_regions.size();
}

int BrainSegmentationTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 4;  // 中文名称、位置、容积、全脑占比
}

QVariant BrainSegmentationTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_regions.size())
        return QVariant();

    const SegmentationRegion& region = m_regions[index.row()];

    // 用于 TableView 的角色
    if (role == ChineseNameRole) {
        return region.chineseName;
    } else if (role == HemisphereRole) {
        if (region.hemisphere == "L") {
            return QStringLiteral("左");
        } else if (region.hemisphere == "R") {
            return QStringLiteral("右");
        } else {
            return QStringLiteral("中");
        }
    } else if (role == VolumeRole) {
        return QString::number(region.volume, 'f', 2);
    } else if (role == VolumePercentRole) {
        return QString::number(region.volumePercent, 'f', 2) + "%";
    } else if (role == LabelRole) {
        return region.label;
    }

    // 用于传统 TableView 的 DisplayRole
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return region.chineseName;
        case 1: {
            if (region.hemisphere == "L") {
                return QStringLiteral("左");
            } else if (region.hemisphere == "R") {
                return QStringLiteral("右");
            } else {
                return QStringLiteral("中");
            }
        }
        case 2: return QString::number(region.volume, 'f', 2);
        case 3: return QString::number(region.volumePercent, 'f', 2) + "%";
        default: return QVariant();
        }
    }

    return QVariant();
}

QVariant BrainSegmentationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return QStringLiteral("中文名称");
        case 1: return QStringLiteral("位置");
        case 2: return QStringLiteral("容积");
        case 3: return QStringLiteral("全脑占比");
        default: return QVariant();
        }
    } else {
        return section + 1;  // 行号从1开始
    }
}

QHash<int, QByteArray> BrainSegmentationTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ChineseNameRole] = "chineseName";
    roles[HemisphereRole] = "hemisphere";
    roles[VolumeRole] = "volume";
    roles[VolumePercentRole] = "volumePercent";
    roles[LabelRole] = "label";
    return roles;
}

