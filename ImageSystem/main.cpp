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
// 自定义交互样式 - 轴向视图（滚轮调整切片，左键调整窗宽窗位）
class AxialInteractorStyle : public vtkInteractorStyleImage
{
public:
    static AxialInteractorStyle* New();
    vtkTypeMacro(AxialInteractorStyle, vtkInteractorStyleImage);

    AxialInteractorStyle() : m_isDragging(false), m_lastX(0), m_lastY(0) {}

    void OnMouseWheelForward() override {
        int currentSlice = DicomDataManager::instance().axialSlice();
        int maxSlice = DicomDataManager::instance().maxAxialSlice();
        if (currentSlice < maxSlice) {
            DicomDataManager::instance().setAxialSlice(currentSlice + 1);
        }
    }

    void OnMouseWheelBackward() override {
        int currentSlice = DicomDataManager::instance().axialSlice();
        if (currentSlice > 0) {
            DicomDataManager::instance().setAxialSlice(currentSlice - 1);
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
            
            double newWidth = currentWidth + dx * 4.0;  // 调整灵敏度
            double newLevel = currentLevel + dy * 4.0;
            
            if (newWidth < 1) newWidth = 1;
            
            DicomDataManager::instance().setWindowWidth(newWidth);
            DicomDataManager::instance().setWindowLevel(newLevel);
            
            m_lastX = pos[0];
            m_lastY = pos[1];
        }
    }

private:
    bool m_isDragging;
    int m_lastX;
    int m_lastY;
};
vtkStandardNewMacro(AxialInteractorStyle);

// 自定义交互样式 - 矢状视图（滚轮调整切片，左键调整窗宽窗位）
class SagittalInteractorStyle : public vtkInteractorStyleImage
{
public:
    static SagittalInteractorStyle* New();
    vtkTypeMacro(SagittalInteractorStyle, vtkInteractorStyleImage);

    SagittalInteractorStyle() : m_isDragging(false), m_lastX(0), m_lastY(0) {}

    void OnMouseWheelForward() override {
        int currentSlice = DicomDataManager::instance().sagittalSlice();
        int maxSlice = DicomDataManager::instance().maxSagittalSlice();
        if (currentSlice < maxSlice) {
            DicomDataManager::instance().setSagittalSlice(currentSlice + 1);
        }
    }

