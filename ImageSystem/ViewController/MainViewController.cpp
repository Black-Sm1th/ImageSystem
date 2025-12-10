#include "MainViewController.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include "Modules/BrainNetworkData.h"
vtkStandardNewMacro(SliceInteractorStyle);
vtkStandardNewMacro(SliceViewData);
vtkStandardNewMacro(VolumeViewData);

// ===== SliceInteractorStyle implementations =====
SliceInteractorStyle::SliceInteractorStyle()
    : m_orientation(SliceOrientation::Axial),
      m_isDragging(false),
      m_lastX(0),
      m_lastY(0)
{
    m_dataModel = GET_SINGLETON(DicomDataModel);
}

void SliceInteractorStyle::SetOrientation(SliceOrientation orientation)
{
    m_orientation = orientation;
}

void SliceInteractorStyle::OnMouseWheelForward()
{
    int currentSlice = getCurrentSlice();
    int maxSlice = getMaxSlice();
    if (currentSlice < maxSlice) {
        setSlice(currentSlice + 1);
    }
}

void SliceInteractorStyle::OnMouseWheelBackward()
{
    int currentSlice = getCurrentSlice();
    if (currentSlice > 0) {
        setSlice(currentSlice - 1);
    }
}

void SliceInteractorStyle::OnLeftButtonDown()
{
    m_isDragging = true;
    int* pos = this->GetInteractor()->GetEventPosition();
    m_lastX = pos[0];
    m_lastY = pos[1];
}

void SliceInteractorStyle::OnLeftButtonUp()
{
    m_isDragging = false;
}

void SliceInteractorStyle::OnMouseMove()
{
    if (m_isDragging && !m_dataModel->isSegDataMode()) {
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

int SliceInteractorStyle::getCurrentSlice() const
{
    // 根据当前数据模式选择切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->segAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->segSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->segCoronalSlice();
        }
    } else {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->axialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->sagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->coronalSlice();
        }
    }
    return 0;
}

int SliceInteractorStyle::getMaxSlice() const
{
    // 根据当前数据模式选择最大切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->maxSegAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->maxSegSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->maxSegCoronalSlice();
        }
    } else {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->maxAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->maxSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->maxCoronalSlice();
        }
    }
    return 0;
}

void SliceInteractorStyle::setSlice(int slice)
{
    // 根据当前数据模式设置切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            m_dataModel->setSegAxialSlice(slice);
            break;
        case SliceOrientation::Sagittal:
            m_dataModel->setSegSagittalSlice(slice);
            break;
        case SliceOrientation::Coronal:
            m_dataModel->setSegCoronalSlice(slice);
            break;
        }
    } else {
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
}

// ===== SliceVtkItemBase implementations =====
SliceVtkItemBase::SliceVtkItemBase(SliceOrientation orientation, const char* viewName)
    : m_orientation(orientation),
      m_viewName(viewName)
{
    m_dataModel = GET_SINGLETON(DicomDataModel);
}

