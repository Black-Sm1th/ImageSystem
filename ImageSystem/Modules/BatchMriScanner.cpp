#include "BatchMriScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

// DCMTK headers
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"

// ============================================================================
// BatchMriScanner Implementation
// ============================================================================

namespace {

QStringList selectProbeFiles(const QStringList& files, int maxProbeCount);

bool hasDicomPreamble(const QString& filePath)
{
    QFile file(filePath);
    if (file.size() < 132)
        return false;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    file.seek(128);
    const QByteArray magic = file.read(4);
    file.close();
    return magic == "DICM";
}

QStringList collectReadableDicomFiles(const QString& dirPath)
{
    QDir dir(dirPath);
    QStringList candidates = dir.entryList({"*.dcm", "*.DCM", "*.ima", "*.IMA", "*.dicom", "*.DICOM"}, QDir::Files, QDir::Name);

    const QStringList allFiles = dir.entryList(QDir::Files, QDir::Name);
    QStringList unnamedFiles;
    for (const QString& fileName : allFiles) {
        if (fileName.contains('.'))
            continue;
        unnamedFiles.append(fileName);
    }

    // For extensionless vendor files, sample a few files to confirm the folder
    // is a DICOM series, then count the whole batch without opening every file.
    if (!unnamedFiles.isEmpty()) {
        const QStringList probeFiles = selectProbeFiles(unnamedFiles, 8);
        bool hasUnnamedDicom = false;
        for (const QString& fileName : probeFiles) {
            if (hasDicomPreamble(dir.absoluteFilePath(fileName))) {
                hasUnnamedDicom = true;
                break;
            }
        }

        if (hasUnnamedDicom)
            candidates.append(unnamedFiles);
    }

    candidates.removeDuplicates();
    return candidates;
}

QStringList selectProbeFiles(const QStringList& files, int maxProbeCount = 8)
{
    if (files.size() <= maxProbeCount)
        return files;

    QStringList selected;
    selected.reserve(maxProbeCount);

    auto appendIfMissing = [&selected, &files](int index) {
        if (index < 0 || index >= files.size())
            return;
        const QString& value = files[index];
        if (!selected.contains(value))
            selected.append(value);
    };

    appendIfMissing(0);
    appendIfMissing(1);
    appendIfMissing(files.size() / 2);
    appendIfMissing(files.size() - 2);
    appendIfMissing(files.size() - 1);

    for (int i = 0; selected.size() < maxProbeCount && i < files.size(); ++i) {
        const int index = (i * files.size()) / maxProbeCount;
        appendIfMissing(index);
    }

    return selected;
}

void fillDicomInfoFromDataset(DcmDataset* dataset, MriSeriesInfo& info)
{
    OFString value;

    if (dataset->findAndGetOFString(DCM_PatientID, value).good())
        info.patientId = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_PatientName, value).good())
        info.patientName = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_PatientSex, value).good())
        info.patientSex = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_PatientBirthDate, value).good())
        info.patientBirthDate = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_StudyDate, value).good())
        info.studyDate = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_StudyInstanceUID, value).good())
        info.studyInstanceUid = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_SeriesInstanceUID, value).good())
        info.seriesInstanceUid = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_SeriesDescription, value).good())
        info.seriesDescription = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_Modality, value).good())
        info.modality = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_ScanningSequence, value).good())
        info.scanningSequence = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_SequenceName, value).good())
        info.sequenceName = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_ProtocolName, value).good())
        info.protocolName = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_ImageType, value).good())
        info.imageType = QString::fromUtf8(value.c_str());

    if (dataset->findAndGetOFString(DCM_MRAcquisitionType, value).good())
        info.mrAcquisitionType = QString::fromUtf8(value.c_str());

    Float64 tr = 0;
    Float64 te = 0;
    if (dataset->findAndGetFloat64(DCM_RepetitionTime, tr).good())
        info.repetitionTime = tr;

    if (dataset->findAndGetFloat64(DCM_EchoTime, te).good())
        info.echoTime = te;

    Sint32 nFrames = 0;
    if (dataset->findAndGetSint32(DCM_NumberOfFrames, nFrames).good() && nFrames > 1) {
        info.numberOfFrames = static_cast<int>(nFrames);
        info.numberOfImages = static_cast<int>(nFrames);
    }
}

