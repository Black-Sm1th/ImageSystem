#include "AppDataModel.h"
#include <QDate>
#include <QRegularExpression>

namespace {

QString normalizeWhitespace(const QString& value)
{
    QString normalized = value.trimmed();
    normalized.replace('^', ' ');
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized.toLower();
}

QString normalizeDateKey(const QString& value)
{
    const QString text = value.trimmed();
    if (text.isEmpty())
        return {};

    QDate date = QDate::fromString(text, QStringLiteral("yyyyMMdd"));
    if (!date.isValid())
        date = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid())
        date = QDate::fromString(text, Qt::ISODate);
    if (date.isValid())
        return date.toString(QStringLiteral("yyyyMMdd"));

    QString digits = text;
    digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
    return digits.left(8);
}

bool looksLikeDicomUid(const QString& value)
{
    static const QRegularExpression re(QStringLiteral("^\\d+(?:\\.\\d+)+$"));
    return re.match(value.trimmed()).hasMatch();
}

bool isSameCaseIdentity(const QString& existingName,
                        const QString& existingExamDate,
                        const QString& existingSeriesUid,
                        const QString& incomingName,
                        const QString& incomingExamDate,
                        const QString& incomingSeriesUid)
{
    const QString lhsUid = existingSeriesUid.trimmed();
    const QString rhsUid = incomingSeriesUid.trimmed();
    const bool lhsDicomUid = looksLikeDicomUid(lhsUid);
    const bool rhsDicomUid = looksLikeDicomUid(rhsUid);

    if (lhsDicomUid && rhsDicomUid)
        return lhsUid == rhsUid;

    if (!lhsUid.isEmpty() && !rhsUid.isEmpty() && lhsUid == rhsUid)
        return true;

    const QString lhsName = normalizeWhitespace(existingName);
    const QString rhsName = normalizeWhitespace(incomingName);
    const QString lhsDate = normalizeDateKey(existingExamDate);
    const QString rhsDate = normalizeDateKey(incomingExamDate);

    return !lhsName.isEmpty()
        && !lhsDate.isEmpty()
        && lhsName == rhsName
        && lhsDate == rhsDate;
}

}

AppDataModel::AppDataModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AppDataModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_records.size();
}

QVariant AppDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_records.size())
        return {};
    const auto &r = m_records[index.row()];
    switch (role) {
    case NameRole:       return r.name;
    case PatientIdRole:  return r.patientId;
    case ExamDateRole:   return r.examDate;
    case StatusRole:     return r.status;
    case SourceRole:     return r.source;
    case SexRole:        return r.sex;
    case AgeRole:        return r.age;
    case CheckTypeRole:  return r.checkType;
    case SliceCountRole: return r.sliceCount;
    case OutputPathRole: return r.outputPath;
    case BidsPathRole:   return r.bidsPath;
    case DbIdRole:       return r.dbId;
    default: return {};
    }
}

QHash<int, QByteArray> AppDataModel::roleNames() const
{
    return {
        { NameRole,       "name" },
        { PatientIdRole,  "patientId" },
        { ExamDateRole,   "examDate" },
        { StatusRole,     "status" },
        { SourceRole,     "source" },
        { SexRole,        "sex" },
        { AgeRole,        "age" },
        { CheckTypeRole,  "checkType" },
        { SliceCountRole, "sliceCount" },
        { OutputPathRole, "outputPath" },
        { BidsPathRole,   "bidsPath" },
        { DbIdRole,       "dbId" }
    };
}

void AppDataModel::refresh()
{
    rebuildRecords();
}

QVariantList AppDataModel::records() const
{
    QVariantList list;
    for (const auto &r : m_records) {
        QVariantMap row;
        row.insert("name", r.name);
        row.insert("patientId", r.patientId);
        row.insert("examDate", r.examDate);
        row.insert("status", r.status);
        row.insert("source", r.source);
        row.insert("sex", r.sex);
        row.insert("age", r.age);
        row.insert("checkType", r.checkType);
        row.insert("sliceCount", r.sliceCount);
        row.insert("outputPath", r.outputPath);
        row.insert("bidsPath", r.bidsPath);
        row.insert("seriesUid", r.seriesUid);
        row.insert("dbId", r.dbId);
        list.append(row);
    }
    return list;
}

