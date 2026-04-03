#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include "Modules/BatchMriScanner.h"

/**
 * @brief MRI 配对结果表格模型
 * 
 * 用于在 QML 中显示扫描配对结果，支持勾选功能
 */
class MriPairResultModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedCountChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)

public:
    enum ColumnRoles {
        PatientIdRole = Qt::UserRole + 1,
        PatientNameRole,
        PatientSexRole,
        PatientBirthDateRole,
        StudyDateRole,
        T1PathRole,
        T1SeriesDescRole,
        T1ImageCountRole,
        BoldPathRole,
        BoldSeriesDescRole,
        BoldImageCountRole,
        IsCompleteRole,
        IsCheckedRole,
        PredictedBrainAgeRole,
        ScanModeRole
    };

    explicit MriPairResultModel(QObject* parent = nullptr);

    // 加载扫描结果
    void loadResults(const QList<MriPairResult>& results);
    void clear();

    // 勾选相关
    Q_INVOKABLE void setChecked(int row, bool checked);
    Q_INVOKABLE bool isChecked(int row) const;
    Q_INVOKABLE void toggleChecked(int row);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselectAll();
    Q_INVOKABLE QList<MriPairResult> getCheckedResults() const;
    int checkedCount() const;
    int resultCount() const;

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void checkedCountChanged();
    void resultCountChanged();

private:
    QList<MriPairResult> m_results;
    QVector<bool> m_checkedStates;
};

