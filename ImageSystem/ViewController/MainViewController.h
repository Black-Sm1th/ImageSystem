#pragma once
#include "Model/DicomDataModel.h"
#include "Model/BrainRegionTableModel.h"
#include "Model/BrainSegmentationTableModel.h"
#include "Model/MriPairResultModel.h"
#include "Modules/CommonFunc.h"
#include "Modules/DicomNetwork.h"
#include "Modules/BatchMriScanner.h"
#include "Modules/BidsConverter.h"
#include "Modules/InteractionState.h"
#include "Modules/DockerPrepRunner.h"
#include "Modules/BrainRegionProcessor.h"
#include <vtkInteractorStyleImage.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkObjectFactory.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkTextMapper.h>
#include <QQuickVTKItem.h>
#include <vtkImageProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkTextProperty.h>
#include <vtkActor2D.h>
#include <vtkCamera.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <QProcess.h>
#include <QTimer.h>
#include <QPointer.h>
#include <QFile.h>
#include <QMap>
#include <vtkAxisActor2D.h>
#include <vtkProperty2D.h>
#include <memory>
#include <optional>
#include <map>

class MainViewController : public QObject
{
    Q_OBJECT
        SINGLETON_CLASS(MainViewController)
        QUICK_PROPERTY(int, t2)
        QUICK_PROPERTY(int, skin)
        QUICK_PROPERTY(int, micro)
        QUICK_PROPERTY(int, sei)
        QUICK_PROPERTY(int, ader)
        QUICK_PROPERTY(int, disp)
        QUICK_PROPERTY(double, cclsResult)
        QUICK_PROPERTY(double, ccrccResult)

        QUICK_PROPERTY(double, globalEfficiency)
        QUICK_PROPERTY(double, averageLocalEfficiency)
        QUICK_PROPERTY(double, averageClusteringCoefficient)
        QUICK_PROPERTY(double, richClubConnections)
        QUICK_PROPERTY(double, bridgeConnections)
        QUICK_PROPERTY(double, localConnections)
        QUICK_PROPERTY(double, predictedBrainAge)
        QUICK_PROPERTY(bool, brainAgeProcessing)

        QUICK_PROPERTY(QString, currentAlffUrl)
        QUICK_PROPERTY(QString, currentCovarianceUrl)
        QUICK_PROPERTY(QString, currentRegionplotsUrl)
        QUICK_PROPERTY(QString, currentViewConnectomeUrl)

        // 扫描进度相关属性
        QUICK_PROPERTY(bool, isScanning)
        QUICK_PROPERTY(int, scanTotalFolders)
        QUICK_PROPERTY(int, scanScannedFolders)
        QUICK_PROPERTY(int, scanFoundT1Count)
        QUICK_PROPERTY(int, scanFoundBoldCount)
        QUICK_PROPERTY(int, scanPairedCount)
        QUICK_PROPERTY(double, scanProgress)
        QUICK_PROPERTY(QString, scanCurrentFolder)

