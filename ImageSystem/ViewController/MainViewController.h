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
public:
    Q_INVOKABLE void calculateKidney();
    Q_INVOKABLE void importBrainData(const QString& url);
    Q_INVOKABLE void selectBrainRegion(int row);
    Q_INVOKABLE void startfmriprepAnalysis(const QString& dicomDir,
                                           const QString& bidsDir,
                                           const QString& outputDir,
                                           const QString& licenseFile,
                                           bool useFreesurfer);
    Q_INVOKABLE void stopFmriprepProcess();
    Q_INVOKABLE void clearFmriprepLog();
    Q_INVOKABLE void startDeepprepAnalysis(const QString& inputDir,
                                           const QString& bidsDir,
                                           const QString& outputDir,
                                           const QString& licenseFile);
    Q_INVOKABLE void stopDeepprepProcess();
    Q_INVOKABLE void clearDeepprepLog();
    Q_INVOKABLE void startAnalysisBrainAge(const QString& path, bool preprocess);
    Q_INVOKABLE void generatePdfReport(const QString& savePath);
    Q_INVOKABLE bool isDeepprepOutput(const QString& outputPath);
    Q_INVOKABLE void updateAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deleteAnnotation(int orientation, int index);
    Q_INVOKABLE void updateCircleAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deleteCircleAnnotation(int orientation, int index);
    Q_INVOKABLE void updatePenAnnotationText(int orientation, int index, const QString& text);
    Q_INVOKABLE void deletePenAnnotation(int orientation, int index);
    Q_INVOKABLE void captureViewScreenshot(int viewType, const QString& filePath);
    Q_INVOKABLE void scanFolder(const QString& inputDir);
    Q_INVOKABLE void startPreAnalysis(int method, const QString& bidsPath, const QString& outputPath, const QString& licenseFile);
    Q_PROPERTY(QString fmriprepLog READ fmriprepLog NOTIFY fmriprepLogUpdated)
    QString fmriprepLog() const { return m_fmriprepLog; }
    Q_PROPERTY(QString deepprepLog READ deepprepLog NOTIFY deepprepLogUpdated)
    QString deepprepLog() const { return m_deepprepLog; }
    // 获取表格模型
    BrainRegionTableModel* getBrainRegionTableModel() const;
    BrainSegmentationTableModel* getBrainSegmentationTableModel() const;
    MriPairResultModel* getMriPairResultModel() const;
    
signals:
    void brainAnalysisStarted();
    void brainAnalysisFinished(bool success);
    void networkTableIndexChanged(int index);
    void fmriprepLogUpdated();
    void deepprepLogUpdated();
    void annotationCreated(double screenX, double screenY, int annotationIndex, int orientation, int annotationType);

public slots:
    void onScanProgressUpdated(const ScanProgress& progress);
    void onScanFinished(const QList<MriPairResult>& results);
    void onConverterProgressUpdated(const BidsConversionProgress& progress);
    void onConversionFinished(const QList<BidsSubjectResult>& results);

private:
    bool loadOutputData(const QString& path);
    void processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir);
    void appendFmriprepLog(const QString& text);
    void startLogTimer(const QString& logFilePath);
    void stopLogTimer();
    void appendDeepprepLog(const QString& text);
    void startDeepprepLogTimer(const QString& logFilePath);
    void stopDeepprepLogTimer();
    BidsConverter* m_bidsConverter;
    BrainRegionTableModel* m_brainRegionTableModel;
    BrainSegmentationTableModel* m_brainSegmentationTableModel;
    MriPairResultModel* m_mriPairResultModel;
    BatchMriScanner* m_mriScanner;
    QPointer<QProcess> m_fmriprepProcess;
    qint64 m_fmriprepPid = -1;
    QString m_fmriprepLog;
    QString m_fmriprepLogFilePath;
    qint64 m_fmriprepLogReadPos = 0;
    QTimer* m_fmriprepLogTimer = nullptr;
    QTimer* m_fmriprepLogUpdateTimer = nullptr; // 节流Timer，避免频繁更新UI
    QPointer<QProcess> m_deepprepProcess;
    qint64 m_deepprepPid = -1;
    QString m_deepprepLog;
    QString m_deepprepLogFilePath;
    qint64 m_deepprepLogReadPos = 0;
    QTimer* m_deepprepLogTimer = nullptr;
    QTimer* m_deepprepLogUpdateTimer = nullptr; // 节流Timer，避免频繁更新UI
};

