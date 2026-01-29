#include "BidsConverter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QThreadPool>
#include <QThread>
#include <QMutex>
#include <QAtomicInt>
#include <QElapsedTimer>

// DCMTK headers for checking compression
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcmetinf.h"
#include "dcmtk/dcmdata/dcxfer.h"

// Global dcmdjpeg path (found once, used many times)
static QString g_dcmdjpegPath;

// ============================================================================
// BidsConverter Implementation
// ============================================================================

BidsConverter::BidsConverter(QObject* parent)
    : QObject(parent)
{
    // 注册自定义类型，用于跨线程信号队列连接
    qRegisterMetaType<BidsConversionProgress>("BidsConversionProgress");
    qRegisterMetaType<BidsSubjectResult>("BidsSubjectResult");
    qRegisterMetaType<QList<BidsSubjectResult>>("QList<BidsSubjectResult>");

    m_dcm2niixPath = findDcm2niixPath();
}

BidsConverter::~BidsConverter()
{
    stopConversion();
}

void BidsConverter::setOutputDirectory(const QString& outputDir)
{
    m_outputDir = outputDir;
}

void BidsConverter::setDatasetName(const QString& name)
{
    m_datasetName = name;
}

void BidsConverter::setTaskName(const QString& taskName)
{
    m_taskName = taskName;
}

QString BidsConverter::findDcm2niixPath()
{
    // Search order:
    // 1. Same directory as executable
    // 2. x64/Debug or x64/Release
    // 3. Scripts folder
    
    QStringList searchPaths;
    QString appDir = QCoreApplication::applicationDirPath();
    
    searchPaths 
                << appDir + "/dcm2niix.exe"
                << appDir + "/../x64/Debug/dcm2niix.exe"
                << appDir + "/../x64/Release/dcm2niix.exe"
                << appDir + "/Scripts/dcm2niix.exe";
    
    // Also check if dcm2niix is in PATH
    for (const QString& path : searchPaths) {
        if (QFileInfo::exists(path)) {
            qDebug() << "Found dcm2niix at:" << path;
            return path;
        }
        // Check if it's a directory containing dcm2niix.exe
        QString exePath = path + "/dcm2niix.exe";
        if (QFileInfo::exists(exePath)) {
            qDebug() << "Found dcm2niix at:" << exePath;
            return exePath;
        }
    }
    
    // Try just "dcm2niix" (assume it's in PATH)
    qDebug() << "dcm2niix not found in expected locations, will try PATH";
    return "dcm2niix";
}

QString BidsConverter::generateSubjectId(const MriPairResult& pair, int index)
{
    // Generate subject ID: sub-YYYYMMDDNNN
    // YYYYMMDD: study date or current date
    // NNN: sequence number (001, 002, etc.)
    
    QString dateStr;
    
    // Try to use study date from DICOM
    if (!pair.studyDate.isEmpty() && pair.studyDate.length() >= 8) {
        // DICOM date format is YYYYMMDD
        dateStr = pair.studyDate.left(8);
    } else {
        // Fallback to current date
        dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
    }
    
    // Generate sequence number
    QString baseId = dateStr;
    int seqNum = 1;
    
    if (m_usedSubjectIds.contains(baseId)) {
        seqNum = m_usedSubjectIds[baseId] + 1;
    }
    m_usedSubjectIds[baseId] = seqNum;
    
    // Format: sub-YYYYMMDDNNN (e.g., sub-20260120001)
    QString subjectId = QString("sub-%1%2").arg(dateStr).arg(seqNum, 3, 10, QChar('0'));
    
    return subjectId;
}

bool BidsConverter::createSubjectDirectories(const QString& subjectId)
{
    QDir outputDir(m_outputDir);
    
    // Create main output directory if not exists
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) {
            qWarning() << "Failed to create output directory:" << m_outputDir;
            return false;
        }
    }
    
    // Create subject directory structure
    QString subjectPath = m_outputDir + "/" + subjectId;
    QDir subjectDir(subjectPath);
    
    // Create anat and func directories
    if (!subjectDir.mkpath("anat")) {
        qWarning() << "Failed to create anat directory for" << subjectId;
        return false;
    }
    
    if (!subjectDir.mkpath("func")) {
        qWarning() << "Failed to create func directory for" << subjectId;
        return false;
    }
    
    return true;
}