bool hasUsefulIdentificationTags(const MriSeriesInfo& info)
{
    return !info.seriesDescription.trimmed().isEmpty()
        || !info.protocolName.trimmed().isEmpty()
        || !info.scanningSequence.trimmed().isEmpty()
        || !info.sequenceName.trimmed().isEmpty()
        || !info.imageType.trimmed().isEmpty();
}

QString formatScanLogLine(const QString& text)
{
    return text.endsWith('\n') ? text : text + '\n';
}

QString prefixedScanLog(const QString& level, const QString& text)
{
    return QStringLiteral("[%1] %2").arg(level, text);
}

QString seriesTypeLabel(MriSeriesInfo::Type type)
{
    switch (type) {
    case MriSeriesInfo::T1W:
        return QStringLiteral("T1W");
    case MriSeriesInfo::BOLD:
        return QStringLiteral("BOLD");
    default:
        return QStringLiteral("未知");
    }
}

QStringList identifySeriesReasons(const MriSeriesInfo& info, int mode)
{
    QStringList reasons;

    const QString modality = info.modality.toUpper().trimmed();
    const QString desc = info.seriesDescription.toUpper();
    const QString proto = info.protocolName.toUpper();
    QString scanSeq = info.scanningSequence.toUpper();
    scanSeq.remove(' ');
    scanSeq.replace('/', '\\');
    const QString imageType = info.imageType.toUpper();

    if (!modality.isEmpty() && modality != QStringLiteral("MR")) {
        reasons << QStringLiteral("模态=%1，不是 MR").arg(info.modality);
        return reasons;
    }

    if (desc.contains(QStringLiteral("T1")) || proto.contains(QStringLiteral("T1")))
        reasons << QStringLiteral("SeriesDescription/ProtocolName 含 T1");

    if (scanSeq.contains(QStringLiteral("GR\\IR"))
            || scanSeq.contains(QStringLiteral("IR\\GR"))
            || scanSeq.contains(QStringLiteral("SE\\IR"))) {
        reasons << QStringLiteral("ScanningSequence=%1 命中结构像规则").arg(info.scanningSequence);
    }

    static const QRegularExpression boldPatterns(
        "(BOLD|FMRI|F[-_]?MRI|RESTING|EPI[-_]?BOLD|FUNCTIONAL|"
        "RS[-_]?FMRI|TASK[-_]?REST|EP2D[-_]?BOLD|REST[-_]?STATE|\\bREST\\b)",
        QRegularExpression::CaseInsensitiveOption
    );

    if (boldPatterns.match(desc).hasMatch())
        reasons << QStringLiteral("SeriesDescription 命中 BOLD 关键词");
    if (boldPatterns.match(proto).hasMatch())
        reasons << QStringLiteral("ProtocolName 命中 BOLD 关键词");
    if (imageType.contains(QStringLiteral("EPI")))
        reasons << QStringLiteral("ImageType 含 EPI");
    if (info.scanningSequence.toUpper().contains(QStringLiteral("EP")))
        reasons << QStringLiteral("ScanningSequence 含 EP");

    if (reasons.isEmpty()) {
        reasons << QStringLiteral("未命中 T1/BOLD 识别规则");
        if (mode == BatchMriScanner::BrainAgeMode)
            reasons << QStringLiteral("当前为仅脑龄模式，只接收 T1W");
    } else if (mode == BatchMriScanner::BrainAgeMode && reasons.join(' ').contains(QStringLiteral("BOLD"))) {
        reasons << QStringLiteral("当前为仅脑龄模式，BOLD 即使识别成功也不会入结果");
    }

    return reasons;
}

}

BatchMriScanner::BatchMriScanner(QObject* parent)
    : QObject(parent)
{
    // 注册自定义类型，用于跨线程信号队列连接
    qRegisterMetaType<ScanProgress>("ScanProgress");
    qRegisterMetaType<MriSeriesInfo>("MriSeriesInfo");
    qRegisterMetaType<MriPairResult>("MriPairResult");
    qRegisterMetaType<QList<MriPairResult>>("QList<MriPairResult>");
}

