#include "QQuickVTKRenderItem.h"
#include "QQuickVTKRenderWindow.h"
#include "CustomVTKItem.h"

#include <QGuiApplication>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
int main(int argc, char* argv[])
{
    cout << "CTEST_FULL_OUTPUT (Avoid ctest truncation of output)" << endl;

    QQuickVTKRenderWindow::setupGraphicsBackend();
    QGuiApplication app(argc, argv);

    qmlRegisterType<QQuickVTKRenderWindow>("VTK", 9, 2, "VTKRenderWindow");
    qmlRegisterType<CustomVTKItem>("VTK", 9, 2, "VTKRenderItem");

    QQmlApplicationEngine engine;
    qDebug() << "QML2_IMPORT_PATH:" << engine.importPathList();
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    auto rootObjects = engine.rootObjects();
    if (!rootObjects.isEmpty() && rootObjects.first()) {
        qDebug() << "First root object is not null";
    }
    else {
        qDebug() << "First root object is null or list is empty";
    }
    QObject* topLevel = engine.rootObjects().value(0);
    QQuickWindow* window = qobject_cast<QQuickWindow*>(topLevel);

    window->show();

    // Fetch the QQuick window using the standard object name set up in the constructor
    CustomVTKItem* qquickvtkItem = static_cast<CustomVTKItem*>(topLevel->findChild<QQuickVTKRenderItem*>("ConeView"));

    // 设置DICOM文件目录 - 请修改为你的DICOM文件夹路径
    // 示例: qquickvtkItem->setDicomDirectory("C:/path/to/your/dicom/folder");
    qquickvtkItem->setDicomDirectory("C:\\Users\\71455\\Desktop\\Dicom\\SLC");  // 请修改此路径为实际的DICOM文件夹

    qquickvtkItem->InitInteractorStyle();
    qquickvtkItem->InitData();
    qquickvtkItem->update();

    return app.exec();
}