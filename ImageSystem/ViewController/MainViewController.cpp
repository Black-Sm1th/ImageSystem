#include "MainViewController.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "Modules/BrainNetworkData.h"
vtkStandardNewMacro(SliceInteractorStyle);
vtkStandardNewMacro(SliceViewData);
vtkStandardNewMacro(VolumeViewData);

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
