#include <QtQml/QQmlApplicationEngine>
#include <QtGui/QGuiApplication>
#include <QtWebEngine/QtWebEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QQuickVTKItem.h>
#include <QEvent>
#include <QKeyEvent>
#include <vtkAutoInit.h>
#include "Modules/CommonFunc.h"
#include "ViewController/MainViewController.h"
#include "Model/DicomDataModel.h"
#include "Modules/Version.h"
#include "Modules/DicomNetwork.h"

// 全局键盘事件过滤：不依赖 QML focus，把 1-6 写入 DicomDataModel.toolMode
class GlobalKeyFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        Q_UNUSED(obj);
        if (event->type() != QEvent::KeyPress) {
            return false;
        }
        auto* ke = static_cast<QKeyEvent*>(event);
        const int key = ke->key();
        int mode = -1;
        // 与 ViewController/InteractionState.h 的 ToolMode 数值保持一致：
        // None=0, WindowLevel=1, Contrast=2, Pan=3, Zoom=4, MeasureDistance=5, MeasureAngle=6
        switch (key) {
        case Qt::Key_0: mode = 0; break;
        case Qt::Key_1: mode = 1; break;
        case Qt::Key_2: mode = 2; break;
        case Qt::Key_3: mode = 4; break; // 3 -> Zoom
        case Qt::Key_4: mode = 3; break; // 4 -> Pan
        case Qt::Key_5: mode = 5; break;
        case Qt::Key_6: mode = 6; break;
        case Qt::Key_7:
            GET_SINGLETON(DicomDataModel)->resetAllInteractions();
            qDebug() << "[GlobalKey] resetAllInteractions (7)";
            return true;  // 吞掉事件
        case Qt::Key_8: {
            auto* m = GET_SINGLETON(DicomDataModel);
            m->setCrosshairEnabled(!m->crosshairEnabled());
            qDebug() << "[GlobalKey] crosshairEnabled =" << m->crosshairEnabled() << "(8)";
            return true;  // 吞掉事件
        }
        case Qt::Key_9: {
            // 切换分割图像显示模式: 0=叠加 -> 1=仅原图 -> 2=仅分割 -> 0=叠加
            auto* m = GET_SINGLETON(DicomDataModel);
            if (m->isSegDataMode()) {
                int currentMode = m->segDisplayMode();
                int nextMode = (currentMode + 1) % 3;
                // 如果没有原始图像，跳过"仅原图"模式
                if (nextMode == 1 && !m->hasRawImage()) {
                    nextMode = 2;
                }
                m->setSegDisplayMode(nextMode);
                static const char* modeNames[] = { "Overlay", "OriginalOnly", "SegmentOnly" };
                qDebug() << "[GlobalKey] segDisplayMode =" << modeNames[nextMode] << "(9)";
            }
            return true;  // 吞掉事件，阻止多次触发
        }
        default: break;
        }
        if (mode >= 0) {
            GET_SINGLETON(DicomDataModel)->setToolMode(mode);
            qDebug() << "[GlobalKey] toolMode =" << mode;
            return false; // 仍让事件继续传递（不吞掉）
        }
        return false;
    }
};
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

// ============================================================================
// DICOM 网络测试函数
// ============================================================================
//void testDicomCFind(const QString& patientId)
//{
//    qDebug() << "\n========== DICOM C-FIND 测试 ==========";
//    
//    DicomNetwork dicom;
//    
//    // 使用默认配置 (可根据需要修改)
//    // dicom.setLocalAeTitle("SHHZLX");
//    // dicom.setRemoteAeTitle("ORANTHC");
//    // dicom.setRemoteHost("127.0.0.1");
//    // dicom.setRemotePort(8042);
//    
//    qDebug() << "本地 AE Title:" << dicom.localAeTitle();
//    qDebug() << "远程 AE Title:" << dicom.remoteAeTitle();
//    qDebug() << "远程服务器:" << dicom.remoteHost() << ":" << dicom.remotePort();
//    qDebug() << "查询患者ID:" << patientId;
//    qDebug() << "========================================\n";
//    
//    // 1. 先测试连接 (C-ECHO)
//    qDebug() << "[Step 1] 执行 C-ECHO 测试连接...";
//    bool echoOk = dicom.cEcho();
//    if (!echoOk) {
//        qWarning() << "C-ECHO 失败，无法连接到 PACS 服务器！";
//        return;
//    }
//    qDebug() << "C-ECHO 成功！\n";
//    
//    // 2. 查询患者 (C-FIND Patient)
//    qDebug() << "[Step 2] 执行 C-FIND 查询患者...";
//    QList<QVariantMap> patients = dicom.findPatients(patientId, "*");
//    
//    qDebug() << "\n========== 查询结果 ==========";
//    qDebug() << "找到" << patients.size() << "个患者:";
//    for (const auto& patient : patients) {
//        qDebug() << "  - 患者ID:" << patient.value("patientId").toString()
//                 << " 姓名:" << patient.value("patientName").toString()
//                 << " 性别:" << patient.value("patientSex").toString()
//                 << " 出生日期:" << patient.value("patientBirthDate").toString();
//    }
//    
//    // 3. 如果找到患者，继续查询检查
//    if (!patients.isEmpty()) {
//        QString pid = patients.first().value("patientId").toString();
//        qDebug() << "\n[Step 3] 查询患者" << pid << "的检查记录...";
//        QList<QVariantMap> studies = dicom.findStudies(pid, "*", "*");
//        
//        qDebug() << "找到" << studies.size() << "个检查:";
//        for (const auto& study : studies) {
//            qDebug() << "  - 检查日期:" << study.value("studyDate").toString()
//                     << " 描述:" << study.value("studyDescription").toString()
//                     << " 模态:" << study.value("modality").toString()
//                     << " UID:" << study.value("studyInstanceUid").toString();
//        }
//    }
//    
//    qDebug() << "\n========== 测试完成 ==========\n";
//}

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
    
    // ====== 检查命令行参数，执行 DICOM 测试 ======
    //testDicomCFind("N12188674");


    // 安装全局键盘监听（不依赖焦点）
    app.installEventFilter(new GlobalKeyFilter(&app));

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