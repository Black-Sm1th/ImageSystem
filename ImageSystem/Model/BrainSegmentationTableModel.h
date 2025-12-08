#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QString>

// 脑区分割数据结构
struct SegmentationRegion {
    QString chineseName;    // 中文名称
    QString hemisphere;     // 位置（L/R/N）
    double volume;          // 容积
    double volumePercent;   // 全脑占比
    int label;              // 标签值
    double colorR;          // 红色分量 (0-1)
    double colorG;          // 绿色分量 (0-1)
    double colorB;          // 蓝色分量 (0-1)
    double colorA;          // 透明度分量 (0-1)
    int partnerLabel;       // 配对半球的label值，-1表示无配对
    double asymmetryIndex;  // 不对称指数（仅配对脑区有效）
    bool visible;           // 是否可见
};

class BrainSegmentationTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum ColumnRoles {
        ChineseNameRole = Qt::UserRole + 1,
        HemisphereRole,
        VolumeRole,
        VolumePercentRole,
        LabelRole,
        ColorRole,
        AsymmetryIndexRole,
        VisibleRole
    };

    explicit BrainSegmentationTableModel(QObject* parent = nullptr);

    // 加载数据
    void loadRegions(const QVector<SegmentationRegion>& regions);
    void clear();
    
    // 更新可见性状态
    void setRegionVisible(int row, bool visible);

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<SegmentationRegion> m_regions;
};

