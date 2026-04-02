#include "BatchMriScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QRegularExpression>

// DCMTK headers
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"

// ============================================================================
// BatchMriScanner Implementation
// ============================================================================

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

void BatchMriScanner::startScan(const QString& rootPath, int maxDepth)
{
    if (m_isScanning) {
        emit scanError("扫描正在进行中，请等待完成或停止当前扫描");
        return;
    }

    m_stopRequested = false;
    m_isScanning = true;
    m_results.clear();
    m_progress = ScanProgress();

    // 在后台线程执行扫描
    QtConcurrent::run([this, rootPath, maxDepth]() {
        performScan(rootPath, maxDepth);
        m_isScanning = false;
    });
}

QList<MriPairResult> BatchMriScanner::scanSync(const QString& rootPath, int maxDepth)
{
    m_stopRequested = false;
    m_isScanning = true;
    m_results.clear();
    m_progress = ScanProgress();

    performScan(rootPath, maxDepth);
    
    m_isScanning = false;
    return m_results;
}

void BatchMriScanner::stopScan()
{
    m_stopRequested = true;
}

QStringList BatchMriScanner::collectDicomFolders(const QString& rootPath, int maxDepth)
{
    QStringList folders;
    
    // 使用 BFS 方式遍历，控制深度
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
            
            // 检查是否包含 DICOM 文件
            QDir subDirObj(fullPath);
            QStringList dcmFiles = subDirObj.entryList({"*.dcm", "*.DCM", "*.ima", "*.IMA"}, QDir::Files);
            
            if (!dcmFiles.isEmpty()) {
                folders.append(fullPath);
            }
            
            // 继续向下搜索（如果未达到最大深度）
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
    QStringList files = dir.entryList({"*.dcm", "*.DCM", "*.ima", "*.IMA"}, QDir::Files);
    
    if (files.isEmpty()) {
        return info;
    }
    
    // 记录图像张数
    info.numberOfImages = files.size();
    
    // 读取第一个文件获取元数据
    QString firstFile = dir.absoluteFilePath(files.first());
    
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(firstFile.toLocal8Bit().constData());
    
    if (status.bad()) {
        qWarning() << "无法读取 DICOM 文件:" << firstFile << status.text();
        return info;
    }
    
    DcmDataset* dataset = fileformat.getDataset();
    OFString value;
    
    // 读取患者信息
    if (dataset->findAndGetOFString(DCM_PatientID, value).good())
        info.patientId = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_PatientName, value).good())
        info.patientName = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_PatientSex, value).good())
        info.patientSex = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_PatientBirthDate, value).good())
        info.patientBirthDate = QString::fromUtf8(value.c_str());
    
    // 读取检查信息
    if (dataset->findAndGetOFString(DCM_StudyDate, value).good())
        info.studyDate = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_StudyInstanceUID, value).good())
        info.studyInstanceUid = QString::fromUtf8(value.c_str());
    
    // 读取序列信息
    if (dataset->findAndGetOFString(DCM_SeriesInstanceUID, value).good())
        info.seriesInstanceUid = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_SeriesDescription, value).good())
        info.seriesDescription = QString::fromUtf8(value.c_str());
    
    if (dataset->findAndGetOFString(DCM_Modality, value).good())
        info.modality = QString::fromUtf8(value.c_str());
    
    // Read Physical Parameters for better identification
    if (dataset->findAndGetOFString(DCM_ScanningSequence, value).good())
        info.scanningSequence = QString::fromUtf8(value.c_str());
    
    Float64 tr = 0, te = 0;
    if (dataset->findAndGetFloat64(DCM_RepetitionTime, tr).good())
        info.repetitionTime = tr;
    
    if (dataset->findAndGetFloat64(DCM_EchoTime, te).good())
        info.echoTime = te;
    
    // 识别序列类型
    info.type = identifySeriesType(info);
    
    return info;
}

