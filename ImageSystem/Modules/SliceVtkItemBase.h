#pragma once
#include "Model/DicomDataModel.h"
#include "Modules/InteractionState.h"
#include <vtkInteractorStyleImage.h>
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

    // 更新画笔标注文字
    void UpdatePenAnnotationText(int index, const std::string& text);

    // 删除画笔标注
    void DeletePenAnnotation(int index);

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
    void onScreenshotRequested(int viewType, QString filePath);

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
signals:
    void messageRequest(bool success, const QString& msg);
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

    // 获取3D视图的渲染窗口
    static vtkRenderWindow* GetVolumeRenderWindow();

private slots:
    void onDataLoaded();
    void onSegDataLoaded();
    void onSegRefreshRenderer();
    void onInteractionResetRequested();
    void onScreenshotRequested(int viewType, QString filePath);

private:
    static void setupView(vtkRenderWindow* renderWindow, VolumeViewData* data, vtkImageData* imageData);
};