BatchMriScanner::~BatchMriScanner()
{
    stopScan();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void BatchMriScanner::startScan(const QString& rootPath, int maxDepth, int mode)
{
    if (m_isScanning) {
        emit scanError("扫描正在进行中，请等待完成或停止当前扫描");
        return;
    }

    m_stopRequested = false;
    m_isScanning = true;
    m_results.clear();
    m_progress = ScanProgress();
    m_scanMode = mode;

    // 在后台线程执行扫描
    QtConcurrent::run([this, rootPath, maxDepth, mode]() {
        performScan(rootPath, maxDepth, mode);
        m_isScanning = false;
    });
}

QList<MriPairResult> BatchMriScanner::scanSync(const QString& rootPath, int maxDepth, int mode)
{
    m_stopRequested = false;
    m_isScanning = true;
    m_results.clear();
    m_progress = ScanProgress();
    m_scanMode = mode;

    performScan(rootPath, maxDepth, mode);

    m_isScanning = false;
    return m_results;
}

void BatchMriScanner::stopScan()
{
    m_stopRequested = true;
}

// 判断目录是否含有 DICOM 文件（含无扩展名文件的 GE 格式）
static bool hasDicomFiles(const QString& dirPath)
{
    QDir dir(dirPath);
    // 常见扩展名
    QStringList named = dir.entryList({"*.dcm","*.DCM","*.ima","*.IMA","*.dicom"}, QDir::Files);
    if (!named.isEmpty()) return true;

    // GE 等厂商：文件无扩展名，用数字命名
    // 取前几个无扩展名文件，尝试读取 DICOM magic bytes (DICM at offset 128)
    QStringList allFiles = dir.entryList(QDir::Files);
    int checked = 0;
    for (const QString& f : allFiles) {
        if (f.contains('.')) continue; // 跳过有扩展名的
        QFile file(dir.absoluteFilePath(f));
        if (file.size() < 132) continue;
        if (!file.open(QIODevice::ReadOnly)) continue;
        file.seek(128);
        QByteArray magic = file.read(4);
        file.close();
        if (magic == "DICM") return true;
        if (++checked >= 5) break; // 最多检查5个
    }
    return false;
}

// 判断目录是否含有 NIfTI 文件
static bool hasNiftiFiles(const QString& dirPath)
{
    QDir dir(dirPath);
    return !dir.entryList({"*.nii","*.nii.gz"}, QDir::Files).isEmpty();
}

QStringList BatchMriScanner::collectDicomFolders(const QString& rootPath, int maxDepth)
{
    QStringList folders;

    QList<QPair<QString, int>> queue;
    queue.append({rootPath, 0});

    while (!queue.isEmpty() && !m_stopRequested) {
        auto current = queue.takeFirst();
        QString currentPath = current.first;
        int currentDepth = current.second;

        QDir dir(currentPath);
        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString& subDir : subDirs) {
            QString fullPath = dir.absoluteFilePath(subDir);

            if (hasDicomFiles(fullPath) || hasNiftiFiles(fullPath)) {
                folders.append(fullPath);
            }

            if (currentDepth < maxDepth) {
                queue.append({fullPath, currentDepth + 1});
            }
        }
    }

    return folders;
}

MriSeriesInfo BatchMriScanner::readDicomInfo(const QString& dirPath)
{
    MriSeriesInfo info;
    info.folderPath = dirPath;
    
    QDir dir(dirPath);
    const QStringList files = collectReadableDicomFiles(dirPath);
    
    if (files.isEmpty()) {
        return info;
    }
    
    // 记录图像张数
    info.numberOfImages = files.size();

    QStringList failedFiles;
    MriSeriesInfo fallbackInfo;

    const QStringList probeFiles = selectProbeFiles(files);

    for (const QString& fileName : probeFiles) {
        const QString candidateFile = dir.absoluteFilePath(fileName);

        DcmFileFormat fileformat;
        const OFCondition status = fileformat.loadFile(candidateFile.toLocal8Bit().constData());
        if (status.bad()) {
            failedFiles.append(QStringLiteral("%1 (%2)").arg(candidateFile, QString::fromLocal8Bit(status.text())));
            continue;
        }

        MriSeriesInfo candidateInfo;
        candidateInfo.folderPath = dirPath;
        candidateInfo.sourceFilePath = candidateFile;
        candidateInfo.numberOfImages = info.numberOfImages;
        fillDicomInfoFromDataset(fileformat.getDataset(), candidateInfo);

        if (fallbackInfo.sourceFilePath.isEmpty())
            fallbackInfo = candidateInfo;

        if (hasUsefulIdentificationTags(candidateInfo)) {
            info = candidateInfo;
            break;
        }
    }

    if (info.sourceFilePath.isEmpty() && !fallbackInfo.sourceFilePath.isEmpty())
        info = fallbackInfo;

    if (info.sourceFilePath.isEmpty()) {
        for (const QString& failure : failedFiles)
            qWarning() << "无法读取 DICOM 文件:" << failure;
        return info;
    }

    // 识别序列类型
    info.type = identifySeriesType(info);
    
    return info;
}

