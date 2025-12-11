#pragma once
#include "Model/DicomDataModel.h"
#include "Model/BrainRegionTableModel.h"
#include "Modules/CommonFunc.h"
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

// 枚举类型：切片方向
enum class SliceOrientation {
    Axial,    // 轴向 (Z方向)
    Sagittal, // 矢状 (X方向)
    Coronal   // 冠状 (Y方向)
};

// 通用交互样式类 - 减少重复代码
class SliceInteractorStyle : public vtkInteractorStyleImage
{
public:
    static SliceInteractorStyle* New();
    vtkTypeMacro(SliceInteractorStyle, vtkInteractorStyleImage);

    void SetOrientation(SliceOrientation orientation);

    void OnMouseWheelForward() override;
    void OnMouseWheelBackward() override;
    void OnLeftButtonDown() override;
    void OnLeftButtonUp() override;
    void OnMouseMove() override;

protected:
    SliceInteractorStyle();

private:
    int getCurrentSlice() const;
    int getMaxSlice() const;
    void setSlice(int slice);
    DicomDataModel* m_dataModel;
    SliceOrientation m_orientation;
    bool m_isDragging;
    int m_lastX;
    int m_lastY;
};


// 存储切片视图的VTK对象
class SliceViewData : public vtkObject
{
public:
    static SliceViewData* New();
    vtkTypeMacro(SliceViewData, vtkObject);

    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkImageSliceMapper> imageMapper;
    vtkSmartPointer<vtkImageSlice> imageSlice;
    vtkSmartPointer<vtkTextMapper> wwwlTextMapper;
    vtkSmartPointer<vtkActor2D> wwwlTextActor;

protected:
    SliceViewData() {}
    ~SliceViewData() override {}

private:
    SliceViewData(const SliceViewData&) = delete;
    void operator=(const SliceViewData&) = delete;
};

// 切片视图基类 - 减少重复代码
class SliceVtkItemBase : public QQuickVTKItem
{
    Q_OBJECT
public:
    SliceVtkItemBase(SliceOrientation orientation, const char* viewName);

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;

private slots:
    void onDataLoaded();
    void onSegDataLoaded();
    void onSliceChanged(int slice);
    void onSegSliceChanged(int slice);
    void onWindowChanged();

private:
    void setupView(vtkRenderWindow* renderWindow, SliceViewData* data, vtkImageData* imageData);
    void applyParallelScale(vtkImageSlice* imageSlice, vtkRenderer* renderer);
    void setMapperOrientation(vtkImageSliceMapper* mapper);
    void setupCamera(vtkRenderer* renderer);

    int getCurrentSlice() const;
    int getSegCurrentSlice() const;

    void updateWWWLText(SliceViewData* data);
    DicomDataModel* m_dataModel;
    SliceOrientation m_orientation;
    const char* m_viewName;
};

// 轴向视图（Axial - XY平面）
class AxialVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    AxialVtkItem();
};

// 矢状视图（Sagittal - YZ平面）
class SagittalVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    SagittalVtkItem();
};

// 冠状视图（Coronal - XZ平面）
class CoronalVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    CoronalVtkItem();
};

// 存储体渲染视图的VTK  
class VolumeViewData : public vtkObject
{
public:
    static VolumeViewData* New();
    vtkTypeMacro(VolumeViewData, vtkObject);

    vtkSmartPointer<vtkRenderer> renderer;

protected:
    VolumeViewData() {}
    ~VolumeViewData() override {}

private:
    VolumeViewData(const VolumeViewData&) = delete;
    void operator=(const VolumeViewData&) = delete;
};

// 3D体渲染视图
class VolumeVtkItem : public QQuickVTKItem
{
    Q_OBJECT
public:
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;

private slots:
    void onDataLoaded();
    void onSegDataLoaded();
    void onSegRefreshRenderer();

private:
    static void setupView(vtkRenderWindow* renderWindow, VolumeViewData* data, vtkImageData* imageData);
};

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
    Q_INVOKABLE void startAnalysisBrainAge(const QString& path, bool preprocess);
    Q_PROPERTY(QString fmriprepLog READ fmriprepLog NOTIFY fmriprepLogUpdated)
    QString fmriprepLog() const { return m_fmriprepLog; }
    // 获取表格模型
    BrainRegionTableModel* getBrainRegionTableModel() const;
    
signals:
    void brainAnalysisStarted();
    void brainAnalysisFinished(bool success);
    void networkTableIndexChanged(int index);
    void fmriprepLogUpdated();
    
private:
    bool loadOutputData(const QString& path);
    void processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir);
    void appendFmriprepLog(const QString& text);
    void startLogTimer(const QString& logFilePath);
    void stopLogTimer();
    
    BrainRegionTableModel* m_brainRegionTableModel;
    QPointer<QProcess> m_fmriprepProcess;
    qint64 m_fmriprepPid = -1;
    QString m_fmriprepLog;
    QString m_fmriprepLogFilePath;
    qint64 m_fmriprepLogReadPos = 0;
    QTimer* m_fmriprepLogTimer = nullptr;
};