QQuickVTKItem::vtkUserData SliceVtkItemBase::initializeVTK(vtkRenderWindow* renderWindow)
{
    // 创建用户数据对象
    vtkSmartPointer<SliceViewData> data = vtkSmartPointer<SliceViewData>::New();

    // 连接信号（在GUI线程）
    connect(m_dataModel, &DicomDataModel::dataLoaded,
        this, &SliceVtkItemBase::onDataLoaded);
    connect(m_dataModel, &DicomDataModel::segDataLoaded,
        this, &SliceVtkItemBase::onSegDataLoaded);
    connect(m_dataModel, &DicomDataModel::axialSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    connect(m_dataModel, &DicomDataModel::sagittalSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    connect(m_dataModel, &DicomDataModel::coronalSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    // 连接SegData切片变化信号
    connect(m_dataModel, &DicomDataModel::segAxialSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
    connect(m_dataModel, &DicomDataModel::segSagittalSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
    connect(m_dataModel, &DicomDataModel::segCoronalSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
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

void SliceVtkItemBase::onDataLoaded()
{
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

void SliceVtkItemBase::onSegDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            // 清除旧的渲染器
            rw->GetRenderers()->RemoveAllItems();

            switch (m_orientation) {
            case SliceOrientation::Axial:
                data->imageSlice = m_dataModel->getSegImageData(0);
                break;
            case SliceOrientation::Sagittal:
                data->imageSlice = m_dataModel->getSegImageData(2);
                break;
            case SliceOrientation::Coronal:
                data->imageSlice = m_dataModel->getSegImageData(1);
                break;
            }

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

            rw->AddRenderer(data->renderer);

            // 设置交互样式
            vtkSmartPointer<SliceInteractorStyle> style = vtkSmartPointer<SliceInteractorStyle>::New();
            style->SetOrientation(m_orientation);
            rw->GetInteractor()->SetInteractorStyle(style);

            // 设置相机方向
            setupCamera(data->renderer);
            applyParallelScale(data->imageSlice, data->renderer);
        }
    });
    scheduleRender();
}

void SliceVtkItemBase::onSliceChanged(int slice)
{
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

void SliceVtkItemBase::onSegSliceChanged(int slice)
{
    Q_UNUSED(slice);
    // 使用 dispatch_async 在渲染线程更新SegData切片
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        Q_UNUSED(rw);
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            if (data->imageSlice && data->imageSlice->GetMapper()) {
                vtkImageSliceMapper* mapper = vtkImageSliceMapper::SafeDownCast(data->imageSlice->GetMapper());
                if (mapper) {
                    mapper->SetSliceNumber(getSegCurrentSlice());
                    mapper->Modified();
                }
            }
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onWindowChanged()
{
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

void SliceVtkItemBase::setupView(vtkRenderWindow* renderWindow, SliceViewData* data, vtkImageData* imageData)
{
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

    // 设置相机方向并锁定并行缩放（保持视口尺寸稳定）
    setupCamera(data->renderer);
    applyParallelScale(data->imageSlice, data->renderer);
}

void SliceVtkItemBase::setMapperOrientation(vtkImageSliceMapper* mapper)
{
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

void SliceVtkItemBase::setupCamera(vtkRenderer* renderer)
{
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

void SliceVtkItemBase::applyParallelScale(vtkImageSlice* imageSlice, vtkRenderer* renderer)
{
    if (!imageSlice || !renderer) {
        return;
    }
    vtkCamera* cam = renderer->GetActiveCamera();
    if (!cam) {
        return;
    }
    cam->SetParallelProjection(true);

    auto* mapper = vtkImageSliceMapper::SafeDownCast(imageSlice->GetMapper());
    if (!mapper) {
        return;
    }
    auto* img = vtkImageData::SafeDownCast(mapper->GetInput());
    if (!img) {
        return;
    }

    int extent[6];
    double spacing[3];
    img->GetExtent(extent);
    img->GetSpacing(spacing);

    double w = (extent[1] - extent[0] + 1) * spacing[0];
    double h = (extent[3] - extent[2] + 1) * spacing[1];
    if (m_orientation == SliceOrientation::Sagittal) {
        w = (extent[3] - extent[2] + 1) * spacing[1];
        h = (extent[5] - extent[4] + 1) * spacing[2];
    } else if (m_orientation == SliceOrientation::Coronal) {
        w = (extent[1] - extent[0] + 1) * spacing[0];
        h = (extent[5] - extent[4] + 1) * spacing[2];
    }

    // 让切片占用视口 80%（可按需调整）
    const double targetFill = 0.8;
    cam->SetParallelScale(0.5 * std::max(w, h) / targetFill);
}

int SliceVtkItemBase::getCurrentSlice() const
{
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

int SliceVtkItemBase::getSegCurrentSlice() const
{
    switch (m_orientation) {
    case SliceOrientation::Axial:
        return m_dataModel->segAxialSlice();
    case SliceOrientation::Sagittal:
        return m_dataModel->segSagittalSlice();
    case SliceOrientation::Coronal:
        return m_dataModel->segCoronalSlice();
    }
    return 0;
}

void SliceVtkItemBase::updateWWWLText(SliceViewData* data)
{
    if (data->wwwlTextMapper) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f",
            m_dataModel->windowWidth(),
            m_dataModel->windowLevel());
        data->wwwlTextMapper->SetInput(buffer);
    }
}

// ===== Derived Slice Items =====
AxialVtkItem::AxialVtkItem()
    : SliceVtkItemBase(SliceOrientation::Axial, "Axial View")
{
}

SagittalVtkItem::SagittalVtkItem()
    : SliceVtkItemBase(SliceOrientation::Sagittal, "Sagittal View")
{
}

CoronalVtkItem::CoronalVtkItem()
    : SliceVtkItemBase(SliceOrientation::Coronal, "Coronal View")
{
}

// ===== VolumeVtkItem implementations =====
QQuickVTKItem::vtkUserData VolumeVtkItem::initializeVTK(vtkRenderWindow* renderWindow)
{
    // 创建用户数据对象
    vtkSmartPointer<VolumeViewData> data = vtkSmartPointer<VolumeViewData>::New();

    // 连接信号
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::dataLoaded,
        this, &VolumeVtkItem::onDataLoaded);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::segDataLoaded,
        this, &VolumeVtkItem::onSegDataLoaded);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::segRefreshRenderer,
        this, &VolumeVtkItem::onSegRefreshRenderer);
    // 在渲染线程初始化VTK对象
    vtkSmartPointer<vtkImageData> imageData = GET_SINGLETON(DicomDataModel)->getImageData();
    if (imageData) {
        setupView(renderWindow, data, imageData);
    }

    return data;
}

void VolumeVtkItem::onDataLoaded()
{
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

void VolumeVtkItem::onSegRefreshRenderer()
{
    scheduleRender();
}

void VolumeVtkItem::onSegDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
        VolumeViewData* data = static_cast<VolumeViewData*>(userData.GetPointer());
        data->renderer = GET_SINGLETON(DicomDataModel)->getSeg3DRenderer();
        data->renderer->SetBackground(0, 0, 0);
        rw->AddRenderer(data->renderer);

        // 设置3D交互样式
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
            vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        rw->GetInteractor()->SetInteractorStyle(style);
        data->renderer->GetActiveCamera()->SetViewUp(0, 0, -1);
        data->renderer->GetActiveCamera()->SetPosition(0, 1, 0);
        data->renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
        data->renderer->ResetCamera();
        });
    scheduleRender();
}

void VolumeVtkItem::setupView(vtkRenderWindow* renderWindow, VolumeViewData* data, vtkImageData* imageData)
{
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
MainViewController::MainViewController(QObject* parent)
    : QObject(parent)
{
    setader(-1);
    sett2(-1);
    setskin(-1);
    setmicro(-1);
    setsei(-1);
    setdisp(-1);
    setcclsResult(0.0);
    setccrccResult(0.0);
    setcurrentAlffUrl("");
    setcurrentCovarianceUrl("");
    setcurrentRegionplotsUrl("");
    setcurrentViewConnectomeUrl("");
    setglobalEfficiency(0.0);
    setaverageLocalEfficiency(0.0);
    setaverageClusteringCoefficient(0.0);
    setrichClubConnections(0.0);
    setbridgeConnections(0.0);
    setlocalConnections(0.0);
    
    // 初始化表格模型
    m_brainRegionTableModel = new BrainRegionTableModel(this);

    // 应用退出时停止 fmriprep 进程并停止日志轮询
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() {
                    stopFmriprepProcess();
                });
    }
}

void MainViewController::calculateKidney() {
    // 检查所有参数是否已设置
    if (gett2() == -1 || getskin() == -1 || getmicro() == -1 ||
        getsei() == -1 || getader() == -1 || getdisp() == -1) {
        qWarning() << QStringLiteral("部分参数未设置，无法计算");
        return;
    }

    // 构建Python程序路径
    QString pythonPath = "Scripts/kidney_processor.exe";
    
    // 准备参数
    QStringList arguments;
    arguments << QString::number(gett2())
              << QString::number(getskin())
              << QString::number(getmicro())
              << QString::number(getsei())
              << QString::number(getader())
              << QString::number(getdisp());

    // 创建进程
    QProcess* process = new QProcess(this);
    
    // 连接信号
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                QString output = QString::fromUtf8(process->readAllStandardOutput());
                QString error = QString::fromUtf8(process->readAllStandardError());
                // 解析输出结果
                QStringList lines = output.split('\n', Qt::SkipEmptyParts);
                for (const QString& line : lines) {
                    if (line.contains("result ccls:", Qt::CaseInsensitive)) {
                        QStringList parts = line.split(':');
                        if (parts.size() >= 2) {
                            bool ok;
                            double value = parts[1].trimmed().toDouble(&ok);
                            if (ok) {
                                setcclsResult(value);
                                qDebug() << QStringLiteral("CCLS结果:") << value;
                            }
                        }
                    } else if (line.contains("result ccrcc:", Qt::CaseInsensitive)) {
                        QStringList parts = line.split(':');
                        if (parts.size() >= 2) {
                            bool ok;
                            double value = parts[1].trimmed().toDouble(&ok);
                            if (ok) {
                                setccrccResult(value);
                                qDebug() << QStringLiteral("CCRCC结果:") << value;
                            }
                        }
                    }
                }
                
                if (!error.isEmpty()) {
                    qDebug() << QStringLiteral("错误:") << error;
                }
            } else {
                qWarning() << QStringLiteral("计算失败！退出代码:") << exitCode;
                qWarning() << QStringLiteral("错误信息:") << QString::fromUtf8(process->readAllStandardError());
            }
            process->deleteLater();
        });
    
    connect(process, &QProcess::errorOccurred, [=](QProcess::ProcessError error) {
        qWarning() << QStringLiteral("进程错误:") << error;
        qWarning() << QStringLiteral("错误信息:") << process->errorString();
        process->deleteLater();
    });

    // 启动进程
    qDebug() << QStringLiteral("启动计算程序:") << pythonPath;
    qDebug() << QStringLiteral("参数:") << arguments;
    process->start(pythonPath, arguments);
}