    void OnMouseWheelBackward() override {
        int currentSlice = DicomDataManager::instance().sagittalSlice();
        if (currentSlice > 0) {
            DicomDataManager::instance().setSagittalSlice(currentSlice - 1);
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

private:
    bool m_isDragging;
    int m_lastX;
    int m_lastY;
};
vtkStandardNewMacro(SagittalInteractorStyle);

// 自定义交互样式 - 冠状视图（滚轮调整切片，左键调整窗宽窗位）
class CoronalInteractorStyle : public vtkInteractorStyleImage
{
public:
    static CoronalInteractorStyle* New();
    vtkTypeMacro(CoronalInteractorStyle, vtkInteractorStyleImage);

    CoronalInteractorStyle() : m_isDragging(false), m_lastX(0), m_lastY(0) {}

    void OnMouseWheelForward() override {
        int currentSlice = DicomDataManager::instance().coronalSlice();
        int maxSlice = DicomDataManager::instance().maxCoronalSlice();
        if (currentSlice < maxSlice) {
            DicomDataManager::instance().setCoronalSlice(currentSlice + 1);
        }
    }

    void OnMouseWheelBackward() override {
        int currentSlice = DicomDataManager::instance().coronalSlice();
        if (currentSlice > 0) {
            DicomDataManager::instance().setCoronalSlice(currentSlice - 1);
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

private:
    bool m_isDragging;
    int m_lastX;
    int m_lastY;
};
vtkStandardNewMacro(CoronalInteractorStyle);


// 轴向视图（Axial - XY平面）
class AxialVtkItem : public QQuickVTKItem
{
    Q_OBJECT
public:
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        m_renderWindow = renderWindow;
        updateView();
        // 连接数据管理器信号
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &AxialVtkItem::updateView);
        connect(&DicomDataManager::instance(), &DicomDataManager::axialSliceChanged,
                this, &AxialVtkItem::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowWidthChanged,
                this, &AxialVtkItem::onWindowChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowLevelChanged,
                this, &AxialVtkItem::onWindowChanged);
        
        return nullptr;
    }

private slots:
    void updateView() {
        if (!m_renderWindow) return;
        
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (!imageData) return;

        // 清除旧的渲染器
        m_renderWindow->GetRenderers()->RemoveAllItems();
        
        // 创建图像切片
        m_imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        m_imageMapper->SetInputData(imageData);
        m_imageMapper->SetOrientationToZ(); // 轴向视图
        m_imageMapper->SetSliceNumber(DicomDataManager::instance().axialSlice());

        m_imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        m_imageSlice->SetMapper(m_imageMapper);
        m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
        m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());

        // 创建渲染器
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderer->AddViewProp(m_imageSlice);
        m_renderer->SetBackground(0, 0, 0);

        // 添加标题文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Axial View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        m_renderer->AddActor2D(textActor);

        // 添加窗宽窗位显示文本（右下角）
        m_wwwlTextMapper = vtkSmartPointer<vtkTextMapper>::New();
        updateWWWLText();
        m_wwwlTextMapper->GetTextProperty()->SetFontSize(16);
        m_wwwlTextMapper->GetTextProperty()->SetColor(0.0, 1.0, 0.0);
        m_wwwlTextMapper->GetTextProperty()->SetJustificationToRight();
        
        m_wwwlTextActor = vtkSmartPointer<vtkActor2D>::New();
        m_wwwlTextActor->SetMapper(m_wwwlTextMapper);
        m_wwwlTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        m_wwwlTextActor->SetPosition(0.98, 0.02);  // 右下角
        m_renderer->AddActor2D(m_wwwlTextActor);

        m_renderWindow->AddRenderer(m_renderer);
        
        // 设置自定义交互样式（滚轮调整切片）
        vtkSmartPointer<AxialInteractorStyle> style = vtkSmartPointer<AxialInteractorStyle>::New();
        m_renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        // 设置相机方向
        m_renderer->ResetCamera();
        vtkCamera* camera = m_renderer->GetActiveCamera();
        camera->SetViewUp(0, -1, 0);
        camera->SetPosition(0, 0, 1);
        camera->SetFocalPoint(0, 0, 0);
        m_renderer->ResetCamera();
        
        scheduleRender();
    }

    void onSliceChanged(int slice) {
        if (m_imageMapper) {
            m_imageMapper->SetSliceNumber(slice);
            m_imageMapper->Modified();
            scheduleRender();
        }
    }

    void onWindowChanged() {
        if (m_imageSlice) {
            m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
            m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());
            m_imageSlice->GetProperty()->Modified();
            updateWWWLText();
            scheduleRender();
        }
    }

    void updateWWWLText() {
        if (m_wwwlTextMapper) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f", 
                     DicomDataManager::instance().windowWidth(),
                     DicomDataManager::instance().windowLevel());
            m_wwwlTextMapper->SetInput(buffer);
        }
    }

private:
    vtkRenderWindow* m_renderWindow = nullptr;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkImageSliceMapper> m_imageMapper;
    vtkSmartPointer<vtkImageSlice> m_imageSlice;
    vtkSmartPointer<vtkTextMapper> m_wwwlTextMapper;
    vtkSmartPointer<vtkActor2D> m_wwwlTextActor;
};

// 矢状视图（Sagittal - YZ平面）
class SagittalVtkItem : public QQuickVTKItem
{
    Q_OBJECT
public:
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        m_renderWindow = renderWindow;
        updateView();
        
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &SagittalVtkItem::updateView);
        connect(&DicomDataManager::instance(), &DicomDataManager::sagittalSliceChanged,
                this, &SagittalVtkItem::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowWidthChanged,
                this, &SagittalVtkItem::onWindowChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowLevelChanged,
                this, &SagittalVtkItem::onWindowChanged);
        
        return nullptr;
    }

