#include "BrainRegionTableModel.h"
#include <QDebug>

BrainRegionTableModel::BrainRegionTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void BrainRegionTableModel::loadRegions(const QVector<BrainRegion>& regions, const QString& basePath)
{
    beginResetModel();
    m_regions = regions;
    m_basePath = basePath;
    endResetModel();
    
    qDebug() << QStringLiteral("表格模型加载了") << m_regions.size() << QStringLiteral("个脑区");
}

void BrainRegionTableModel::clear()
{
    beginResetModel();
    m_regions.clear();
    m_basePath.clear();
    endResetModel();
}

int BrainRegionTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_regions.size();
}

int BrainRegionTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 5;  // 中文名称、Mricro命名、度、聚类、局部效率
}

QVariant BrainRegionTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_regions.size())
        return QVariant();

    const BrainRegion& region = m_regions[index.row()];

    // 用于 TableView 的角色
    if (role == ChineseNameRole) {
        return region.chineseName;
    } else if (role == EnglishNameRole) {
        return region.englishName;
    } else if (role == DegreeRole) {
        return region.degree;
    } else if (role == ClusteringRole) {
        return QString::number(region.clustering, 'f', 2);
    } else if (role == LocalEfficiencyRole) {
        return QString::number(region.localEfficiency, 'f', 2);
    } else if (role == ImagePathRole) {
        // 返回完整的 file:/// 路径
        return "file:///" + m_basePath + "/" + region.timeSeriesImagePath;
    }

    // 用于传统 TableView 的 DisplayRole
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return region.chineseName;
        case 1: return region.englishName;
        case 2: return region.degree;
        case 3: return QString::number(region.clustering, 'f', 2);
        case 4: return QString::number(region.localEfficiency, 'f', 2);
        default: return QVariant();
        }
    }

    return QVariant();
}

QVariant BrainRegionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return QStringLiteral("中文名称");
        case 1: return QStringLiteral("Mricro命名");
        case 2: return QStringLiteral("度");
        case 3: return QStringLiteral("聚类");
        case 4: return QStringLiteral("局部效率");
        default: return QVariant();
        }
    } else {
        return section + 1;  // 行号从1开始
    }
}

QHash<int, QByteArray> BrainRegionTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ChineseNameRole] = "chineseName";
    roles[EnglishNameRole] = "englishName";
    roles[DegreeRole] = "degree";
    roles[ClusteringRole] = "clustering";
    roles[LocalEfficiencyRole] = "localEfficiency";
    roles[ImagePathRole] = "imagePath";
    return roles;
}