MriSeriesInfo::Type BatchMriScanner::identifySeriesType(const MriSeriesInfo& info)
{
    // 只处理 MR 模态（匿名化数据允许空模态）
    QString modality = info.modality.toUpper();
    if (!modality.isEmpty() && modality != "MR") {
        return MriSeriesInfo::Unknown;
    }

    // 合并所有文本字段，提升关键词命中率
    QString desc     = info.seriesDescription.toUpper();
    QString proto    = info.protocolName.toUpper();
    QString seqName  = info.sequenceName.toUpper();
    QString imgType  = info.imageType.toUpper();
    QString scanSeq  = info.scanningSequence.toUpper();  // EP / GR / IR / SE
    QString acqType  = info.mrAcquisitionType.toUpper(); // "2D" / "3D"

    // -----------------------------------------------------------------------
    // 结构项（T1W）硬规则（按需求）：
    // 1) SeriesDescription / ProtocolName 出现 "T1" 即认为是结构项
    // 2) (0018,0020) ScanningSequence 满足 GR\IR、IR\GR、SE\IR 任一也认为是结构项
    // -----------------------------------------------------------------------
    static QRegularExpression t1AnyRe("T1", QRegularExpression::CaseInsensitiveOption);
    if (t1AnyRe.match(desc).hasMatch() || t1AnyRe.match(proto).hasMatch()) {
        return MriSeriesInfo::T1W;
    }

    // DICOM 的多值字段通常用 "\" 分隔，兼容不同格式的分隔符/空格
    QString scanSeqCompact = scanSeq;
    scanSeqCompact.remove(' ');
    scanSeqCompact.replace('/', '\\');
    if (scanSeqCompact.contains("GR\\IR") ||
        scanSeqCompact.contains("IR\\GR") ||
        scanSeqCompact.contains("SE\\IR")) {
        return MriSeriesInfo::T1W;
    }

    // 其余仍按 BOLD 关键词做基本识别（配对模式需要）
    static QRegularExpression boldPatterns(
        "(BOLD|FMRI|F[-_]?MRI|RESTING|EPI[-_]?BOLD|FUNCTIONAL|"
        "RS[-_]?FMRI|TASK[-_]?REST|EP2D[-_]?BOLD|REST[-_]?STATE|"
        "\\bREST\\b)",
        QRegularExpression::CaseInsensitiveOption
    );
    bool descIsBold = boldPatterns.match(desc).hasMatch();
    bool protoIsBold = boldPatterns.match(proto).hasMatch();
    bool imgTypeEPI = imgType.contains("EPI");
    bool hasEPI = scanSeq.contains("EP");
    if (descIsBold || protoIsBold || imgTypeEPI || hasEPI) {
        return MriSeriesInfo::BOLD;
    }

    return MriSeriesInfo::Unknown;
}

