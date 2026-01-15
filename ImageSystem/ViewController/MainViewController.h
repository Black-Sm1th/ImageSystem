#pragma once
#include "Model/DicomDataModel.h"
#include "Model/BrainRegionTableModel.h"
#include "Model/BrainSegmentationTableModel.h"
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
#include <vtkAxisActor2D.h>
#include <vtkProperty2D.h>
#include <memory>
#include <optional>
#include <map>

// 枚举类型：切片方向
enum class SliceOrientation : int {
    Axial,    // 轴向 (Z方向)
    Sagittal, // 矢状 (X方向)
    Coronal   // 冠状 (Y方向)
};

#include "ViewController/InteractionState.h"

// 通用交互样式类 - 减少重复代码
class SliceInteractorStyle : public vtkInteractorStyleImage
{
public:
    static SliceInteractorStyle* New();
    vtkTypeMacro(SliceInteractorStyle, vtkInteractorStyleImage);

    void SetOrientation(SliceOrientation orientation);
    void SetToolMode(ToolMode mode);
    ToolMode toolMode() const { return m_toolMode; }
	void setAxisActor(vtkSmartPointer<vtkAxisActor2D> axisActor);
	void rescaleAxisActor();

    void OnMouseWheelForward() override;
    void OnMouseWheelBackward() override;
    void OnLeftButtonDown() override;
    void OnLeftButtonUp() override;
    void OnRightButtonDown() override;
    void OnRightButtonUp() override;
    void OnMiddleButtonDown() override;
    void OnMiddleButtonUp() override;
    void OnMouseMove() override;
    void OnKeyPress() override;

    // Reset：清除测量/取消当前 state（相机重置由视图层完成）
    void ResetInteractionState();
    void UpdateMeasurementVisibility(int sliceNumber, bool segMode);

    // 设置标注完成回调
    void SetAnnotationCallback(std::function<void(double screenX, double screenY, int annotationIndex, SliceOrientation orientation, int annotationType)> callback);

    // 更新标注文字
    void UpdateAnnotationText(int index, const std::string& text);

    // 删除标注
    void DeleteAnnotation(int index);

    // 更新圆形标注文字
    void UpdateCircleAnnotationText(int index, const std::string& text);

    // 删除圆形标注
    void DeleteCircleAnnotation(int index);

protected:
    SliceInteractorStyle();

private:
    int getCurrentSlice() const;
    int getMaxSlice() const;
    void setSlice(int slice);
    void BeginInteraction(std::optional<ToolMode> forcedMode);
    DicomDataModel* m_dataModel;
    SliceOrientation m_orientation;
	vtkSmartPointer<vtkAxisActor2D> m_axisActor;

    // 交互框架：当前工具 + 当前状态 + 上下文
    ToolMode m_toolMode{ ToolMode::WindowLevel };
    std::unique_ptr<IInteractionState> m_state;
    InteractionContext m_ctx;
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
	vtkSmartPointer<vtkAxisActor2D> axisActor;
    vtkSmartPointer<SliceInteractorStyle> interactorStyle;

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

    // 获取指定方向的interactor style
    static SliceInteractorStyle* GetInteractorStyle(SliceOrientation orientation);

private slots:
    void onDataLoaded();
    void onSegDataLoaded();
    void onSegRefreshRenderer();  // 分割视图刷新
    void onSliceChanged(int slice);
    void onSegSliceChanged(int slice);
    void onWindowChanged();
    void onInteractionResetRequested();
    void onCrosshairEnabledChanged(bool enabled);

private:
    void setupView(vtkRenderWindow* renderWindow, SliceViewData* data, vtkImageData* imageData);
    void applyParallelScale(vtkImageSlice* imageSlice, vtkRenderer* renderer);
    void setMapperOrientation(vtkImageSliceMapper* mapper);
    void setupCamera(vtkRenderer* renderer);
    void resetViewState(vtkRenderWindow* rw, SliceViewData* data);

    int getCurrentSlice() const;
    int getSegCurrentSlice() const;

	

    void updateWWWLText(SliceViewData* data);
    DicomDataModel* m_dataModel;
    SliceOrientation m_orientation;
    const char* m_viewName;
	vtkSmartPointer<vtkAxisActor2D> m_axisActor;

    // 静态映射：方向 -> interactor style
    static std::map<SliceOrientation, vtkSmartPointer<SliceInteractorStyle>> s_interactorStyles;
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
    void onInteractionResetRequested();

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
    Q_PROPERTY(QString fmriprepLog READ fmriprepLog NOTIFY fmriprepLogUpdated)
    QString fmriprepLog() const { return m_fmriprepLog; }
    Q_PROPERTY(QString deepprepLog READ deepprepLog NOTIFY deepprepLogUpdated)
    QString deepprepLog() const { return m_deepprepLog; }
    // 获取表格模型
    BrainRegionTableModel* getBrainRegionTableModel() const;
    BrainSegmentationTableModel* getBrainSegmentationTableModel() const;
    
signals:
    void brainAnalysisStarted();
    void brainAnalysisFinished(bool success);
    void networkTableIndexChanged(int index);
    void fmriprepLogUpdated();
    void deepprepLogUpdated();
    void annotationCreated(double screenX, double screenY, int annotationIndex, int orientation, int annotationType);
    
private:
    bool loadOutputData(const QString& path);
    void processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir);
    void appendFmriprepLog(const QString& text);
    void startLogTimer(const QString& logFilePath);
    void stopLogTimer();
    void appendDeepprepLog(const QString& text);
    void startDeepprepLogTimer(const QString& logFilePath);
    void stopDeepprepLogTimer();
    
    BrainRegionTableModel* m_brainRegionTableModel;
    BrainSegmentationTableModel* m_brainSegmentationTableModel;
    QPointer<QProcess> m_fmriprepProcess;
    qint64 m_fmriprepPid = -1;
    QString m_fmriprepLog;
    QString m_fmriprepLogFilePath;
    qint64 m_fmriprepLogReadPos = 0;
    QTimer* m_fmriprepLogTimer = nullptr;
    
    QPointer<QProcess> m_deepprepProcess;
    qint64 m_deepprepPid = -1;
    QString m_deepprepLog;
    QString m_deepprepLogFilePath;
    qint64 m_deepprepLogReadPos = 0;
    QTimer* m_deepprepLogTimer = nullptr;
};

