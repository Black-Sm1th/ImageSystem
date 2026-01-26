#ifndef BIDSCONVERTER_H
#define BIDSCONVERTER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include "BatchMriScanner.h"

// ============================================================================
// BIDS Conversion Progress
// ============================================================================
struct BidsConversionProgress {
    int totalPairs = 0;
    int convertedPairs = 0;
    int currentT1Status = 0;    // 0: pending, 1: converting, 2: done, -1: failed
    int currentBoldStatus = 0;
    QString currentSubject;
    QString currentStep;        // "T1W" or "BOLD"
    QString lastError;
    
    double percentage() const {
        if (totalPairs == 0) return 0;
        return (double)convertedPairs / totalPairs;
    }
};

// ============================================================================
// BIDS Conversion Result (per subject)
// ============================================================================
struct BidsSubjectResult {
    QString subjectId;          // e.g., "sub-20260120001"
    QString t1NiftiPath;        // Path to converted T1W NIfTI
    QString boldNiftiPath;      // Path to converted BOLD NIfTI
    QString t1JsonPath;         // Path to T1W sidecar JSON
    QString boldJsonPath;       // Path to BOLD sidecar JSON
    bool t1Success = false;
    bool boldSuccess = false;
    QString errorMessage;
};

// ============================================================================
// BidsConverter Class
// ============================================================================
class BidsConverter : public QObject
{
    Q_OBJECT

public:
    explicit BidsConverter(QObject* parent = nullptr);
    ~BidsConverter();

    // Set output directory (where BIDS dataset will be created)
    void setOutputDirectory(const QString& outputDir);
    
    // Set dataset name (for dataset_description.json)
    void setDatasetName(const QString& name);
    
    // Set task name for BOLD (default: "rest")
    void setTaskName(const QString& taskName);
    
    // Convert a list of paired MRI results to BIDS format
    // This is synchronous (blocking)
    QList<BidsSubjectResult> convertSync(const QList<MriPairResult>& pairs);
    
    // Start asynchronous conversion
    void startConversion(const QList<MriPairResult>& pairs);
    
    // Stop conversion
    void stopConversion();
    
    // Check if conversion is running
    bool isConverting() const { return m_isConverting; }
    
    // Get conversion results
    QList<BidsSubjectResult> getResults() const { return m_results; }

signals:
    void progressUpdated(const BidsConversionProgress& progress);
    void subjectConverted(const BidsSubjectResult& result);
    void conversionFinished(const QList<BidsSubjectResult>& results);
    void conversionError(const QString& error);

private:
    // Generate unique subject ID based on study date
    QString generateSubjectId(const MriPairResult& pair, int index);
    
    // Create BIDS directory structure for a subject
    bool createSubjectDirectories(const QString& subjectId);
    
    // Create dataset_description.json
    bool createDatasetDescription();
    
    // Create/update participants.tsv
    bool updateParticipantsTsv(const QList<BidsSubjectResult>& results);
    
    // Run dcm2niix for a single conversion
    bool runDcm2niix(const QString& inputDir, const QString& outputDir, 
                     const QString& outputFileName, QString& errorMsg);
    
    // Check if DICOM files in directory are compressed
    bool isCompressedDicom(const QString& dicomDir);
    
    // Decompress DICOM files using DCMTK
    bool decompressDicom(const QString& inputDir, const QString& outputDir, QString& errorMsg);
    
    // Perform the actual conversion
    void performConversion(const QList<MriPairResult>& pairs);
    
    // Find dcm2niix executable
    QString findDcm2niixPath();

private:
    QString m_outputDir;
    QString m_datasetName = "BrainMRI_Dataset";
    QString m_taskName = "rest";
    QString m_dcm2niixPath;
    
    bool m_isConverting = false;
    bool m_stopRequested = false;
    
    BidsConversionProgress m_progress;
    QList<BidsSubjectResult> m_results;
    
    // Track used subject IDs to avoid duplicates
    QMap<QString, int> m_usedSubjectIds;
};

#endif // BIDSCONVERTER_H