// 从 NIfTI 文件名推断 T1W 类型
// NIfTI 没有 DICOM 元数据，只能靠文件名关键词 + 旁路 JSON sidecar
MriSeriesInfo BatchMriScanner::readNiftiInfo(const QString& dirPath)
{
    MriSeriesInfo info;
    info.folderPath = dirPath;
    info.isNifti    = true;
    info.modality   = "MR";

    QDir dir(dirPath);
    QStringList niiFiles = dir.entryList({"*.nii", "*.nii.gz"}, QDir::Files);
    if (niiFiles.isEmpty()) return info;

    // 用图像数量（文件数）作为粗略估计
    info.numberOfImages = niiFiles.size();

    // 优先读取 BIDS JSON sidecar（dcm2niix 生成的 .json 与 .nii 同名）
    for (const QString& nii : niiFiles) {
        QString base = nii;
        base.remove(QRegularExpression("\\.nii(\\.gz)?$", QRegularExpression::CaseInsensitiveOption));
        QString jsonPath = dir.absoluteFilePath(base + ".json");

        if (QFile::exists(jsonPath)) {
            QFile f(jsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
                f.close();

                // 从 JSON 提取关键字段
                if (obj.contains("SeriesDescription"))
                    info.seriesDescription = obj["SeriesDescription"].toString();
                if (obj.contains("ProtocolName"))
                    info.protocolName = obj["ProtocolName"].toString();
                if (obj.contains("SequenceName"))
                    info.sequenceName = obj["SequenceName"].toString();
                if (obj.contains("ImageType")) {
                    QJsonArray arr = obj["ImageType"].toArray();
                    QStringList parts;
                    for (auto v : arr) parts << v.toString();
                    info.imageType = parts.join("\\");
                }
                if (obj.contains("MRAcquisitionType"))
                    info.mrAcquisitionType = obj["MRAcquisitionType"].toString();
                if (obj.contains("RepetitionTime"))
                    info.repetitionTime = obj["RepetitionTime"].toDouble() * 1000.0; // s→ms
                if (obj.contains("EchoTime"))
                    info.echoTime = obj["EchoTime"].toDouble() * 1000.0;             // s→ms
                if (obj.contains("ScanningSequence"))
                    info.scanningSequence = obj["ScanningSequence"].toString();
                if (obj.contains("PatientID"))
                    info.patientId = obj["PatientID"].toString();
                if (obj.contains("AcquisitionDate") || obj.contains("StudyDate"))
                    info.studyDate = obj.contains("StudyDate")
                                     ? obj["StudyDate"].toString()
                                     : obj["AcquisitionDate"].toString();

                break; // 找到第一个有效 JSON 即可
            }
        }

        // 没有 JSON 时，用文件名本身作为 seriesDescription
        if (info.seriesDescription.isEmpty()) {
            info.seriesDescription = base;
        }
    }

    // 用文件夹名补充 seriesDescription（如果还是空）
    if (info.seriesDescription.isEmpty())
        info.seriesDescription = dir.dirName();

    info.type = identifySeriesType(info);
    return info;
}

