#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>
#include <QQmlContext>
#include <vtkRendererCollection.h>
#include <QStandardPaths>

#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>
#include <vtkResliceImageViewer.h>
// 3D Volume Rendering
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkInteractorStyleTrackballCamera.h>
// Slice Viewing
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageProperty.h>
#include <vtkInteractorStyleImage.h>
#include <vtkTextProperty.h>
#include <vtkTextMapper.h>
#include <vtkActor2D.h>
#include <vtkResliceCursor.h>
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

// DICOM数据管理器
class DicomDataManager : public QObject
{
    Q_OBJECT
        Q_PROPERTY(int axialSlice READ axialSlice WRITE setAxialSlice NOTIFY axialSliceChanged)
        Q_PROPERTY(int sagittalSlice READ sagittalSlice WRITE setSagittalSlice NOTIFY sagittalSliceChanged)
        Q_PROPERTY(int coronalSlice READ coronalSlice WRITE setCoronalSlice NOTIFY coronalSliceChanged)
        Q_PROPERTY(int maxAxialSlice READ maxAxialSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxSagittalSlice READ maxSagittalSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxCoronalSlice READ maxCoronalSlice NOTIFY dataLoaded)
        Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
        Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowLevelChanged)
        Q_PROPERTY(QString dicomInfo READ dicomInfo NOTIFY dataLoaded)
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataLoaded)

public:
    static DicomDataManager& instance() {
        static DicomDataManager instance;
        return instance;
    }

    vtkSmartPointer<vtkImageData> getImageData() { return m_imageData; }

    int axialSlice() const { return m_axialSlice; }
    int sagittalSlice() const { return m_sagittalSlice; }
    int coronalSlice() const { return m_coronalSlice; }
    int maxAxialSlice() const { return m_dims[2] - 1; }
    int maxSagittalSlice() const { return m_dims[0] - 1; }
    int maxCoronalSlice() const { return m_dims[1] - 1; }
    double windowWidth() const { return m_windowWidth; }
    double windowLevel() const { return m_windowLevel; }
    QString dicomInfo() const { return m_dicomInfo; }
    bool hasData() const { return m_imageData != nullptr; }

    void setAxialSlice(int slice) {
        if (slice != m_axialSlice && slice >= 0 && slice < m_dims[2]) {
            m_axialSlice = slice;
            emit axialSliceChanged(slice);
        }
    }

    void setSagittalSlice(int slice) {
        if (slice != m_sagittalSlice && slice >= 0 && slice < m_dims[0]) {
            m_sagittalSlice = slice;
            emit sagittalSliceChanged(slice);
        }
    }

    void setCoronalSlice(int slice) {
        if (slice != m_coronalSlice && slice >= 0 && slice < m_dims[1]) {
            m_coronalSlice = slice;
            emit coronalSliceChanged(slice);
        }
    }

    void setWindowWidth(double width) {
        if (width != m_windowWidth && width > 0) {
            m_windowWidth = width;
            emit windowWidthChanged(width);
        }
    }

    void setWindowLevel(double level) {
        if (level != m_windowLevel) {
            m_windowLevel = level;
            emit windowLevelChanged(level);
        }
    }

    Q_INVOKABLE bool loadDicomDirectory(const QString& path) {
        if (path.isEmpty()) return false;

        QString dirPath = path;
        if (dirPath.startsWith("file:///")) {
            dirPath = dirPath.mid(8);
        }

        vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
        reader->SetDirectoryName(dirPath.toStdString().c_str());
        reader->Update();

        if (reader->GetOutput()->GetNumberOfPoints() == 0) {
            qDebug() << "Failed to read DICOM data or directory is empty!";
            return false;
        }

        m_imageData = reader->GetOutput();
        m_imageData->GetDimensions(m_dims);

        // 设置默认窗宽窗位
        m_windowWidth = 2000;
        m_windowLevel = 0;

        // 获取DICOM信息
        double* spacing = m_imageData->GetSpacing();
        double* origin = m_imageData->GetOrigin();

        m_dicomInfo = QString("Dimensions: %1 x %2 x %3\n"
            "Spacing: %4 x %5 x %6 mm\n"
            "Origin: (%7, %8, %9)")
            .arg(m_dims[0]).arg(m_dims[1]).arg(m_dims[2])
            .arg(spacing[0]).arg(spacing[1]).arg(spacing[2])
            .arg(origin[0]).arg(origin[1]).arg(origin[2]);

        emit dataLoaded();
        // 设置默认切片为中间位置
        setAxialSlice(m_dims[2] / 2);
        setSagittalSlice(m_dims[0] / 2);
        setCoronalSlice(m_dims[1] / 2);
        return true;
    }

signals:
    void axialSliceChanged(int slice);
    void sagittalSliceChanged(int slice);
    void coronalSliceChanged(int slice);
    void windowWidthChanged(double width);
    void windowLevelChanged(double level);
    void dataLoaded();

private:
    DicomDataManager() : m_axialSlice(0), m_sagittalSlice(0), m_coronalSlice(0),
        m_windowWidth(2000), m_windowLevel(0) {
        m_dims[0] = m_dims[1] = m_dims[2] = 1;
    }

    vtkSmartPointer<vtkImageData> m_imageData;
    int m_dims[3];
    int m_axialSlice;
    int m_sagittalSlice;
    int m_coronalSlice;
    double m_windowWidth;
    double m_windowLevel;
    QString m_dicomInfo;
};
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
            double currentWidth = DicomDataManager::instance().windowWidth();
            double currentLevel = DicomDataManager::instance().windowLevel();
            
            double newWidth = currentWidth + dx * 4.0;
            double newLevel = currentLevel + dy * 4.0;
            
            if (newWidth < 1) newWidth = 1;
            
            DicomDataManager::instance().setWindowWidth(newWidth);
            DicomDataManager::instance().setWindowLevel(newLevel);
            
            m_lastX = pos[0];
            m_lastY = pos[1];
        }
    }