bool BidsConverter::createDatasetDescription()
{
    QDir outputDir(m_outputDir);

    // Create main output directory if not exists
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) {
            qWarning() << "Failed to create output directory:" << m_outputDir;
            return false;
        }
    }

    QString descPath = m_outputDir + "/dataset_description.json";
    
    // Check if already exists
    if (QFileInfo::exists(descPath)) {
        return true;
    }
    
    QJsonObject desc;
    desc["Name"] = m_datasetName;
    desc["BIDSVersion"] = "1.9.0";
    desc["DatasetType"] = "raw";
    
    QJsonArray authors;
    authors.append("ImageSystem Auto-Converter");
    desc["Authors"] = authors;
    
    desc["GeneratedBy"] = QJsonArray({
        QJsonObject({
            {"Name", "ImageSystem BidsConverter"},
            {"Version", "1.0.0"},
            {"CodeURL", ""}
        })
    });
    
    QJsonDocument doc(desc);
    
    QFile file(descPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create dataset_description.json";
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qDebug() << "Created dataset_description.json";
    return true;
}

bool BidsConverter::updateParticipantsTsv(const QList<BidsSubjectResult>& results)
{
    QString tsvPath = m_outputDir + "/participants.tsv";
    
    QFile file(tsvPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create participants.tsv";
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "participant_id\tsex\tage\n";
    
    // Write each subject
    for (const auto& result : results) {
        if (result.t1Success || result.boldSuccess) {
            out << result.subjectId << "\tn/a\tn/a\n";
        }
    }
    
    file.close();
    qDebug() << "Updated participants.tsv with" << results.size() << "subjects";
    return true;
}

bool BidsConverter::isCompressedDicom(const QString& dicomDir)
{
    // Check the first DICOM file in the directory to determine if compressed
    QDir dir(dicomDir);
    QStringList files = dir.entryList({"*.dcm", "*.DCM", "*.ima", "*.IMA"}, QDir::Files);
    
    if (files.isEmpty()) {
        return false;
    }
    
    QString firstFile = dir.absoluteFilePath(files.first());
    
    DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile(firstFile.toLocal8Bit().constData());
    
    if (status.bad()) {
        qWarning() << "Cannot read DICOM file for compression check:" << firstFile;
        return false;
    }
    
    // Get Transfer Syntax UID
    DcmMetaInfo* metaInfo = fileformat.getMetaInfo();
    if (!metaInfo) {
        return false;
    }
    
    OFString transferSyntaxUID;
    if (metaInfo->findAndGetOFString(DCM_TransferSyntaxUID, transferSyntaxUID).bad()) {
        return false;
    }
    
    QString tsUID = QString::fromUtf8(transferSyntaxUID.c_str());
    
    // List of compressed Transfer Syntax UIDs
    // JPEG Baseline: 1.2.840.10008.1.2.4.50
    // JPEG Extended: 1.2.840.10008.1.2.4.51
    // JPEG Lossless: 1.2.840.10008.1.2.4.57, 1.2.840.10008.1.2.4.70
    // JPEG-LS: 1.2.840.10008.1.2.4.80, 1.2.840.10008.1.2.4.81
    // JPEG 2000: 1.2.840.10008.1.2.4.90, 1.2.840.10008.1.2.4.91
    // RLE: 1.2.840.10008.1.2.5
    
    bool isCompressed = tsUID.startsWith("1.2.840.10008.1.2.4") ||  // JPEG family
                        tsUID == "1.2.840.10008.1.2.5";              // RLE
    
    if (isCompressed) {
        qDebug() << "    Detected compressed DICOM (Transfer Syntax:" << tsUID << ")";
    }
    
    return isCompressed;
}

// Find dcmdjpeg.exe tool path
static QString findDcmdjpegPath()
{
    if (!g_dcmdjpegPath.isEmpty() && QFileInfo::exists(g_dcmdjpegPath)) {
        return g_dcmdjpegPath;
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Search in common locations
    QStringList searchPaths = {
        appDir + "/dcmdjpeg.exe",
        appDir + "/../dcmdjpeg.exe",
        "C:/dcmtk/bin/dcmdjpeg.exe",
        "C:/Program Files/DCMTK/bin/dcmdjpeg.exe",
        "dcmdjpeg"  // Try PATH
    };
    
    // Also check DCMTK install path from environment
    QString dcmtkPath = qEnvironmentVariable("DCMTK_DIR");
    if (!dcmtkPath.isEmpty()) {
        searchPaths.prepend(dcmtkPath + "/bin/dcmdjpeg.exe");
    }
    
    for (const QString& path : searchPaths) {
        if (QFileInfo::exists(path)) {
            g_dcmdjpegPath = path;
            qDebug() << "Found dcmdjpeg at:" << path;
            return path;
        }
    }
    
    // Try to run dcmdjpeg to see if it's in PATH
    QProcess test;
    test.start("dcmdjpeg", {"--version"});
    if (test.waitForFinished(3000) && test.exitCode() == 0) {
        g_dcmdjpegPath = "dcmdjpeg";
        qDebug() << "Found dcmdjpeg in PATH";
        return "dcmdjpeg";
    }
    
    qWarning() << "dcmdjpeg not found! Please ensure DCMTK tools are installed.";
    return QString();
}

// Decompress a single file using dcmdjpeg (for parallel execution)
static bool decompressOneFile(const QString& dcmdjpegPath, const QString& inputFile, const QString& outputFile)
{
    QProcess process;
    process.start(dcmdjpegPath, {inputFile, outputFile});
    
    if (!process.waitForFinished(30000)) {  // 30 second timeout per file
        process.kill();
        return false;
    }
    
    return process.exitCode() == 0;
}

bool BidsConverter::decompressDicom(const QString& inputDir, const QString& outputDir, QString& errorMsg)
{
    QElapsedTimer timer;
    timer.start();
    
    // Find dcmdjpeg tool
    QString dcmdjpegPath = findDcmdjpegPath();
    if (dcmdjpegPath.isEmpty()) {
        errorMsg = "dcmdjpeg tool not found. Please install DCMTK or add it to PATH.";
        return false;
    }
    
    // Create output directory
    QDir outDir(outputDir);
    if (!outDir.exists()) {
        if (!outDir.mkpath(".")) {
            errorMsg = "Failed to create decompression output directory";
            return false;
        }
    }
    
    // Get all DICOM files
    QDir dir(inputDir);
    QStringList files = dir.entryList({"*.dcm", "*.DCM", "*.ima", "*.IMA"}, QDir::Files);
    
    if (files.isEmpty()) {
        errorMsg = "No DICOM files found in input directory";
        return false;
    }
    
    int totalFiles = files.size();
    int numThreads = qMin(QThread::idealThreadCount(), 8);  // Max 8 parallel processes
    
    qDebug() << QString("    Decompressing %1 files using dcmdjpeg (%2 parallel processes)...")
                .arg(totalFiles).arg(numThreads);
    
    // Atomic counters for progress
    QAtomicInt successCount(0);
    QAtomicInt failCount(0);
    QAtomicInt processedCount(0);
    QAtomicInt lastReportedPercent(-1);
    QMutex progressMutex;
    
    // Set thread pool size
    QThreadPool pool;
    pool.setMaxThreadCount(numThreads);
    
    // Submit all tasks to thread pool
    for (const QString& filename : files) {
        QString inputFile = dir.absoluteFilePath(filename);
        QString outputFile = outputDir + "/" + filename;
        
        // Create a runnable task (capture totalFiles by value, atomics by reference)
        auto task = [=, &successCount, &failCount, &processedCount, &lastReportedPercent, &progressMutex]() {
            bool success = decompressOneFile(dcmdjpegPath, inputFile, outputFile);
            
            if (success) {
                successCount.fetchAndAddRelaxed(1);
            } else {
                failCount.fetchAndAddRelaxed(1);
            }
            
            // Update progress
            int processed = processedCount.fetchAndAddRelaxed(1) + 1;
            int percent = (processed * 100) / totalFiles;
            
            // Print progress every 10%
            int lastPercent = lastReportedPercent.loadRelaxed();
            if (percent >= lastPercent + 10 || processed == totalFiles) {
                QMutexLocker locker(&progressMutex);
                if (percent >= lastReportedPercent.loadRelaxed() + 10 || processed == totalFiles) {
                    lastReportedPercent.storeRelaxed((percent / 10) * 10);
                    qDebug() << QString("    Progress: %1% (%2/%3)")
                                .arg(percent).arg(processed).arg(totalFiles);
                }
            }
        };
        
        // Submit to thread pool using QRunnable wrapper
        pool.start(task);
    }
    
    // Wait for all tasks to complete
    pool.waitForDone();
    
    qint64 elapsed = timer.elapsed();
    double speed = elapsed > 0 ? (totalFiles * 1000.0) / elapsed : 0;
    
    qDebug() << QString("    Decompression complete: %1 success, %2 failed in %3 ms (%.1f files/sec)")
                .arg(successCount.loadRelaxed())
                .arg(failCount.loadRelaxed())
                .arg(elapsed)
                .arg(speed);
    
    if (successCount.loadRelaxed() == 0) {
        errorMsg = QString("All %1 files failed to decompress").arg(files.size());
        return false;
    }
    
    if (failCount.loadRelaxed() > 0) {
        qWarning() << "    Warning:" << failCount.loadRelaxed() << "files failed to decompress";
    }
    
    return true;
}

bool BidsConverter::runDcm2niix(const QString& inputDir, const QString& outputDir,
                                 const QString& outputFileName, QString& errorMsg)
{
    QProcess process;
    
    QStringList arguments;
    arguments << "-o" << outputDir;      // Output directory
    arguments << "-f" << outputFileName; // Output filename (without extension)
    arguments << "-z" << "y";            // Compress to .nii.gz
    arguments << "-b" << "y";            // Generate BIDS sidecar JSON
    arguments << inputDir;               // Input DICOM directory
    
    qDebug() << "Running dcm2niix:" << m_dcm2niixPath << arguments.join(" ");
    
    process.start(m_dcm2niixPath, arguments);
    
    if (!process.waitForStarted(5000)) {
        errorMsg = "Failed to start dcm2niix: " + process.errorString();
        qWarning() << errorMsg;
        return false;
    }
    
    if (!process.waitForFinished(120000)) { // Wait up to 2 minutes
        errorMsg = "dcm2niix timed out";
        process.kill();
        qWarning() << errorMsg;
        return false;
    }
    
    if (process.exitCode() != 0) {
        errorMsg = QString::fromUtf8(process.readAllStandardError());
        qWarning() << "dcm2niix error:" << errorMsg;
        return false;
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    qDebug() << "dcm2niix output:" << output;
    
    return true;
}

void BidsConverter::performConversion(const QList<MriPairResult>& pairs)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    int totalSubjects = pairs.size();
    int cpuCores = QThread::idealThreadCount();
    
    qDebug() << "\n##################################################";
    qDebug() << "#          BIDS Batch Conversion Started         #";
    qDebug() << "##################################################";
    qDebug() << QString("Total Subjects: %1").arg(totalSubjects);
    qDebug() << QString("CPU Cores: %1 (parallel decompression)").arg(cpuCores);
    qDebug() << QString("Output: %1").arg(m_outputDir);
    qDebug() << QString("Task Name: %1").arg(m_taskName);
    qDebug() << "##################################################\n";
    
    m_progress.totalPairs = pairs.size();
    m_progress.convertedPairs = 0;
    m_results.clear();
    m_usedSubjectIds.clear();
    
    // Ensure output directory exists before creating dataset files
    QDir outputDir(m_outputDir);
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) {
            qWarning() << "Failed to create output directory:" << m_outputDir;
            emit conversionError("Failed to create output directory: " + m_outputDir);
            return;
        }
        qDebug() << "Created output directory:" << m_outputDir;
    }
    
    // Create dataset description
    createDatasetDescription();
    
    for (int i = 0; i < pairs.size(); ++i) {
        if (m_stopRequested) {
            qDebug() << "Conversion stopped by user";
            emit conversionError("Conversion stopped by user");
            break;
        }
        
        QElapsedTimer subjectTimer;
        subjectTimer.start();
        
        const MriPairResult& pair = pairs[i];
        BidsSubjectResult result;
        
        // Generate subject ID
        result.subjectId = generateSubjectId(pair, i);
        m_progress.currentSubject = result.subjectId;
        
        int overallPercent = (i * 100) / totalSubjects;
        qDebug() << QString("\n[Subject %1/%2] (%3%%) Converting: %4")
                    .arg(i + 1).arg(totalSubjects).arg(overallPercent).arg(result.subjectId);
        qDebug() << QString("    PatientName: %1 | StudyDate: %2")
                    .arg(pair.patientName.isEmpty() ? "(empty)" : pair.patientName)
                    .arg(pair.studyDate.isEmpty() ? "(empty)" : pair.studyDate);
        
        // Create directories
        if (!createSubjectDirectories(result.subjectId)) {
            result.errorMessage = "Failed to create directories";
            m_results.append(result);
            continue;
        }
        
        QString subjectPath = m_outputDir + "/" + result.subjectId;
        QString errorMsg;
        
        // Temporary directory for decompressed files
        QString tempDir = m_outputDir + "/.temp_decompress";
        
        // ========== Convert T1W ==========
        m_progress.currentStep = "T1W";
        m_progress.currentT1Status = 1; // Converting
        emit progressUpdated(m_progress);
        
        QString t1OutputName = result.subjectId + "_T1w";
        QString t1OutputDir = subjectPath + "/anat";
        QString t1InputDir = pair.t1Path;
        QString t1TempDir;
        
        qDebug() << "    [T1W] Source:" << pair.t1Path;
        
        // Check if T1W needs decompression
        if (isCompressedDicom(pair.t1Path)) {
            t1TempDir = tempDir + "/" + result.subjectId + "_t1w";
            qDebug() << "    [T1W] Compressed DICOM detected, decompressing...";
            
            if (decompressDicom(pair.t1Path, t1TempDir, errorMsg)) {
                t1InputDir = t1TempDir;  // Use decompressed files
                qDebug() << "    [T1W] Using decompressed files from:" << t1TempDir;
            } else {
                qWarning() << "    [T1W] Decompression failed:" << errorMsg;
                // Try original files anyway
            }
        }
        
        // Run dcm2niix
        if (runDcm2niix(t1InputDir, t1OutputDir, t1OutputName, errorMsg)) {
            result.t1NiftiPath = t1OutputDir + "/" + t1OutputName + ".nii.gz";
            result.t1JsonPath = t1OutputDir + "/" + t1OutputName + ".json";
            result.t1Success = true;
            m_progress.currentT1Status = 2; // Done
            qDebug() << "    [T1W] Conversion successful:" << result.t1NiftiPath;
        } else {
            result.errorMessage += "T1W: " + errorMsg + "; ";
            m_progress.currentT1Status = -1; // Failed
            qWarning() << "    [T1W] Conversion failed:" << errorMsg;
        }
        
        // Clean up T1W temp files
        if (!t1TempDir.isEmpty() && QDir(t1TempDir).exists()) {
            QDir(t1TempDir).removeRecursively();
        }
        
        // ========== Convert BOLD ==========
        m_progress.currentStep = "BOLD";
        m_progress.currentBoldStatus = 1; // Converting
        emit progressUpdated(m_progress);
        
        QString boldOutputName = result.subjectId + "_task-" + m_taskName + "_bold";
        QString boldOutputDir = subjectPath + "/func";
        QString boldInputDir = pair.boldPath;
        QString boldTempDir;
        
        qDebug() << "    [BOLD] Source:" << pair.boldPath;
        
        // Check if BOLD needs decompression
        if (isCompressedDicom(pair.boldPath)) {
            boldTempDir = tempDir + "/" + result.subjectId + "_bold";
            qDebug() << "    [BOLD] Compressed DICOM detected, decompressing...";
            
            if (decompressDicom(pair.boldPath, boldTempDir, errorMsg)) {
                boldInputDir = boldTempDir;  // Use decompressed files
                qDebug() << "    [BOLD] Using decompressed files from:" << boldTempDir;
            } else {
                qWarning() << "    [BOLD] Decompression failed:" << errorMsg;
                // Try original files anyway
            }
        }
        
        // Run dcm2niix
        if (runDcm2niix(boldInputDir, boldOutputDir, boldOutputName, errorMsg)) {
            result.boldNiftiPath = boldOutputDir + "/" + boldOutputName + ".nii.gz";
            result.boldJsonPath = boldOutputDir + "/" + boldOutputName + ".json";
            result.boldSuccess = true;
            m_progress.currentBoldStatus = 2; // Done
            qDebug() << "    [BOLD] Conversion successful:" << result.boldNiftiPath;
        } else {
            result.errorMessage += "BOLD: " + errorMsg + "; ";
            m_progress.currentBoldStatus = -1; // Failed
            qWarning() << "    [BOLD] Conversion failed:" << errorMsg;
        }
        
        // Clean up BOLD temp files
        if (!boldTempDir.isEmpty() && QDir(boldTempDir).exists()) {
            QDir(boldTempDir).removeRecursively();
        }
        
        m_results.append(result);
        emit subjectConverted(result);
        
        m_progress.convertedPairs++;
        emit progressUpdated(m_progress);
        
        // Subject completion status
        qint64 subjectElapsed = subjectTimer.elapsed();
        QString status = (result.t1Success && result.boldSuccess) ? "OK" : 
                         (result.t1Success || result.boldSuccess) ? "PARTIAL" : "FAILED";
        qDebug() << QString("    [%1] Subject completed in %2 ms")
                    .arg(status).arg(subjectElapsed);
        
        // Estimate remaining time
        qint64 totalElapsed = totalTimer.elapsed();
        int remaining = totalSubjects - (i + 1);
        if (i > 0 && remaining > 0) {
            qint64 avgTime = totalElapsed / (i + 1);
            qint64 estimatedRemaining = avgTime * remaining;
            int mins = estimatedRemaining / 60000;
            int secs = (estimatedRemaining % 60000) / 1000;
            qDebug() << QString("    Estimated remaining: %1 min %2 sec (%3 subjects left)")
                        .arg(mins).arg(secs).arg(remaining);
        }
        
        // Reset status for next iteration
        m_progress.currentT1Status = 0;
        m_progress.currentBoldStatus = 0;
    }
    
    // Create participants.tsv
    updateParticipantsTsv(m_results);
    
    // Final Summary
    qint64 totalElapsedMs = totalTimer.elapsed();
    int totalMins = totalElapsedMs / 60000;
    int totalSecs = (totalElapsedMs % 60000) / 1000;
    
    int successCount = 0;
    int partialCount = 0;
    int failedCount = 0;
    for (const auto& r : m_results) {
        if (r.t1Success && r.boldSuccess) successCount++;
        else if (r.t1Success || r.boldSuccess) partialCount++;
        else failedCount++;
    }
    
    qDebug() << "\n##################################################";
    qDebug() << "#         BIDS Batch Conversion Complete         #";
    qDebug() << "##################################################";
    qDebug() << QString("Total Time: %1 min %2 sec").arg(totalMins).arg(totalSecs);
    qDebug() << QString("Total Subjects: %1").arg(pairs.size());
    qDebug() << QString("  - Full Success: %1").arg(successCount);
    qDebug() << QString("  - Partial:      %1").arg(partialCount);
    qDebug() << QString("  - Failed:       %1").arg(failedCount);
    if (pairs.size() > 0) {
        double avgSec = totalElapsedMs / (1000.0 * pairs.size());
        qDebug() << QString("Average: %.1f sec/subject").arg(avgSec);
    }
    qDebug() << QString("Output: %1").arg(m_outputDir);
    qDebug() << "##################################################\n";
    
    emit conversionFinished(m_results);
}

QList<BidsSubjectResult> BidsConverter::convertSync(const QList<MriPairResult>& pairs)
{
    if (pairs.isEmpty()) {
        qWarning() << "No pairs to convert";
        return m_results;
    }
    
    m_stopRequested = false;
    m_isConverting = true;
    
    performConversion(pairs);
    
    m_isConverting = false;
    return m_results;
}

void BidsConverter::startConversion(const QList<MriPairResult>& pairs)
{
    if (m_isConverting) {
        emit conversionError("Conversion already in progress");
        return;
    }
    
    if (pairs.isEmpty()) {
        emit conversionError("No pairs to convert");
        return;
    }
    
    m_stopRequested = false;
    m_isConverting = true;
    
    // Run in background thread using QThread
    QThread* thread = QThread::create([this, pairs]() {
        performConversion(pairs);
        m_isConverting = false;
    });
    
    // Auto-delete thread when finished
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void BidsConverter::stopConversion()
{
    m_stopRequested = true;
}

