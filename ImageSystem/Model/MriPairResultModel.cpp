#include "MriPairResultModel.h"
#include <QDebug>

MriPairResultModel::MriPairResultModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void MriPairResultModel::loadResults(const QList<MriPairResult>& results)
{
    beginResetModel();
    m_results = results;
    m_checkedStates.clear();
    m_checkedStates.resize(results.size());
    endResetModel();
    
    emit checkedCountChanged();
    emit resultCountChanged();
    qDebug() << QStringLiteral("MRI配对结果模型加载了") << m_results.size() << QStringLiteral("条记录");
}

void MriPairResultModel::clear()
{
    beginResetModel();
    m_results.clear();
    m_checkedStates.clear();
    endResetModel();
    
    emit checkedCountChanged();
    emit resultCountChanged();
}

void MriPairResultModel::setChecked(int row, bool checked)
{
    if (row < 0 || row >= m_checkedStates.size())
        return;
    
    if (m_checkedStates[row] != checked) {
        m_checkedStates[row] = checked;
        QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {IsCheckedRole});
        emit checkedCountChanged();
    }
}

bool MriPairResultModel::isChecked(int row) const
{
    if (row < 0 || row >= m_checkedStates.size())
        return false;
    return m_checkedStates[row];
}

void MriPairResultModel::toggleChecked(int row)
{
    if (row < 0 || row >= m_checkedStates.size())
        return;
    setChecked(row, !m_checkedStates[row]);
}

void MriPairResultModel::selectAll()
{
    for (int i = 0; i < m_checkedStates.size(); ++i) {
        m_checkedStates[i] = true;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsCheckedRole});
    emit checkedCountChanged();
}

void MriPairResultModel::deselectAll()
{
    for (int i = 0; i < m_checkedStates.size(); ++i) {
        m_checkedStates[i] = false;
    }
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {IsCheckedRole});
    emit checkedCountChanged();
}

QList<MriPairResult> MriPairResultModel::getCheckedResults() const
{
    QList<MriPairResult> checkedResults;
    for (int i = 0; i < m_results.size(); ++i) {
        if (m_checkedStates[i]) {
            checkedResults.append(m_results[i]);
        }
    }
    return checkedResults;
}

int MriPairResultModel::checkedCount() const
{
    int count = 0;
    for (bool checked : m_checkedStates) {
        if (checked) count++;
    }
    return count;
}

int MriPairResultModel::resultCount() const
{
    return m_results.size();
}

int MriPairResultModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_results.size();
}

int MriPairResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 7;  // 勾选、患者ID、姓名、性别、出生日期、检查日期、状态
}

QVariant MriPairResultModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return QVariant();

    const MriPairResult& result = m_results[index.row()];

    // 用于 QML TableView 的角色
    switch (role) {
    case PatientIdRole:
        return result.patientId;
    case PatientNameRole:
        return result.patientName;
    case PatientSexRole:
        return result.patientSex;
    case PatientBirthDateRole:
        return result.patientBirthDate;
    case StudyDateRole:
        return result.studyDate;
    case T1PathRole:
        return result.t1Path;
    case T1SeriesDescRole:
        return result.t1SeriesDesc;
    case T1ImageCountRole:
        return result.t1ImageCount;
    case BoldPathRole:
        return result.boldPath;
    case BoldSeriesDescRole:
        return result.boldSeriesDesc;
    case BoldImageCountRole:
        return result.boldImageCount;
    case IsCompleteRole:
        return result.isComplete();
    case IsCheckedRole:
        return m_checkedStates[index.row()];
    default:
        break;
    }

    // 用于传统 TableView 的 DisplayRole
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return m_checkedStates[index.row()];  // 勾选状态
        case 1: return result.patientId;
        case 2: return result.patientName;
        case 3: return result.patientSex;
        case 4: return result.patientBirthDate;
        case 5: return result.studyDate;
        case 6: return result.isComplete() ? QStringLiteral("完整") : QStringLiteral("不完整");
        default: return QVariant();
        }
    }

    if (role == Qt::CheckStateRole && index.column() == 0) {
        return m_checkedStates[index.row()] ? Qt::Checked : Qt::Unchecked;
    }

    return QVariant();
}

bool MriPairResultModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_results.size())
        return false;

    if (role == IsCheckedRole || (role == Qt::CheckStateRole && index.column() == 0)) {
        bool checked = value.toBool();
        if (role == Qt::CheckStateRole) {
            checked = (value.toInt() == Qt::Checked);
        }
        setChecked(index.row(), checked);
        return true;
    }

    return false;
}

QVariant MriPairResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return QStringLiteral("选择");
        case 1: return QStringLiteral("患者ID");
        case 2: return QStringLiteral("姓名");
        case 3: return QStringLiteral("性别");
        case 4: return QStringLiteral("出生日期");
        case 5: return QStringLiteral("检查日期");
        case 6: return QStringLiteral("状态");
        default: return QVariant();
        }
    } else {
        return section + 1;  // 行号从1开始
    }
}

QHash<int, QByteArray> MriPairResultModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PatientIdRole] = "patientId";
    roles[PatientNameRole] = "patientName";
    roles[PatientSexRole] = "patientSex";
    roles[PatientBirthDateRole] = "patientBirthDate";
    roles[StudyDateRole] = "studyDate";
    roles[T1PathRole] = "t1Path";
    roles[T1SeriesDescRole] = "t1SeriesDesc";
    roles[T1ImageCountRole] = "t1ImageCount";
    roles[BoldPathRole] = "boldPath";
    roles[BoldSeriesDescRole] = "boldSeriesDesc";
    roles[BoldImageCountRole] = "boldImageCount";
    roles[IsCompleteRole] = "isComplete";
    roles[IsCheckedRole] = "isChecked";
    return roles;
}

Qt::ItemFlags MriPairResultModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);
    if (index.column() == 0) {
        return defaultFlags | Qt::ItemIsUserCheckable;
    }
    return defaultFlags;
}

