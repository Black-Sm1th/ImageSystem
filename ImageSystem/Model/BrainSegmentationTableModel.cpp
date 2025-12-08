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
    return 6;  // 颜色、中文名称、位置、容积、全脑占比、不对称指数
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
    } else if (role == ColorRole) {
        // 返回十六进制颜色字符串 "#RRGGBBAA"，RGB转换为0-255范围
        int r = qBound(0, static_cast<int>(region.colorR * 255), 255);
        int g = qBound(0, static_cast<int>(region.colorG * 255), 255);
        int b = qBound(0, static_cast<int>(region.colorB * 255), 255);
        int a = qBound(0, static_cast<int>(region.colorA * 255), 255);
        
        // 格式化为 #RRGGBBAA
        return QString("#%1%2%3%4")
            .arg(a, 2, 16, QChar('0'))
            .arg(r, 2, 16, QChar('0'))
            .arg(g, 2, 16, QChar('0'))
            .arg(b, 2, 16, QChar('0'));
    } else if (role == AsymmetryIndexRole) {
        // 如果没有配对，返回空字符串
        if (region.partnerLabel == -1) {
            return QString("-");
        }
        return QString::number(region.asymmetryIndex, 'f', 3);
    }

    // 用于传统 TableView 的 DisplayRole
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: {
            // 颜色列返回十六进制颜色字符串
            int r = qBound(0, static_cast<int>(region.colorR * 255), 255);
            int g = qBound(0, static_cast<int>(region.colorG * 255), 255);
            int b = qBound(0, static_cast<int>(region.colorB * 255), 255);
            int a = qBound(0, static_cast<int>(region.colorA * 255), 255);
            
            // 格式化为 #RRGGBBAA
            return QString("#%1%2%3%4")
                .arg(a, 2, 16, QChar('0'))
                .arg(r, 2, 16, QChar('0'))
                .arg(g, 2, 16, QChar('0'))
                .arg(b, 2, 16, QChar('0'));
        }
        case 1: return region.chineseName;
        case 2: {
            if (region.hemisphere == "L") {
                return QStringLiteral("左");
            } else if (region.hemisphere == "R") {
                return QStringLiteral("右");
            } else {
                return QStringLiteral("中");
            }
        }
        case 3: return QString::number(region.volume, 'f', 2);
        case 4: return QString::number(region.volumePercent, 'f', 2) + "%";
        case 5: {
            // 如果没有配对，返回"-"
            if (region.partnerLabel == -1) {
                return QString("-");
            }
            return QString::number(region.asymmetryIndex, 'f', 3);
        }
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
        case 0: return QStringLiteral("颜色");
        case 1: return QStringLiteral("中文名称");
        case 2: return QStringLiteral("位置");
        case 3: return QStringLiteral("容积");
        case 4: return QStringLiteral("全脑占比");
        case 5: return QStringLiteral("不对称");
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
    roles[ColorRole] = "regionColor";
    roles[AsymmetryIndexRole] = "asymmetryIndex";
    return roles;
}