void AppDataModel::setPendingItems(const QList<HardwareScanResult> &items)
{
    m_pendingItems = items;
    rebuildRecords();
}

void AppDataModel::addPendingItem(const HardwareScanResult &item)
{
    for (const auto &existing : m_pendingItems) {
        if (isSameCaseIdentity(existing.name, existing.examDate, existing.seriesUid,
                               item.name, item.examDate, item.seriesUid)) {
            return;
        }
    }

    for (const auto &existing : m_completedItems) {
        if (isSameCaseIdentity(existing.value(QStringLiteral("name")).toString(),
                               existing.value(QStringLiteral("exam_date")).toString(),
                               existing.value(QStringLiteral("series_uid")).toString(),
                               item.name,
                               item.examDate,
                               item.seriesUid)) {
            return;
        }
    }
    m_pendingItems.append(item);
    rebuildRecords();
}

void AppDataModel::removePendingItem(const QString &name, const QString &examDate, const QString &seriesUid)
{
    for (int i = m_pendingItems.size() - 1; i >= 0; --i) {
        const auto &item = m_pendingItems[i];
        if (isSameCaseIdentity(item.name, item.examDate, item.seriesUid,
                               name, examDate, seriesUid)) {
            m_pendingItems.removeAt(i);
        }
    }
    rebuildRecords();
}

void AppDataModel::setPendingItemStatus(const QString &name, const QString &examDate, const QString &seriesUid, const QString &status)
{
    bool changed = false;
    for (auto &item : m_pendingItems) {
        if (!isSameCaseIdentity(item.name, item.examDate, item.seriesUid,
                                name, examDate, seriesUid)) {
            continue;
        }
        if (item.status == status)
            continue;
        item.status = status;
        changed = true;
    }

    if (changed)
        rebuildRecords();
}

void AppDataModel::clearPendingItems()
{
    m_pendingItems.clear();
    rebuildRecords();
}

void AppDataModel::setCompletedItems(const QList<QVariantMap> &items)
{
    m_completedItems = items;
    rebuildRecords();
}

QString AppDataModel::matchingRecordStatus(const QString &name, const QString &examDate, const QString &seriesUid) const
{
    for (const auto &item : m_pendingItems) {
        if (isSameCaseIdentity(item.name, item.examDate, item.seriesUid, name, examDate, seriesUid))
            return item.status.isEmpty() ? QStringLiteral("processing") : item.status;
    }

    for (const auto &item : m_completedItems) {
        if (isSameCaseIdentity(item.value(QStringLiteral("name")).toString(),
                               item.value(QStringLiteral("exam_date")).toString(),
                               item.value(QStringLiteral("series_uid")).toString(),
                               name,
                               examDate,
                               seriesUid)) {
            return item.value(QStringLiteral("status"), QStringLiteral("completed")).toString();
        }
    }

    return {};
}

void AppDataModel::rebuildRecords()
{
    beginResetModel();
    m_records.clear();

    for (const auto &p : m_pendingItems) {
        UnifiedRecord r;
        r.source     = UnifiedRecord::Processing;
        r.name       = p.name;
        r.patientId  = p.patientId;
        r.examDate   = p.examDate;
        r.checkType  = p.checkType;
        r.sex        = p.sex;
        r.age        = p.age;
        r.seriesUid  = p.seriesUid;
        r.sliceCount = p.sliceCount;
        r.status     = p.status.isEmpty() ? QStringLiteral("processing") : p.status;
        r.outputPath = p.outputPath;
        r.bidsPath   = p.bidsPath;
        m_records.append(r);
    }

    for (const auto &c : m_completedItems) {
        UnifiedRecord r;
        r.source     = UnifiedRecord::Completed;
        r.dbId       = c.value("id").toInt();
        r.name       = c.value("name").toString();
        r.patientId  = c.value("patient_id").toString();
        r.examDate   = c.value("exam_date").toString();
        r.seriesUid  = c.value("series_uid").toString();
        r.age        = c.value("age").toInt();
        r.sex        = c.value("sex").toString();
        r.checkType  = c.value("check_type").toString();
        r.status     = c.value("status", "completed").toString();
        r.bidsPath   = c.value("bids_path").toString();
        r.outputPath = c.value("output_path").toString();
        m_records.append(r);
    }

    endResetModel();
}