MriSeriesInfo::Type BatchMriScanner::identifySeriesType(const MriSeriesInfo& info)
{
    // Only process MR modality (but allow empty modality for anonymized data)
    QString modality = info.modality.toUpper();
    if (!modality.isEmpty() && modality != "MR") {
        return MriSeriesInfo::Unknown;
    }
    
    QString desc = info.seriesDescription.toUpper();
    QString seq = info.scanningSequence.toUpper();
    
    // --- Step 1: Identification by Keywords (Primary) ---
    
    static QRegularExpression t1Patterns(
        "(MPRAGE|3D\\s*T1|T1\\s*W|T1[-_]?MPRAGE|SPGR|BRAVO|IR[-_]?FSPGR|T1[-_]?3D|"
        "T1[-_]?WEIGHTED|ADNI|SAG\\s*T1|COR\\s*T1|AX\\s*T1|T1\\s*SAG|T1\\s*COR|T1\\s*AX)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    static QRegularExpression boldPatterns(
        "(BOLD|FMRI|F[-_]?MRI|REST|RESTING|EPI[-_]?BOLD|FUNCTIONAL|"
        "RS[-_]?FMRI|TASK[-_]?REST|EP2D[-_]?BOLD)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    static QRegularExpression excludeForT1(
        "(FLAIR|T2|DWI|DTI|ADC|BOLD|FMRI|REST|PERFUSION|ASL|SWI|TOF|MRA|EP2D|EPI)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    static QRegularExpression excludeForBold(
        "(T1W|MPRAGE|SPGR|BRAVO|FLAIR|DWI|DTI|ADC|PERFUSION|ASL|SWI|TOF|MRA|LOCALIZER|SCOUT)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    if (t1Patterns.match(desc).hasMatch() && !excludeForT1.match(desc).hasMatch()) {
        return MriSeriesInfo::T1W;
    }
    
    if (boldPatterns.match(desc).hasMatch() && !excludeForBold.match(desc).hasMatch()) {
        return MriSeriesInfo::BOLD;
    }
    
    // --- Step 2: Identification by Physical Parameters (Fallback) ---
    // Use physics characteristics if metadata name is missing or non-standard
    
    // 1. T1W Structural Imaging Characteristics:
    // - Very short TE (typically < 8ms)
    // - 3D Volume (High number of images, usually > 100)
    // - Note: We ignore TR here because MPRAGE TR (2000-3000ms) differs from FLASH TR (<50ms)
    if (info.echoTime > 0.1 && info.echoTime < 10.0 && info.numberOfImages >= 100) {
        return MriSeriesInfo::T1W;
    }
    
    // 2. BOLD Functional Imaging Characteristics:
    // - Moderate TE (20ms - 50ms) for T2* sensitivity
    // - Long TR (> 500ms)
    // - Time Series (High number of images, usually > 50)
    if (info.echoTime >= 15.0 && info.echoTime <= 60.0 && info.numberOfImages >= 50) {
        if (info.repetitionTime > 500) {
            return MriSeriesInfo::BOLD;
        }
    }
    
    return MriSeriesInfo::Unknown;
}

void BatchMriScanner::performScan(const QString& rootPath, int maxDepth)
{
    qDebug() << "\n========== Starting Batch MRI Scan ==========";
    qDebug() << "Root Path:" << rootPath;
    qDebug() << "Max Depth:" << maxDepth;
    
    // Step 1: Collect folders
    qDebug() << "\n[Step 1] Counting DICOM folders...";
    QStringList folders = collectDicomFolders(rootPath, maxDepth);
    
    if (m_stopRequested) {
        qDebug() << "Scan cancelled";
        emit scanError("Scan cancelled");
        return;
    }
    
    m_progress.totalFolders = folders.size();
    qDebug() << "Found" << m_progress.totalFolders << "DICOM folders";
    
    if (folders.isEmpty()) {
        qDebug() << "No DICOM folders found";
        emit scanFinished(m_results);
        return;
    }
    
    // Step 2: Identify and Pair
    qDebug() << "\n[Step 2] Identifying series types...";
    
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
        
        MriSeriesInfo info = readDicomInfo(folder);
        
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
        qDebug() << QString("      Identity: PatientID=%1, StudyDate=%2")
                    .arg(info.patientId.isEmpty() ? "(empty)" : info.patientId)
                    .arg(info.studyDate.isEmpty() ? "(empty)" : info.studyDate);

        // ========== Generate Pair Key ==========
        // Priority 1: PatientID + StudyDate (most reliable for same-session pairing)
        // Priority 2: Folder name prefix (fallback for anonymized data)
        QString pairKey;
        
        if (!info.patientId.isEmpty() && !info.studyDate.isEmpty()) {
            // Best case: use PatientID + StudyDate
            pairKey = info.patientId + "_" + info.studyDate;
            qDebug() << QString("      PairKey: %1 (by PatientID+StudyDate)").arg(pairKey);
        } else if (!info.patientId.isEmpty()) {
            // PatientID only (no study date)
            pairKey = info.patientId;
            qDebug() << QString("      PairKey: %1 (by PatientID only)").arg(pairKey);
        } else {
            // Fallback: use folder name prefix
            pairKey = folderName.section('_', 0, 0);
            if (pairKey.isEmpty()) {
                pairKey = folderName;
            }
            qDebug() << QString("      PairKey: %1 (by FolderName - FALLBACK)").arg(pairKey);
        }
        
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
            
            qDebug() << "      >>> Identified as T1W";
            emit seriesFound(info);
        }
        else if (info.type == MriSeriesInfo::BOLD) {
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
            
            qDebug() << "      >>> Identified as BOLD";
            emit seriesFound(info);
        }
        else {
            qDebug() << "      --- Ignored (Not T1W or BOLD)";
        }
        
        emit progressUpdated(m_progress);
    }
    
    // Step 3: Extract pairs
    qDebug() << "\n[Step 3] Generating pair results...";
    
    for (auto it = pairMap.begin(); it != pairMap.end(); ++it) {
        if (it.value().isComplete()) {
            m_results.append(it.value());
            m_progress.pairedCount++;
        }
    }
    
    qDebug() << "\n========== Scan Finished ==========";
    qDebug() << "Total Folders:" << m_progress.totalFolders;
    qDebug() << "Found T1W:" << m_progress.foundT1Count;
    qDebug() << "Found BOLD:" << m_progress.foundBoldCount;
    qDebug() << "Paired Success:" << m_progress.pairedCount;
    qDebug() << "==================================\n";
    
    emit progressUpdated(m_progress);
    emit scanFinished(m_results);
}