protected:
    SliceInteractorStyle() : m_orientation(SliceOrientation::Axial), 
                                m_isDragging(false), m_lastX(0), m_lastY(0) {}

private:
    int getCurrentSlice() const {
        switch (m_orientation) {
            case SliceOrientation::Axial: return DicomDataManager::instance().axialSlice();
            case SliceOrientation::Sagittal: return DicomDataManager::instance().sagittalSlice();
            case SliceOrientation::Coronal: return DicomDataManager::instance().coronalSlice();
        }
        return 0;
    }

    int getMaxSlice() const {
        switch (m_orientation) {
            case SliceOrientation::Axial: return DicomDataManager::instance().maxAxialSlice();
            case SliceOrientation::Sagittal: return DicomDataManager::instance().maxSagittalSlice();
            case SliceOrientation::Coronal: return DicomDataManager::instance().maxCoronalSlice();
        }
        return 0;
    }

    void setSlice(int slice) {
        switch (m_orientation) {
            case SliceOrientation::Axial: 
                DicomDataManager::instance().setAxialSlice(slice); 
                break;
            case SliceOrientation::Sagittal: 
                DicomDataManager::instance().setSagittalSlice(slice); 
                break;
            case SliceOrientation::Coronal: 
                DicomDataManager::instance().setCoronalSlice(slice); 
                break;
        }
    }

    SliceOrientation m_orientation;
    bool m_isDragging;
    int m_lastX;
    int m_lastY;
};
vtkStandardNewMacro(SliceInteractorStyle);


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
vtkStandardNewMacro(SliceViewData);

// 切片视图基类 - 减少重复代码
class SliceVtkItemBase : public QQuickVTKItem
{
    Q_OBJECT
public:
    SliceVtkItemBase(SliceOrientation orientation, const char* viewName)
        : m_orientation(orientation), m_viewName(viewName) {}

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        // 创建用户数据对象
        vtkSmartPointer<SliceViewData> data = vtkSmartPointer<SliceViewData>::New();
        
        // 连接信号（在GUI线程）
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &SliceVtkItemBase::onDataLoaded);
        connect(&DicomDataManager::instance(), &DicomDataManager::axialSliceChanged,
                this, &SliceVtkItemBase::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::sagittalSliceChanged,
                this, &SliceVtkItemBase::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::coronalSliceChanged,
                this, &SliceVtkItemBase::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowWidthChanged,
                this, &SliceVtkItemBase::onWindowChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowLevelChanged,
                this, &SliceVtkItemBase::onWindowChanged);
        
        // 在渲染线程初始化VTK对象
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (imageData) {
            setupView(renderWindow, data, imageData);
        }
        
        return data;
    }

private slots:
    void onDataLoaded() {
        // 使用 dispatch_async 在渲染线程更新VTK对象
        dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
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
                        DicomDataManager::instance().windowWidth());
                    data->imageSlice->GetProperty()->SetColorLevel(
                        DicomDataManager::instance().windowLevel());
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
        data->imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
        data->imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());

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
                camera->SetViewUp(0, -1, 0);
                camera->SetPosition(0, 0, 1);
                camera->SetFocalPoint(0, 0, 0);
                break;
            case SliceOrientation::Sagittal:
                camera->SetViewUp(0, 0, 1);
                camera->SetPosition(1, 0, 0);
                camera->SetFocalPoint(0, 0, 0);
                break;
            case SliceOrientation::Coronal:
                camera->SetViewUp(0, 0, 1);
                camera->SetPosition(0, -1, 0);
                camera->SetFocalPoint(0, 0, 0);
                break;
        }
        
        renderer->ResetCamera();
    }

    int getCurrentSlice() const {
        switch (m_orientation) {
            case SliceOrientation::Axial:
                return DicomDataManager::instance().axialSlice();
            case SliceOrientation::Sagittal:
                return DicomDataManager::instance().sagittalSlice();
            case SliceOrientation::Coronal:
                return DicomDataManager::instance().coronalSlice();
        }
        return 0;
    }

    void updateWWWLText(SliceViewData* data) {
        if (data->wwwlTextMapper) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f", 
                        DicomDataManager::instance().windowWidth(),
                        DicomDataManager::instance().windowLevel());
            data->wwwlTextMapper->SetInput(buffer);
        }
    }

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
vtkStandardNewMacro(VolumeViewData);

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
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &VolumeVtkItem::onDataLoaded);
        
        // 在渲染线程初始化VTK对象
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (imageData) {
            setupView(renderWindow, data, imageData);
        }
        
        return data;
    }

private slots:
    void onDataLoaded() {
        // 使用 dispatch_async 在渲染线程更新VTK对象
        dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
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
        
        data->renderer->ResetCamera();
    }
};

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);

    // 注册四个视图类型
    qmlRegisterType<AxialVtkItem>("com.vtk.dicom", 1, 0, "AxialView");
    qmlRegisterType<SagittalVtkItem>("com.vtk.dicom", 1, 0, "SagittalView");
    qmlRegisterType<CoronalVtkItem>("com.vtk.dicom", 1, 0, "CoronalView");
    qmlRegisterType<VolumeVtkItem>("com.vtk.dicom", 1, 0, "VolumeView");

    QQmlApplicationEngine engine;
    
    // 将DicomDataManager暴露给QML
    engine.rootContext()->setContextProperty("dicomManager", &DicomDataManager::instance());
    
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

// 包含moc文件以支持Q_OBJECT宏
#include "main.moc"