void MainViewController::importBrainData(const QString& url)
{
    if (url.isEmpty()) {
        emit errorMsg(QStringLiteral("路径为空"));
        return;
    }
    
    QString dirPath = url;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }
    
    QDir baseDir(dirPath);
    if (!baseDir.exists()) {
        emit errorMsg(QStringLiteral("路径不存在: ") + dirPath);
        return;
    }
    
    // ========== 逻辑一：检查是否存在完整的输出结果 ==========
    QDir outputDir(baseDir.filePath("outputDir"));
    if (outputDir.exists()) {
        bool hasAllFiles = true;
        
        // 检查必需的文件
        QStringList requiredFiles = {
            "alff.png",
            "brain_network_results.json",
            "covariance.png",
            "viewConnectome.html"
        };
        
        for (const QString& fileName : requiredFiles) {
            if (!QFile::exists(outputDir.filePath(fileName))) {
                hasAllFiles = false;
                qDebug() << QStringLiteral("缺失文件:") << fileName;
                break;
            }
        }
        
        // 检查 region_plots 文件夹
        if (hasAllFiles) {
            QDir regionPlotsDir(outputDir.filePath("region_plots"));
            if (regionPlotsDir.exists()) {
                // 获取所有图片文件（支持常见图片格式）
                QStringList filters;
                filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.gif";
                QStringList imageFiles = regionPlotsDir.entryList(filters, QDir::Files);
                
                if (imageFiles.count() == 116) {
                    // ========== 符合逻辑一：已有完整的处理结果 ==========
                    qDebug() << QStringLiteral("检测到完整的脑网络分析结果!!!");
                    
                    if (loadOutputData(outputDir.absolutePath())) {
                        return;
                    }
                } else {
                    qDebug() << QStringLiteral("region_plots 图片数量不正确: ") << imageFiles.count() << QStringLiteral(" (期望: 116)");
                    hasAllFiles = false;
                }
            } else {
                qDebug() << QStringLiteral("region_plots 文件夹不存在");
                hasAllFiles = false;
            }
        }
    }
    
    // ========== 逻辑二：检查原始数据文件是否存在 ==========
    QString boldPath = baseDir.filePath("sub-01/func/sub-01_task-rest_space-MNI152NLin2009cAsym_desc-preproc_bold.nii.gz");
    QString confoundsPath = baseDir.filePath("sub-01/func/sub-01_task-rest_desc-confounds_timeseries.tsv");
    
    if (QFile::exists(boldPath) && QFile::exists(confoundsPath)) {
        // ========== 符合逻辑二：原始数据文件存在，需要处理 ==========
        qDebug() << QStringLiteral("检测到原始脑功能数据文件!!!");
        // 创建输出目录
        QString outputDir = baseDir.filePath("outputDir");
        QDir outputDirObj(outputDir);
        if (!outputDirObj.exists()) {
            if (!outputDirObj.mkpath(".")) {
                emit errorMsg(QStringLiteral("无法创建输出目录: ") + outputDir);
                return;
            }
        }
        
        // 调用 Python 脚本进行脑网络分析
        processBrainNetworkAnalysis(boldPath, confoundsPath, outputDir);
        
        return;
    }
    
    // ========== 不符合任何逻辑，发出错误信号 ==========
    QString errorMessage = QStringLiteral("未找到有效的脑功能数据!!!");
    
    emit errorMsg(errorMessage);
}

