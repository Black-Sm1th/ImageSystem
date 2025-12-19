#include <QtQml/QQmlApplicationEngine>
#include <QtGui/QGuiApplication>
#include <QtWebEngine/QtWebEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QQuickVTKItem.h>
#include <vtkAutoInit.h>
#include "Modules/CommonFunc.h"
#include "ViewController/MainViewController.h"
#include "Model/DicomDataModel.h"
#include "Modules/Version.h"
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    //// 强制使用 OpenGL 3.3 兼容模式
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QtWebEngine::initialize();
    QGuiApplication app(argc, argv);

    // 注册四个视图类型
    qmlRegisterType<AxialVtkItem>("com.vtk.dicom", 1, 0, "AxialView");
    qmlRegisterType<SagittalVtkItem>("com.vtk.dicom", 1, 0, "SagittalView");
    qmlRegisterType<CoronalVtkItem>("com.vtk.dicom", 1, 0, "CoronalView");
    qmlRegisterType<VolumeVtkItem>("com.vtk.dicom", 1, 0, "VolumeView");

    QCoreApplication::setOrganizationName("AETHERMIND");
    QCoreApplication::setOrganizationDomain("aethermind.com");
    QCoreApplication::setApplicationName("ImageSystem");
    QCoreApplication::setApplicationVersion(VER_VERSION_STR);

    QQmlApplicationEngine engine;

    // 将DicomDataManager暴露给QML
    engine.rootContext()->setContextProperty("$DicomDataModel", GET_SINGLETON(DicomDataModel));
    // 将MainViewController暴露给QML
    engine.rootContext()->setContextProperty("$MainViewController", GET_SINGLETON(MainViewController));
    // 将BrainRegionTableModel暴露给QML
    engine.rootContext()->setContextProperty("$BrainRegionTableModel", GET_SINGLETON(MainViewController)->getBrainRegionTableModel());
    // 将BrainSegmentationTableModel暴露给QML
    engine.rootContext()->setContextProperty("$BrainSegmentationTableModel", GET_SINGLETON(DicomDataModel)->getSegmentationTableModel());
    int fontId1 = QFontDatabase::addApplicationFont(":/fonts/AlibabaPuHuiTi-3-55-Regular.ttf");
    int fontId2 = QFontDatabase::addApplicationFont(":/fonts/AlibabaPuHuiTi-3-65-Medium.ttf");
    int fontId3 = QFontDatabase::addApplicationFont(":/fonts/AlibabaPuHuiTi-3-85-Bold.ttf");
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}