private slots:
    void updateView() {
        if (!m_renderWindow) return;
        
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (!imageData) return;
        
        m_renderWindow->GetRenderers()->RemoveAllItems();
        m_imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        m_imageMapper->SetInputData(imageData);
        m_imageMapper->SetOrientationToX(); // 矢状视图
        m_imageMapper->SetSliceNumber(DicomDataManager::instance().sagittalSlice());

        m_imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        m_imageSlice->SetMapper(m_imageMapper);
        m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
        m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddViewProp(m_imageSlice);
        renderer->SetBackground(0, 0, 0);

        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Sagittal View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        // 添加窗宽窗位显示文本（右下角）
        m_wwwlTextMapper = vtkSmartPointer<vtkTextMapper>::New();
        updateWWWLText();
        m_wwwlTextMapper->GetTextProperty()->SetFontSize(16);
        m_wwwlTextMapper->GetTextProperty()->SetColor(0.0, 1.0, 0.0);
        m_wwwlTextMapper->GetTextProperty()->SetJustificationToRight();
        
        m_wwwlTextActor = vtkSmartPointer<vtkActor2D>::New();
        m_wwwlTextActor->SetMapper(m_wwwlTextMapper);
        m_wwwlTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        m_wwwlTextActor->SetPosition(0.98, 0.02);  // 右下角
        renderer->AddActor2D(m_wwwlTextActor);

        m_renderWindow->AddRenderer(renderer);
        
        // 设置自定义交互样式（滚轮调整切片）
        vtkSmartPointer<SagittalInteractorStyle> style = vtkSmartPointer<SagittalInteractorStyle>::New();
        m_renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->SetViewUp(0, 0, 1);
        camera->SetPosition(1, 0, 0);
        camera->SetFocalPoint(0, 0, 0);
        renderer->ResetCamera();
       
        scheduleRender();
    }

    void onSliceChanged(int slice) {
        if (m_imageMapper) {
            m_imageMapper->SetSliceNumber(slice);
            m_imageMapper->Modified();
            scheduleRender();
        }
    }

    void onWindowChanged() {
        if (m_imageSlice) {
            m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
            m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());
            m_imageSlice->GetProperty()->Modified();
            updateWWWLText();
            scheduleRender();
        }
    }

    void updateWWWLText() {
        if (m_wwwlTextMapper) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f", 
                     DicomDataManager::instance().windowWidth(),
                     DicomDataManager::instance().windowLevel());
            m_wwwlTextMapper->SetInput(buffer);
        }
    }

private:
    vtkRenderWindow* m_renderWindow = nullptr;
    vtkSmartPointer<vtkImageSliceMapper> m_imageMapper;
    vtkSmartPointer<vtkImageSlice> m_imageSlice;
    vtkSmartPointer<vtkTextMapper> m_wwwlTextMapper;
    vtkSmartPointer<vtkActor2D> m_wwwlTextActor;
};

// 冠状视图（Coronal - XZ平面）
class CoronalVtkItem : public QQuickVTKItem
{
    Q_OBJECT
public:
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        m_renderWindow = renderWindow;
        updateView();
        
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &CoronalVtkItem::updateView);
        connect(&DicomDataManager::instance(), &DicomDataManager::coronalSliceChanged,
                this, &CoronalVtkItem::onSliceChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowWidthChanged,
                this, &CoronalVtkItem::onWindowChanged);
        connect(&DicomDataManager::instance(), &DicomDataManager::windowLevelChanged,
                this, &CoronalVtkItem::onWindowChanged);
        
        return nullptr;
    }