void MainViewController::importBrainSegData(const QString& url)
{
    
}

bool MainViewController::loadOutputData(const QString& path)
{
    BrainNetworkData networkData;
    if (networkData.loadFromFolder(path)) {
        setcurrentAlffUrl("file:///" + path + "/alff.png");
        setcurrentCovarianceUrl("file:///" + path + "/covariance.png");
        setcurrentViewConnectomeUrl("file:///" + path + "/viewConnectome.html");

        setglobalEfficiency(networkData.globalEfficiency());
        setaverageLocalEfficiency(networkData.averageLocalEff());
        setaverageClusteringCoefficient(networkData.averageClustering());
        setrichClubConnections(networkData.richClubPercentage());
        setbridgeConnections(networkData.bridgePercentage());
        setlocalConnections(networkData.localPercentage());
        
        // 加载表格数据
        m_brainRegionTableModel->loadRegions(networkData.allRegions(), path);
        
        // 默认选中第一个脑区
        if (networkData.regionCount() > 0) {
            selectBrainRegion(0);
        }
        
        return true;
    }else{
        return false;
    }
}

void MainViewController::selectBrainRegion(int row)
{
    if (!m_brainRegionTableModel || row < 0 || row >= m_brainRegionTableModel->rowCount())
        return;
    
    // 获取该行的图片路径
    QModelIndex index = m_brainRegionTableModel->index(row, 0);
    QString imagePath = m_brainRegionTableModel->data(index, BrainRegionTableModel::ImagePathRole).toString();
    emit networkTableIndexChanged(row);
    qDebug() << QStringLiteral("选中脑区:") << row << imagePath;
    setcurrentRegionplotsUrl(imagePath);
}

