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

    void SetOrientation(SliceOrientation orientation) {
        m_orientation = orientation;
    }

    void OnMouseWheelForward() override {
        int currentSlice = getCurrentSlice();
        int maxSlice = getMaxSlice();
        if (currentSlice < maxSlice) {
            setSlice(currentSlice + 1);
        }
    }

    void OnMouseWheelBackward() override {
        int currentSlice = getCurrentSlice();
        if (currentSlice > 0) {
            setSlice(currentSlice - 1);
        }
    }

    void OnLeftButtonDown() override {
        m_isDragging = true;
        int* pos = this->GetInteractor()->GetEventPosition();
        m_lastX = pos[0];
        m_lastY = pos[1];
    }

    void OnLeftButtonUp() override {
        m_isDragging = false;
    }

    void OnMouseMove() override {
        if (m_isDragging) {
            int* pos = this->GetInteractor()->GetEventPosition();
            int dx = pos[0] - m_lastX;
            int dy = pos[1] - m_lastY;

            // 水平拖动调整窗宽，垂直拖动调整窗位
            double currentWidth = m_dataModel->windowWidth();
            double currentLevel = m_dataModel->windowLevel();

            double newWidth = currentWidth + dx * 4.0;
            double newLevel = currentLevel + dy * 4.0;

            if (newWidth < 1) newWidth = 1;

            m_dataModel->setWindowWidth(newWidth);
            m_dataModel->setWindowLevel(newLevel);

            m_lastX = pos[0];
            m_lastY = pos[1];
        }
    }

protected:
    SliceInteractorStyle() : m_orientation(SliceOrientation::Axial),
        m_isDragging(false), m_lastX(0), m_lastY(0) {
        m_dataModel = GET_SINGLETON(DicomDataModel);
    }

private:
    int getCurrentSlice() const {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->axialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->sagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->coronalSlice();
        }
        return 0;
    }

    int getMaxSlice() const {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->maxAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->maxSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->maxCoronalSlice();
        }
        return 0;
    }

    void setSlice(int slice) {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            m_dataModel->setAxialSlice(slice);
            break;
        case SliceOrientation::Sagittal:
            m_dataModel->setSagittalSlice(slice);
            break;
        case SliceOrientation::Coronal:
            m_dataModel->setCoronalSlice(slice);
            break;
        }
    }
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
    SliceVtkItemBase(SliceOrientation orientation, const char* viewName)
        : m_orientation(orientation), m_viewName(viewName) {
        m_dataModel = GET_SINGLETON(DicomDataModel);
    }

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        // 创建用户数据对象
        vtkSmartPointer<SliceViewData> data = vtkSmartPointer<SliceViewData>::New();

        // 连接信号（在GUI线程）
        connect(m_dataModel, &DicomDataModel::dataLoaded,
            this, &SliceVtkItemBase::onDataLoaded);
        connect(m_dataModel, &DicomDataModel::axialSliceChanged,
            this, &SliceVtkItemBase::onSliceChanged);
        connect(m_dataModel, &DicomDataModel::sagittalSliceChanged,
            this, &SliceVtkItemBase::onSliceChanged);
        connect(m_dataModel, &DicomDataModel::coronalSliceChanged,
            this, &SliceVtkItemBase::onSliceChanged);
        connect(m_dataModel, &DicomDataModel::windowWidthChanged,
            this, &SliceVtkItemBase::onWindowChanged);
        connect(m_dataModel, &DicomDataModel::windowLevelChanged,
            this, &SliceVtkItemBase::onWindowChanged);

        // 在渲染线程初始化VTK对象
        vtkSmartPointer<vtkImageData> imageData = m_dataModel->getImageData();
        if (imageData) {
            setupView(renderWindow, data, imageData);
        }

        return data;
    }

private slots:
    void onDataLoaded() {
        // 使用 dispatch_async 在渲染线程更新VTK对象
        dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
            vtkSmartPointer<vtkImageData> imageData = m_dataModel->getImageData();
            if (imageData && userData) {
                SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
                setupView(rw, data, imageData);
            }
            });
        scheduleRender();
    }

    void onSliceChanged(int slice) {
        Q_UNUSED(slice);
        // 使用 dispatch_async 在渲染线程更新切片
        dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
            Q_UNUSED(rw);
            if (userData) {
                SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
                if (data->imageMapper) {
                    data->imageMapper->SetSliceNumber(getCurrentSlice());
                    data->imageMapper->Modified();
                }
            }
            });
        scheduleRender();
    }

    void onWindowChanged() {
        // 使用 dispatch_async 在渲染线程更新窗宽窗位
        dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
            Q_UNUSED(rw);
            if (userData) {
                SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
                if (data->imageSlice) {
                    data->imageSlice->GetProperty()->SetColorWindow(
                        m_dataModel->windowWidth());
                    data->imageSlice->GetProperty()->SetColorLevel(
                        m_dataModel->windowLevel());
                    data->imageSlice->GetProperty()->Modified();
                    updateWWWLText(data);
                }
            }
            });
        scheduleRender();
    }