void BatchMriScanner::performScan(const QString& rootPath, int maxDepth, int mode)
{
    const auto logLine = [this](const QString& text) {
        const QString line = formatScanLogLine(text);
        qDebug().noquote() << line.trimmed();
        emit scanLog(line);
    };

    qDebug() << "\n========== Starting Batch MRI Scan ==========";
    qDebug() << "Root Path:" << rootPath;
    qDebug() << "Max Depth:" << maxDepth;
    qDebug() << "Scan Mode:" << (mode == 0 ? "Pairing Mode (T1W+BOLD)" : "Brain Age Mode (T1W only)");
    logLine(prefixedScanLog(QStringLiteral("PHASE"), QStringLiteral("========== 开始硬盘扫描 ==========")));
    logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("扫描根目录：%1").arg(rootPath)));
    logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("最大扫描深度：%1").arg(maxDepth)));
    logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("扫描模式：%1").arg(mode == 0 ? QStringLiteral("完整配对模式（T1W + BOLD）")
                                                                                              : QStringLiteral("仅脑龄模式（只接收 T1W）"))));
    
    // Step 1: Collect folders
    qDebug() << "\n[Step 1] Counting DICOM folders...";
    logLine(prefixedScanLog(QStringLiteral("STEP1"), QStringLiteral("收集候选目录中...")));
    QStringList folders = collectDicomFolders(rootPath, maxDepth);
    
    if (m_stopRequested) {
        qDebug() << "Scan cancelled";
        logLine(prefixedScanLog(QStringLiteral("WARN"), QStringLiteral("扫描已取消")));
        emit scanError("Scan cancelled");
        return;
    }
    
    m_progress.totalFolders = folders.size();
    qDebug() << "Found" << m_progress.totalFolders << "DICOM folders";
    logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("候选目录数：%1").arg(m_progress.totalFolders)));
    
    if (folders.isEmpty()) {
        qDebug() << "No DICOM folders found";
        logLine(prefixedScanLog(QStringLiteral("WARN"), QStringLiteral("未找到包含 DICOM/NIfTI 的候选目录，本次扫描结束")));
        emit scanFinished(m_results);
        return;
    }
    
    // Step 2: Identify and Pair
    qDebug() << "\n[Step 2] Identifying series types...";
    logLine(prefixedScanLog(QStringLiteral("STEP2"), QStringLiteral("逐个目录识别序列类型...")));
    
    QMap<QString, MriPairResult> pairMap;
    
    for (int i = 0; i < folders.size(); ++i) {
        if (m_stopRequested) {
            qDebug() << "Scan cancelled";
            emit scanError("Scan cancelled");
            return;
        }
        
        const QString& folder = folders[i];
        m_progress.scannedFolders = i + 1;
        m_progress.currentFolder = folder;
        
        MriSeriesInfo info = hasNiftiFiles(folder) && !hasDicomFiles(folder)
                             ? readNiftiInfo(folder)
                             : readDicomInfo(folder);
        const QStringList reasons = identifySeriesReasons(info, mode);
        
        // Debug: Print every folder's info
        QString folderName = QDir(folder).dirName();
        qDebug() << QString("[%1/%2] Folder: %3 | Desc: %4")
                    .arg(i + 1).arg(folders.size())
                    .arg(folderName)
                    .arg(info.seriesDescription.isEmpty() ? "(empty)" : info.seriesDescription);
        qDebug() << QString("      Params: TE=%1, TR=%2, Count=%3, Modality=%4")
                    .arg(info.echoTime)
                    .arg(info.repetitionTime)
                    .arg(info.numberOfImages)
                    .arg(info.modality.isEmpty() ? "(empty)" : info.modality);
        qDebug() << QString("      Source: %1").arg(info.sourceFilePath.isEmpty() ? "(empty)" : info.sourceFilePath);
        qDebug() << QString("      Tags: ScanSeq=%1 | Protocol=%2 | Sequence=%3 | ImageType=%4")
                    .arg(info.scanningSequence.isEmpty() ? "(empty)" : info.scanningSequence)
                    .arg(info.protocolName.isEmpty() ? "(empty)" : info.protocolName)
                    .arg(info.sequenceName.isEmpty() ? "(empty)" : info.sequenceName)
                    .arg(info.imageType.isEmpty() ? "(empty)" : info.imageType);
        qDebug() << QString("      Identity: PatientID=%1, StudyDate=%2")
                    .arg(info.patientId.isEmpty() ? "(empty)" : info.patientId)
                    .arg(info.studyDate.isEmpty() ? "(empty)" : info.studyDate);

        // ========== Generate Pair Key ==========
        // 脑龄模式下为了避免同一天多个 T1 序列被误合并，优先把 SeriesInstanceUID 纳入 key。
        // 完整配对模式仍优先按 PatientID + StudyDate 聚合，便于 T1/BOLD 归并到同一病例。
        QString pairKey;
        
        if (mode == BrainAgeMode
                && !info.patientId.isEmpty()
                && !info.studyDate.isEmpty()
                && !info.seriesInstanceUid.isEmpty()) {
            pairKey = info.patientId + "_" + info.studyDate + "_" + info.seriesInstanceUid;
            qDebug() << QString("      PairKey: %1 (by PatientID+StudyDate+SeriesInstanceUID)").arg(pairKey);
        } else if (!info.patientId.isEmpty() && !info.studyDate.isEmpty()) {
            pairKey = info.patientId + "_" + info.studyDate;
            qDebug() << QString("      PairKey: %1 (by PatientID+StudyDate)").arg(pairKey);
        } else if (!info.patientId.isEmpty()) {
            pairKey = info.patientId;
            qDebug() << QString("      PairKey: %1 (by PatientID only)").arg(pairKey);
        } else {
            pairKey = folderName.section('_', 0, 0);
            if (pairKey.isEmpty()) {
                pairKey = folderName;
            }
            qDebug() << QString("      PairKey: %1 (by FolderName - FALLBACK)").arg(pairKey);
        }

        QString pairKeyReason;
        if (mode == BrainAgeMode
                && !info.patientId.isEmpty()
                && !info.studyDate.isEmpty()
                && !info.seriesInstanceUid.isEmpty())
            pairKeyReason = QStringLiteral("patientId + studyDate + seriesInstanceUid");
        else if (!info.patientId.isEmpty() && !info.studyDate.isEmpty())
            pairKeyReason = QStringLiteral("patientId + studyDate");
        else if (!info.patientId.isEmpty())
            pairKeyReason = QStringLiteral("patientId");
        else
            pairKeyReason = QStringLiteral("文件夹名回退");
        
        if (info.type == MriSeriesInfo::T1W) {
            m_progress.foundT1Count++;

            // Use folder name as ID if patientId is empty
            pairMap[pairKey].patientId = info.patientId.isEmpty() ? pairKey : info.patientId;
            pairMap[pairKey].patientName = info.patientName.isEmpty() ? folderName : info.patientName;
            pairMap[pairKey].patientSex = info.patientSex;
            pairMap[pairKey].patientBirthDate = info.patientBirthDate;
            pairMap[pairKey].studyDate = info.studyDate;
            pairMap[pairKey].t1SeriesUid = info.seriesInstanceUid;
            pairMap[pairKey].t1Path = folder;
            pairMap[pairKey].t1SeriesDesc = info.seriesDescription;
            pairMap[pairKey].t1ImageCount = info.numberOfImages;
            pairMap[pairKey].scanMode = mode;

            qDebug() << "      >>> Identified as T1W";
            logLine(prefixedScanLog(QStringLiteral("CASE"), QStringLiteral("[%1/%2] 目录：%3")
                                    .arg(i + 1).arg(folders.size()).arg(folder)));
            logLine(prefixedScanLog(QStringLiteral("OK"), QStringLiteral("识别结果：成功，类型=T1W")));
            logLine(prefixedScanLog(QStringLiteral("MATCH"), QStringLiteral("命中条件：%1").arg(reasons.join(QStringLiteral("；")))));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("识别源文件：%1").arg(info.sourceFilePath.isEmpty() ? QStringLiteral("(empty)") : info.sourceFilePath)));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("关键标签：ScanningSequence=%1；ProtocolName=%2；SequenceName=%3；ImageType=%4")
                                    .arg(info.scanningSequence.isEmpty() ? QStringLiteral("(empty)") : info.scanningSequence)
                                    .arg(info.protocolName.isEmpty() ? QStringLiteral("(empty)") : info.protocolName)
                                    .arg(info.sequenceName.isEmpty() ? QStringLiteral("(empty)") : info.sequenceName)
                                    .arg(info.imageType.isEmpty() ? QStringLiteral("(empty)") : info.imageType)));
            logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("配对键：%1（来源：%2）").arg(pairKey, pairKeyReason)));
            emit seriesFound(info);
        }
        else if (info.type == MriSeriesInfo::BOLD && mode == 0) {
            // 仅在配对模式下处理 BOLD
            m_progress.foundBoldCount++;

            // Use folder name as ID if patientId is empty
            pairMap[pairKey].patientId = info.patientId.isEmpty() ? pairKey : info.patientId;
            pairMap[pairKey].patientName = info.patientName.isEmpty() ? folderName : info.patientName;
            pairMap[pairKey].patientSex = info.patientSex;
            pairMap[pairKey].patientBirthDate = info.patientBirthDate;
            pairMap[pairKey].studyDate = info.studyDate;
            pairMap[pairKey].boldSeriesUid = info.seriesInstanceUid;
            pairMap[pairKey].boldPath = folder;
            pairMap[pairKey].boldSeriesDesc = info.seriesDescription;
            pairMap[pairKey].boldImageCount = info.numberOfImages;
            pairMap[pairKey].scanMode = mode;

            qDebug() << "      >>> Identified as BOLD";
            logLine(prefixedScanLog(QStringLiteral("CASE"), QStringLiteral("[%1/%2] 目录：%3")
                                    .arg(i + 1).arg(folders.size()).arg(folder)));
            logLine(prefixedScanLog(QStringLiteral("OK"), QStringLiteral("识别结果：成功，类型=BOLD")));
            logLine(prefixedScanLog(QStringLiteral("MATCH"), QStringLiteral("命中条件：%1").arg(reasons.join(QStringLiteral("；")))));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("识别源文件：%1").arg(info.sourceFilePath.isEmpty() ? QStringLiteral("(empty)") : info.sourceFilePath)));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("关键标签：ScanningSequence=%1；ProtocolName=%2；SequenceName=%3；ImageType=%4")
                                    .arg(info.scanningSequence.isEmpty() ? QStringLiteral("(empty)") : info.scanningSequence)
                                    .arg(info.protocolName.isEmpty() ? QStringLiteral("(empty)") : info.protocolName)
                                    .arg(info.sequenceName.isEmpty() ? QStringLiteral("(empty)") : info.sequenceName)
                                    .arg(info.imageType.isEmpty() ? QStringLiteral("(empty)") : info.imageType)));
            logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("配对键：%1（来源：%2）").arg(pairKey, pairKeyReason)));
            emit seriesFound(info);
        }
        else {
            qDebug() << "      --- Ignored (Not T1W or BOLD)";
            logLine(prefixedScanLog(QStringLiteral("CASE"), QStringLiteral("[%1/%2] 目录：%3")
                                    .arg(i + 1).arg(folders.size()).arg(folder)));
            logLine(prefixedScanLog(QStringLiteral("SKIP"), QStringLiteral("识别结果：忽略，类型=%1").arg(seriesTypeLabel(info.type))));
            logLine(prefixedScanLog(QStringLiteral("REASON"), QStringLiteral("原因：%1").arg(reasons.join(QStringLiteral("；")))));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("识别源文件：%1").arg(info.sourceFilePath.isEmpty() ? QStringLiteral("(empty)") : info.sourceFilePath)));
            logLine(prefixedScanLog(QStringLiteral("META"), QStringLiteral("关键标签：ScanningSequence=%1；ProtocolName=%2；SequenceName=%3；ImageType=%4")
                                    .arg(info.scanningSequence.isEmpty() ? QStringLiteral("(empty)") : info.scanningSequence)
                                    .arg(info.protocolName.isEmpty() ? QStringLiteral("(empty)") : info.protocolName)
                                    .arg(info.sequenceName.isEmpty() ? QStringLiteral("(empty)") : info.sequenceName)
                                    .arg(info.imageType.isEmpty() ? QStringLiteral("(empty)") : info.imageType)));
            if (info.numberOfImages <= 0) {
                logLine(prefixedScanLog(QStringLiteral("REASON"), QStringLiteral("补充说明：目录中未读取到有效影像文件，或候选 DICOM 元数据均解析失败")));
            }
        }
        
        emit progressUpdated(m_progress);
    }
    
    // Step 3: Extract pairs
    qDebug() << "\n[Step 3] Generating pair results...";
    logLine(prefixedScanLog(QStringLiteral("STEP3"), QStringLiteral("汇总病例并判断是否满足入结果条件...")));
    
    for (auto it = pairMap.begin(); it != pairMap.end(); ++it) {
        if (it.value().isComplete()) {
            m_results.append(it.value());
            m_progress.pairedCount++;
            logLine(prefixedScanLog(QStringLiteral("PASS"), QStringLiteral("病例 %1：纳入扫描结果，原因=%2")
                                    .arg(it.key(),
                                         mode == BrainAgeMode
                                         ? QStringLiteral("已找到 T1W，满足仅脑龄模式要求")
                                         : QStringLiteral("同时找到 T1W 和 BOLD，满足完整配对要求"))));
            logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("T1W：%1").arg(it.value().t1Path.isEmpty() ? QStringLiteral("无") : it.value().t1Path)));
            if (mode == PairingMode)
                logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("BOLD：%1").arg(it.value().boldPath.isEmpty() ? QStringLiteral("无") : it.value().boldPath)));
        } else {
            QStringList missing;
            if (it.value().t1Path.isEmpty())
                missing << QStringLiteral("缺少 T1W");
            if (mode == PairingMode && it.value().boldPath.isEmpty())
                missing << QStringLiteral("缺少 BOLD");
            logLine(prefixedScanLog(QStringLiteral("FAIL"), QStringLiteral("病例 %1：未纳入扫描结果，原因=%2")
                                    .arg(it.key(), missing.join(QStringLiteral("，")))));
            logLine(prefixedScanLog(QStringLiteral("INFO"), QStringLiteral("当前已有：T1W=%1，BOLD=%2")
                                    .arg(it.value().t1Path.isEmpty() ? QStringLiteral("否") : QStringLiteral("是"))
                                    .arg(it.value().boldPath.isEmpty() ? QStringLiteral("否") : QStringLiteral("是"))));
        }
    }
    
    qDebug() << "\n========== Scan Finished ==========";
    qDebug() << "Total Folders:" << m_progress.totalFolders;
    qDebug() << "Found T1W:" << m_progress.foundT1Count;
    qDebug() << "Found BOLD:" << m_progress.foundBoldCount;
    qDebug() << "Paired Success:" << m_progress.pairedCount;
    qDebug() << "==================================\n";
    logLine(prefixedScanLog(QStringLiteral("PHASE"), QStringLiteral("========== 扫描完成 ==========")));
    logLine(prefixedScanLog(QStringLiteral("SUMMARY"), QStringLiteral("候选目录：%1").arg(m_progress.totalFolders)));
    logLine(prefixedScanLog(QStringLiteral("SUMMARY"), QStringLiteral("识别到 T1W：%1").arg(m_progress.foundT1Count)));
    logLine(prefixedScanLog(QStringLiteral("SUMMARY"), QStringLiteral("识别到 BOLD：%1").arg(m_progress.foundBoldCount)));
    logLine(prefixedScanLog(QStringLiteral("SUMMARY"), QStringLiteral("最终纳入结果：%1").arg(m_progress.pairedCount)));
    
    emit progressUpdated(m_progress);
    emit scanFinished(m_results);
}