void MainViewController::startfmriprepAnalysis(const QString& dicomDir,
                                               const QString& bidsDir,
                                               const QString& outputDir,
                                               const QString& licenseFile,
                                               bool useFreesurfer)
{
    // 如果已有进程在跑，先停止
    stopFmriprepProcess();

    QString exePath = "Scripts/run_fmriprep.exe";

    QStringList arguments;
    arguments << "--dicom_dir" << dicomDir
              << "--bids_dir" << bidsDir
              << "--output_dir" << outputDir
              << "--fs_license_file" << licenseFile;
    if (useFreesurfer) {
        arguments << "--freesurfer";
    }

    m_fmriprepProcess = new QProcess(this);

    // 清空日志，记录路径，重置读取位置
    clearFmriprepLog();
    m_fmriprepLogFilePath = outputDir + "/fmriprep-docker.log";
    m_fmriprepLogReadPos = 0;
    m_fmriprepPid = -1;

    // 实时读取进程输出
    connect(m_fmriprepProcess, &QProcess::readyReadStandardOutput, this, [=]() {
        appendFmriprepLog(QString::fromUtf8(m_fmriprepProcess->readAllStandardOutput()));
    });
    connect(m_fmriprepProcess, &QProcess::readyReadStandardError, this, [=]() {
        appendFmriprepLog(QString::fromUtf8(m_fmriprepProcess->readAllStandardError()));
    });

    connect(m_fmriprepProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        Q_UNUSED(error);
        QString errorOutput = QString::fromUtf8(m_fmriprepProcess->readAllStandardError());
        emit errorMsg(QStringLiteral("无法启动 fmriprep！\n%1").arg(errorOutput));
        stopFmriprepProcess();
        stopLogTimer();
    });

    connect(m_fmriprepProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                qDebug() << QStringLiteral("fmriprep 运行成功！");
            } else {
                QString errorOutput = QString::fromUtf8(m_fmriprepProcess->readAllStandardError());
                emit errorMsg(QStringLiteral("fmriprep 运行失败！\n错误代码: %1\n%2")
                    .arg(exitCode)
                    .arg(errorOutput));
            }
            stopFmriprepProcess();
            stopLogTimer();
            // 完成后尝试读取剩余日志一次
            startLogTimer(m_fmriprepLogFilePath);
            stopLogTimer();
        });
    // 合并输出，减少缓冲延迟；同时设置无缓冲环境变量（若可用）
    m_fmriprepProcess->setProcessChannelMode(QProcess::MergedChannels);
    // 尝试追加 PYTHONUNBUFFERED，但若 QProcessEnvironment 不可用（某些头被去除），则跳过

    startLogTimer(m_fmriprepLogFilePath);
    m_fmriprepProcess->start(exePath, arguments);
    m_fmriprepPid = m_fmriprepProcess->processId();
}