private:
    void setupView(vtkRenderWindow* renderWindow, SliceViewData* data, vtkImageData* imageData) {
        // 清除旧的渲染器
        renderWindow->GetRenderers()->RemoveAllItems();

        // 创建图像切片
        data->imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        data->imageMapper->SetInputData(imageData);
        setMapperOrientation(data->imageMapper);
        data->imageMapper->SetSliceNumber(getCurrentSlice());

        data->imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        data->imageSlice->SetMapper(data->imageMapper);
        data->imageSlice->GetProperty()->SetColorWindow(m_dataModel->windowWidth());
        data->imageSlice->GetProperty()->SetColorLevel(m_dataModel->windowLevel());

        // 创建渲染器
        data->renderer = vtkSmartPointer<vtkRenderer>::New();
        data->renderer->AddViewProp(data->imageSlice);
        data->renderer->SetBackground(0, 0, 0);

        // 添加标题文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput(m_viewName);
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        data->renderer->AddActor2D(textActor);

        // 添加窗宽窗位显示文本（右下角）
        data->wwwlTextMapper = vtkSmartPointer<vtkTextMapper>::New();
        updateWWWLText(data);
        data->wwwlTextMapper->GetTextProperty()->SetFontSize(16);
        data->wwwlTextMapper->GetTextProperty()->SetColor(0.0, 1.0, 0.0);
        data->wwwlTextMapper->GetTextProperty()->SetJustificationToRight();

        data->wwwlTextActor = vtkSmartPointer<vtkActor2D>::New();
        data->wwwlTextActor->SetMapper(data->wwwlTextMapper);
        data->wwwlTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        data->wwwlTextActor->SetPosition(0.98, 0.02);
        data->renderer->AddActor2D(data->wwwlTextActor);

        renderWindow->AddRenderer(data->renderer);

        // 设置交互样式
        vtkSmartPointer<SliceInteractorStyle> style = vtkSmartPointer<SliceInteractorStyle>::New();
        style->SetOrientation(m_orientation);
        renderWindow->GetInteractor()->SetInteractorStyle(style);

        // 设置相机方向
        setupCamera(data->renderer);
    }

    void setMapperOrientation(vtkImageSliceMapper* mapper) {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            mapper->SetOrientationToZ();
            break;
        case SliceOrientation::Sagittal:
            mapper->SetOrientationToX();
            break;
        case SliceOrientation::Coronal:
            mapper->SetOrientationToY();
            break;
        }
    }
    void setupCamera(vtkRenderer* renderer) {
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();

        switch (m_orientation) {
        case SliceOrientation::Axial:
            camera->SetViewUp(0, 1, 0);
            camera->SetPosition(0, 0, 1);
            camera->SetFocalPoint(0, 0, 0);
            break;
        case SliceOrientation::Sagittal:
            camera->SetViewUp(0, 0, -1);
            camera->SetPosition(1, 0, 0);
            camera->SetFocalPoint(0, 0, 0);
            break;
        case SliceOrientation::Coronal:
            camera->SetViewUp(0, 0, -1);
            camera->SetPosition(0, -1, 0);
            camera->SetFocalPoint(0, 0, 0);
            break;
        }

        renderer->ResetCamera();
    }

    int getCurrentSlice() const {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            return m_dataModel->axialSlice();
        case SliceOrientation::Sagittal:
            return m_dataModel->sagittalSlice();
        case SliceOrientation::Coronal:
            return m_dataModel->coronalSlice();
        }
        return 0;
    }

    void updateWWWLText(SliceViewData* data) {
        if (data->wwwlTextMapper) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f",
                m_dataModel->windowWidth(),
                m_dataModel->windowLevel());
            data->wwwlTextMapper->SetInput(buffer);
        }
    }
    DicomDataModel* m_dataModel;
    SliceOrientation m_orientation;
    const char* m_viewName;
};

// 轴向视图（Axial - XY平面）
class AxialVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    AxialVtkItem() : SliceVtkItemBase(SliceOrientation::Axial, "Axial View") {}
};

// 矢状视图（Sagittal - YZ平面）
class SagittalVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    SagittalVtkItem() : SliceVtkItemBase(SliceOrientation::Sagittal, "Sagittal View") {}
};

// 冠状视图（Coronal - XZ平面）
class CoronalVtkItem : public SliceVtkItemBase
{
    Q_OBJECT
public:
    CoronalVtkItem() : SliceVtkItemBase(SliceOrientation::Coronal, "Coronal View") {}
};

// 存储体渲染视图的VTK对象
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
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        // 创建用户数据对象
        vtkSmartPointer<VolumeViewData> data = vtkSmartPointer<VolumeViewData>::New();

        // 连接信号
        connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::dataLoaded,
            this, &VolumeVtkItem::onDataLoaded);

        // 在渲染线程初始化VTK对象
        vtkSmartPointer<vtkImageData> imageData = GET_SINGLETON(DicomDataModel)->getImageData();
        if (imageData) {
            setupView(renderWindow, data, imageData);
        }

        return data;
    }