private slots:
    void updateView() {
        if (!m_renderWindow) return;
        
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (!imageData) return;

        m_renderWindow->GetRenderers()->RemoveAllItems();
        
        m_imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        m_imageMapper->SetInputData(imageData);
        m_imageMapper->SetOrientationToY(); // 冠状视图
        m_imageMapper->SetSliceNumber(DicomDataManager::instance().coronalSlice());

        m_imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        m_imageSlice->SetMapper(m_imageMapper);
        m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
        m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddViewProp(m_imageSlice);
        renderer->SetBackground(0, 0, 0);
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Coronal View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        // 添加窗宽窗位显示文本（右下角）
        m_wwwlTextMapper = vtkSmartPointer<vtkTextMapper>::New();
        updateWWWLText();
        m_wwwlTextMapper->GetTextProperty()->SetFontSize(16);
        m_wwwlTextMapper->GetTextProperty()->SetColor(0.0, 1.0, 0.0);
        m_wwwlTextMapper->GetTextProperty()->SetJustificationToRight();
        
        m_wwwlTextActor = vtkSmartPointer<vtkActor2D>::New();
        m_wwwlTextActor->SetMapper(m_wwwlTextMapper);
        m_wwwlTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        m_wwwlTextActor->SetPosition(0.98, 0.02);  // 右下角
        renderer->AddActor2D(m_wwwlTextActor);

        m_renderWindow->AddRenderer(renderer);
        
        // 设置自定义交互样式（滚轮调整切片）
        vtkSmartPointer<CoronalInteractorStyle> style = vtkSmartPointer<CoronalInteractorStyle>::New();
        m_renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->SetViewUp(0, 0, 1);
        camera->SetPosition(0, -1, 0);
        camera->SetFocalPoint(0, 0, 0);
        renderer->ResetCamera();
        scheduleRender();
    }

    void onSliceChanged(int slice) {
        if (m_imageMapper) {
            m_imageMapper->SetSliceNumber(slice);
            m_imageMapper->Modified();
            scheduleRender();
        }
    }

    void onWindowChanged() {
        if (m_imageSlice) {
            m_imageSlice->GetProperty()->SetColorWindow(DicomDataManager::instance().windowWidth());
            m_imageSlice->GetProperty()->SetColorLevel(DicomDataManager::instance().windowLevel());
            m_imageSlice->GetProperty()->Modified();
            updateWWWLText();
            scheduleRender();
        }
    }

    void updateWWWLText() {
        if (m_wwwlTextMapper) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f", 
                     DicomDataManager::instance().windowWidth(),
                     DicomDataManager::instance().windowLevel());
            m_wwwlTextMapper->SetInput(buffer);
        }
    }

private:
    vtkRenderWindow* m_renderWindow = nullptr;
    vtkSmartPointer<vtkImageSliceMapper> m_imageMapper;
    vtkSmartPointer<vtkImageSlice> m_imageSlice;
    vtkSmartPointer<vtkTextMapper> m_wwwlTextMapper;
    vtkSmartPointer<vtkActor2D> m_wwwlTextActor;
};

// 3D体渲染视图
class VolumeVtkItem : public QQuickVTKItem
{
    Q_OBJECT
public:
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        m_renderWindow = renderWindow;
        updateView();
        
        connect(&DicomDataManager::instance(), &DicomDataManager::dataLoaded,
                this, &VolumeVtkItem::updateView);
        
        return nullptr;
    }

private slots:
    void updateView() {
        if (!m_renderWindow) return;
        
        vtkSmartPointer<vtkImageData> imageData = DicomDataManager::instance().getImageData();
        if (!imageData) return;

        m_renderWindow->GetRenderers()->RemoveAllItems();

        // 创建体渲染映射器
        vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        volumeMapper->SetInputData(imageData);

        // 颜色传输函数
        vtkSmartPointer<vtkColorTransferFunction> colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
        colorFunc->AddRGBPoint(-3024, 0.0, 0.0, 0.0);
        colorFunc->AddRGBPoint(-77, 0.54902, 0.25098, 0.14902);
        colorFunc->AddRGBPoint(94, 0.882353, 0.603922, 0.290196);
        colorFunc->AddRGBPoint(179, 1.0, 0.937033, 0.954531);
        colorFunc->AddRGBPoint(260, 0.615686, 0.0, 0.0);
        colorFunc->AddRGBPoint(3071, 0.827451, 0.658824, 1.0);

        // 不透明度传输函数
        vtkSmartPointer<vtkPiecewiseFunction> opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
        opacityFunc->AddPoint(-3024, 0.0);
        opacityFunc->AddPoint(-77, 0.0);
        opacityFunc->AddPoint(94, 0.29);
        opacityFunc->AddPoint(179, 0.55);
        opacityFunc->AddPoint(260, 0.84);
        opacityFunc->AddPoint(3071, 0.875);

        // 梯度不透明度函数
        vtkSmartPointer<vtkPiecewiseFunction> gradientFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
        gradientFunc->AddPoint(0, 0.0);
        gradientFunc->AddPoint(90, 0.5);
        gradientFunc->AddPoint(100, 1.0);

        // 体属性
        vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
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

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddVolume(volume);
        renderer->SetBackground(0, 0, 0);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("3D Volume Rendering");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        m_renderWindow->AddRenderer(renderer);
        
        // 设置3D交互样式
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        m_renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        renderer->ResetCamera();
        
        scheduleRender();;
    }

private:
    vtkRenderWindow* m_renderWindow = nullptr;
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