void MainViewController::stopFmriprepProcess()
{
    if (m_fmriprepProcess) {
        if (m_fmriprepProcess->state() != QProcess::NotRunning) {
            // 优先杀死进程树，避免子进程（如 docker）存活
#if defined(Q_OS_WIN)
            if (m_fmriprepPid > 0) {
                QProcess::execute("taskkill", {"/PID", QString::number(m_fmriprepPid), "/T", "/F"});
            }
#elif defined(Q_OS_UNIX)
            if (m_fmriprepPid > 0) {
                QProcess::execute("pkill", {"-P", QString::number(m_fmriprepPid)});
            }
#endif
            m_fmriprepProcess->kill();
            m_fmriprepProcess->waitForFinished(3000);
        }
        m_fmriprepProcess->deleteLater();
        m_fmriprepProcess = nullptr;
        m_fmriprepPid = -1;
    }
    stopLogTimer();
}

BrainRegionTableModel* MainViewController::getBrainRegionTableModel() const
{
    return m_brainRegionTableModel;
}

void MainViewController::appendFmriprepLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_fmriprepLog.append(text);
    emit fmriprepLogUpdated();
}

void MainViewController::clearFmriprepLog()
{
    m_fmriprepLog.clear();
    emit fmriprepLogUpdated();
}