        // 预分析状态属性
        QUICK_PROPERTY(bool, isPreAnalysisRunning)
public:
    Q_INVOKABLE void calculateKidney();
    Q_INVOKABLE void importBrainData(const QString& url, const QString& subjectId = "sub-01", const QString& patientId = "sub-01");
    Q_INVOKABLE void selectBrainRegion(int row);
    Q_INVOKABLE void scanFolder(const QString& inputDir);
    Q_INVOKABLE void startPreAnalysis(int method, const QString& bidsPath, const QString& outputPath, const QString& licenseFile);
    Q_INVOKABLE void stopFmriprepProcess();
    Q_INVOKABLE void stopDeepprepProcess();
    Q_INVOKABLE void generatePdfReport(const QString& savePath);
    Q_INVOKABLE bool isDeepprepOutput(const QString& outputPath);
    Q_INVOKABLE void updateAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deleteAnnotation(int orientation, int index);
    Q_INVOKABLE void updateCircleAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deleteCircleAnnotation(int orientation, int index);
    Q_INVOKABLE void updatePenAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deletePenAnnotation(int orientation, int index);
    Q_INVOKABLE void captureViewScreenshot(int viewType, const QString& filePath);
    Q_INVOKABLE QString estimateProcessingTime(int method, int subjectCount);
    Q_PROPERTY(QString preAnalysisLog READ preAnalysisLog NOTIFY preAnalysisLogUpdated)
        QString preAnalysisLog() const { return m_preAnalysisLog; }
    Q_INVOKABLE QVariantMap readMetadataFile(const QString& outputDir);
    // 读取脑龄预测CSV文件
    Q_INVOKABLE bool loadBrainAgePredictions(const QString& basePath);
    // 获取表格模型
    BrainRegionTableModel* getBrainRegionTableModel() const;
    BrainSegmentationTableModel* getBrainSegmentationTableModel() const;
    MriPairResultModel* getMriPairResultModel() const;

signals:
    void brainAnalysisStarted();
    void brainAnalysisFinished(bool success);
    void networkTableIndexChanged(int index);
    void preAnalysisLogUpdated();
    void annotationCreated(double screenX, double screenY, int annotationIndex, int orientation, int annotationType);

public slots:
    void onScanProgressUpdated(const ScanProgress& progress);
    void onScanFinished(const QList<MriPairResult>& results);
    void onConverterProgressUpdated(const BidsConversionProgress& progress);
    void onConversionFinished(const QList<BidsSubjectResult>& results);

private:
    bool loadOutputData(const QString& path);
    void processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir);
    void startPrepLogTimer(const QString& logFilePath);  // 启动日志轮询
    void stopPrepLogTimer();                              // 停止日志轮询
    void startFmriprepAfterBids();   // BIDS转换完成后启动fmriprep
    void startDeepprepAfterBids();   // BIDS转换完成后启动deepprep
    void clearPrepOutputsOnFailure(const QString& outputDir, bool isFmriPrep);
    
    // 映射文件相关
    void writeMetadataFile(const QString& outputDir, const QList<MriPairResult>& pairs);
    
    void appendPreAnalysisLog(const QString& text);  // 追加统一预分析日志
    void clearPreAnalysisLog();                       // 清空统一预分析日志
    void setupDockerPrepRunner();    // 初始化 DockerPrepRunner
    void startBatchBrainAgePrediction(const QList<MriPairResult>& results, const QString& outputDir);  // 批量脑龄预测
    BidsConverter* m_bidsConverter;
    BrainRegionTableModel* m_brainRegionTableModel;
    BrainSegmentationTableModel* m_brainSegmentationTableModel;
    MriPairResultModel* m_mriPairResultModel;
    BatchMriScanner* m_mriScanner;

    // Docker 预处理运行器
    DockerPrepRunner* m_dockerPrepRunner = nullptr;
    
    // 脑区处理器
    BrainRegionProcessor* m_brainRegionProcessor = nullptr;
    void setupBrainRegionProcessor();  // 初始化脑区处理器
    void startBrainRegionProcessing(); // 预处理完成后开始脑区处理

    // 日志文件轮询相关（用于读取 Docker 输出日志）
    QString m_prepLogFilePath;
    qint64 m_prepLogReadPos = 0;
    QTimer* m_prepLogTimer = nullptr;

    // 预分析参数，用于在BIDS转换完成后启动fmriprep/deepprep
    int m_preAnalysisMethod = 0;       // 0: fmriprep, 1: deepprep
    QString m_preAnalysisBidsPath;
    QString m_preAnalysisOutputPath;
    QString m_preAnalysisLicenseFile;
    QList<MriPairResult> m_currentProcessingPairs; // 记录当前正在处理的配对信息

    // 统一预分析日志
    QString m_preAnalysisLog;
    QTimer* m_preAnalysisLogUpdateTimer = nullptr;  // 节流Timer
    
    // 脑龄预测数据缓存（subjectId -> predictedAge）
    QMap<QString, double> m_brainAgePredictions;
    QString m_currentBrainAgeDataPath;  // 当前加载的脑龄数据路径
    
    
};