private slots:
    void onDataLoaded() {
        // 使用 dispatch_async 在渲染线程更新VTK对象
        dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
            vtkSmartPointer<vtkImageData> imageData = GET_SINGLETON(DicomDataModel)->getImageData();
            if (imageData && userData) {
                VolumeViewData* data = static_cast<VolumeViewData*>(userData.GetPointer());
                setupView(rw, data, imageData);
            }
            });
        scheduleRender();
    }

private:
    static void setupView(vtkRenderWindow* renderWindow, VolumeViewData* data, vtkImageData* imageData) {
        // 清除旧的渲染器
        renderWindow->GetRenderers()->RemoveAllItems();

        // 创建体渲染映射器
        vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper =
            vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        volumeMapper->SetInputData(imageData);

        // 颜色传输函数
        vtkSmartPointer<vtkColorTransferFunction> colorFunc =
            vtkSmartPointer<vtkColorTransferFunction>::New();
        colorFunc->AddRGBPoint(-3024, 0.0, 0.0, 0.0);
        colorFunc->AddRGBPoint(-77, 0.54902, 0.25098, 0.14902);
        colorFunc->AddRGBPoint(94, 0.882353, 0.603922, 0.290196);
        colorFunc->AddRGBPoint(179, 1.0, 0.937033, 0.954531);
        colorFunc->AddRGBPoint(260, 0.615686, 0.0, 0.0);
        colorFunc->AddRGBPoint(3071, 0.827451, 0.658824, 1.0);

        // 不透明度传输函数
        vtkSmartPointer<vtkPiecewiseFunction> opacityFunc =
            vtkSmartPointer<vtkPiecewiseFunction>::New();
        opacityFunc->AddPoint(-3024, 0.0);
        opacityFunc->AddPoint(-77, 0.0);
        opacityFunc->AddPoint(94, 0.29);
        opacityFunc->AddPoint(179, 0.55);
        opacityFunc->AddPoint(260, 0.84);
        opacityFunc->AddPoint(3071, 0.875);

        // 梯度不透明度函数
        vtkSmartPointer<vtkPiecewiseFunction> gradientFunc =
            vtkSmartPointer<vtkPiecewiseFunction>::New();
        gradientFunc->AddPoint(0, 0.0);
        gradientFunc->AddPoint(90, 0.5);
        gradientFunc->AddPoint(100, 1.0);

        // 体属性
        vtkSmartPointer<vtkVolumeProperty> volumeProperty =
            vtkSmartPointer<vtkVolumeProperty>::New();
        volumeProperty->SetColor(colorFunc);
        volumeProperty->SetScalarOpacity(opacityFunc);
        volumeProperty->SetGradientOpacity(gradientFunc);
        volumeProperty->SetInterpolationTypeToLinear();
        volumeProperty->ShadeOn();
        volumeProperty->SetAmbient(0.4);
        volumeProperty->SetDiffuse(0.6);
        volumeProperty->SetSpecular(0.2);

        vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
        volume->SetMapper(volumeMapper);
        volume->SetProperty(volumeProperty);

        data->renderer = vtkSmartPointer<vtkRenderer>::New();
        data->renderer->AddVolume(volume);
        data->renderer->SetBackground(0, 0, 0);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("3D Volume Rendering");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        data->renderer->AddActor2D(textActor);

        renderWindow->AddRenderer(data->renderer);

        // 设置3D交互样式
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
            vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        renderWindow->GetInteractor()->SetInteractorStyle(style);
        data->renderer->GetActiveCamera()->SetViewUp(0, 0, -1);
        data->renderer->GetActiveCamera()->SetPosition(0, 1, 0);
        data->renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
        data->renderer->ResetCamera();
    }
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

        QUICK_PROPERTY(QString, currentAlffUrl)
        QUICK_PROPERTY(QString, currentCovarianceUrl)
        QUICK_PROPERTY(QString, currentRegionplotsUrl)
        QUICK_PROPERTY(QString, currentViewConnectomeUrl)
public:
    Q_INVOKABLE void calculateKidney();
    Q_INVOKABLE void importBrainData(const QString& url);
    Q_INVOKABLE void selectBrainRegion(int row);
    
    // 获取表格模型
    BrainRegionTableModel* getBrainRegionTableModel() const { return m_brainRegionTableModel; }
    
signals:
    void errorMsg(const QString& warning);
    void brainAnalysisStarted();
    void brainAnalysisProgress(const QString& message);
    void brainAnalysisFinished(bool success);
    
private:
    bool loadOutputData(const QString& path);
    void processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir);
    
    BrainRegionTableModel* m_brainRegionTableModel;
};