void MainViewController::startLogTimer(const QString& logFilePath)
{
    m_fmriprepLogFilePath = logFilePath;
    if (!m_fmriprepLogTimer) {
        m_fmriprepLogTimer = new QTimer(this);
        m_fmriprepLogTimer->setInterval(1000);
        connect(m_fmriprepLogTimer, &QTimer::timeout, this, [this]() {
            if (m_fmriprepLogFilePath.isEmpty()) return;
            QFile f(m_fmriprepLogFilePath);
            if (!f.exists()) return;
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            if (m_fmriprepLogReadPos > f.size()) {
                m_fmriprepLogReadPos = 0;
            }
            if (!f.seek(m_fmriprepLogReadPos)) return;
            QByteArray data = f.readAll();
            m_fmriprepLogReadPos = f.pos();
            if (!data.isEmpty()) {
                appendFmriprepLog(QString::fromUtf8(data));
            }
        });
    }
    if (!m_fmriprepLogTimer->isActive()) {
        m_fmriprepLogTimer->start();
    }
}

void MainViewController::stopLogTimer()
{
    if (m_fmriprepLogTimer && m_fmriprepLogTimer->isActive()) {
        m_fmriprepLogTimer->stop();
    }
}

void MainViewController::processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir)
{
    // 构建 Python 脚本路径
    QString scriptPath = "Scripts/brain_network.exe";
    
    // 准备参数
    QStringList arguments;
    arguments << "--bold" << boldPath
              << "--confounds" << confoundsPath
              << "--tr" << "2.0"
              << "--output" << outputDir;
    
    // 创建进程
    QProcess* process = new QProcess(this);
    
    // 发出开始信号
    emit brainAnalysisStarted();
    
    // 连接完成信号
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                qDebug() << QStringLiteral("脑网络分析完成！");
                
                // 分析成功，加载结果
                if (loadOutputData(outputDir)) {
                    qDebug() << QStringLiteral("结果加载成功");
                    emit brainAnalysisFinished(true);
                } else {
                    qWarning() << QStringLiteral("结果加载失败");
                    emit errorMsg(QStringLiteral("脑网络分析完成，但加载结果失败"));
                    emit brainAnalysisFinished(false);
                }
            } else {
                QString errorOutput = QString::fromUtf8(process->readAllStandardError());
                qWarning() << QStringLiteral("脑网络分析失败！退出代码:") << exitCode;
                qWarning() << QStringLiteral("错误信息:") << errorOutput;
                
                emit errorMsg(QStringLiteral("脑网络分析失败！\n错误代码: %1\n\n%2")
                    .arg(exitCode)
                    .arg(errorOutput.isEmpty() ? QStringLiteral("未知错误") : errorOutput));
                emit brainAnalysisFinished(false);
            }
            process->deleteLater();
        });
    
    // 连接错误信号
    connect(process, &QProcess::errorOccurred, [=](QProcess::ProcessError error) {
        QString errorMsg;
        switch (error) {
            case QProcess::FailedToStart:
                errorMsg = QStringLiteral("脚本启动失败！请检查脚本路径: ") + scriptPath;
                break;
            case QProcess::Crashed:
                errorMsg = QStringLiteral("脚本运行时崩溃");
                break;
            case QProcess::Timedout:
                errorMsg = QStringLiteral("脚本运行超时");
                break;
            default:
                errorMsg = QStringLiteral("脚本运行错误: ") + process->errorString();
                break;
        }
        
        qWarning() << QStringLiteral("进程错误:") << errorMsg;
        emit this->errorMsg(errorMsg);
        emit brainAnalysisFinished(false);
        process->deleteLater();
    });
    
    // 启动进程
    qDebug() << QStringLiteral("启动脑网络分析程序:") << scriptPath;
    
    process->start(scriptPath, arguments);
}
