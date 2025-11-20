#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>

#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

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

#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
// 全局共享的DICOM数据
static vtkSmartPointer<vtkImageData> g_dicomImageData = nullptr;

// 加载DICOM数据的辅助函数
vtkSmartPointer<vtkImageData> LoadDICOMData()
{
    if (g_dicomImageData != nullptr) {
        return g_dicomImageData;
    }

    std::string inputFilename = "C:\\Users\\71455\\Desktop\\Dicom\\SLC";
    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetDirectoryName(inputFilename.c_str());
    reader->Update();
    
    if (reader->GetOutput()->GetNumberOfPoints() == 0) {
        qDebug() << "Failed to read DICOM data or directory is empty!";
        return nullptr;
    }
    
    g_dicomImageData = reader->GetOutput();
    return g_dicomImageData;
}

// 轴向视图（Axial - XY平面）
struct AxialVtkItem : QQuickVTKItem
{
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        vtkSmartPointer<vtkImageData> imageData = LoadDICOMData();
        if (!imageData) return nullptr;

        // 获取图像维度
        int* dims = imageData->GetDimensions();
        int sliceNumber = dims[2] / 2; // 中间切片

        // 创建图像切片
        vtkSmartPointer<vtkImageSliceMapper> imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        imageMapper->SetInputData(imageData);
        imageMapper->SetOrientationToZ(); // 轴向视图
        imageMapper->SetSliceNumber(sliceNumber);

        vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        imageSlice->SetMapper(imageMapper);
        imageSlice->GetProperty()->SetColorWindow(2000);
        imageSlice->GetProperty()->SetColorLevel(0);

        // 创建渲染器
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddViewProp(imageSlice);
        renderer->SetBackground(0.1, 0.1, 0.1);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Axial View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        renderWindow->AddRenderer(renderer);
        
        // 设置交互样式
        vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
        renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        // 设置相机方向（轴向视图从上往下看）
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->SetViewUp(0, -1, 0);  // Y轴向上（医学影像标准）
        camera->SetPosition(0, 0, 1);  // 从Z轴正方向看
        camera->SetFocalPoint(0, 0, 0);  // 看向原点
        renderer->ResetCamera();
        
        return nullptr;
    }
};

// 矢状视图（Sagittal - YZ平面）
struct SagittalVtkItem : QQuickVTKItem
{
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        vtkSmartPointer<vtkImageData> imageData = LoadDICOMData();
        if (!imageData) return nullptr;

        int* dims = imageData->GetDimensions();
        int sliceNumber = dims[0] / 2; // 中间切片

        vtkSmartPointer<vtkImageSliceMapper> imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        imageMapper->SetInputData(imageData);
        imageMapper->SetOrientationToX(); // 矢状视图
        imageMapper->SetSliceNumber(sliceNumber);

        vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        imageSlice->SetMapper(imageMapper);
        imageSlice->GetProperty()->SetColorWindow(2000);
        imageSlice->GetProperty()->SetColorLevel(0);

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddViewProp(imageSlice);
        renderer->SetBackground(0.1, 0.1, 0.1);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Sagittal View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        renderWindow->AddRenderer(renderer);
        
        vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
        renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        // 设置相机方向（矢状视图从侧面看）
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->SetViewUp(0, 0, 1);  // Z轴向上
        camera->SetPosition(1, 0, 0);  // 从X轴正方向看
        camera->SetFocalPoint(0, 0, 0);  // 看向原点
        renderer->ResetCamera();
        
        return nullptr;
    }
};

// 冠状视图（Coronal - XZ平面）
struct CoronalVtkItem : QQuickVTKItem
{
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        vtkSmartPointer<vtkImageData> imageData = LoadDICOMData();
        if (!imageData) return nullptr;

        int* dims = imageData->GetDimensions();
        int sliceNumber = dims[1] / 2; // 中间切片
        
        vtkSmartPointer<vtkImageSliceMapper> imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        imageMapper->SetInputData(imageData);
        imageMapper->SetOrientationToY(); // 冠状视图
        imageMapper->SetSliceNumber(sliceNumber);

        vtkSmartPointer<vtkImageSlice> imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        imageSlice->SetMapper(imageMapper);
        imageSlice->GetProperty()->SetColorWindow(2000);
        imageSlice->GetProperty()->SetColorLevel(0);

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->AddViewProp(imageSlice);
        renderer->SetBackground(0.1, 0.1, 0.1);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("Coronal View");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        renderWindow->AddRenderer(renderer);
        
        vtkSmartPointer<vtkInteractorStyleImage> style = vtkSmartPointer<vtkInteractorStyleImage>::New();
        renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        // 设置相机方向（冠状视图从前面看）
        renderer->ResetCamera();
        vtkCamera* camera = renderer->GetActiveCamera();
        camera->SetViewUp(0, 0, 1);  // Z轴向上
        camera->SetPosition(0, -1, 0);  // 从Y轴负方向看
        camera->SetFocalPoint(0, 0, 0);  // 看向原点
        renderer->ResetCamera();
        
        return nullptr;
    }
};

// 3D体渲染视图
struct VolumeVtkItem : QQuickVTKItem
{
    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override
    {
        vtkSmartPointer<vtkImageData> imageData = LoadDICOMData();
        if (!imageData) return nullptr;

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
        renderer->SetBackground(0.1, 0.1, 0.1);

        // 添加文本标签
        vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
        textMapper->SetInput("3D Volume Rendering");
        textMapper->GetTextProperty()->SetFontSize(20);
        textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);
        
        vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
        textActor->SetMapper(textMapper);
        textActor->SetPosition(10, 10);
        renderer->AddActor2D(textActor);

        renderWindow->AddRenderer(renderer);
        
        // 设置3D交互样式
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        renderWindow->GetInteractor()->SetInteractorStyle(style);
        
        renderer->ResetCamera();
        
        return nullptr;
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
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

