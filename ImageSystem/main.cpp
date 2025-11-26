#include <QtQml/QQmlApplicationEngine>
#include <QtGui/QGuiApplication>
#include <QQmlContext>
#include <QQuickVTKItem.h>
#include <vtkAutoInit.h>
#include "CommonFunc.h"
#include "MainViewController.h"
#include "DicomDataModel.h"


VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

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
    engine.rootContext()->setContextProperty("$DicomDataModel", GET_SINGLETON(DicomDataModel));

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}