#include "MainViewController.h"
#include <algorithm>
#include <vtkTransform.h>
#include <QQuickItemGrabResult>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QDirIterator>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QFont>
#include <QPainterPath>
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "Modules/BrainNetworkData.h"
#include "Model/DicomDataModel.h"
#include "Modules/SliceVtkItemBase.h"
#include "Modules/BidsConverter.h"

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
    setpredictedBrainAge(0.0);
    setbrainAgeProcessing(false);
    
    // 初始化扫描进度属性
    setisScanning(false);
    setscanTotalFolders(0);
    setscanScannedFolders(0);
    setscanFoundT1Count(0);
    setscanFoundBoldCount(0);
    setscanPairedCount(0);
    setscanProgress(0.0);
    setscanCurrentFolder("");
    
    // 初始化预分析状态
    setisPreAnalysisRunning(false);
    
    // 初始化表格模型
    m_brainRegionTableModel = new BrainRegionTableModel(this);
    m_brainSegmentationTableModel = GET_SINGLETON(DicomDataModel)->getSegmentationTableModel();
    m_mriPairResultModel = new MriPairResultModel(this);
    m_mriScanner = new BatchMriScanner();
    m_bidsConverter = new BidsConverter();
    QObject::connect(m_mriScanner, &BatchMriScanner::progressUpdated, this, &MainViewController::onScanProgressUpdated);
    QObject::connect(m_mriScanner, &BatchMriScanner::scanFinished, this, &MainViewController::onScanFinished);
    QObject::connect(m_bidsConverter, &BidsConverter::progressUpdated, this, &MainViewController::onConverterProgressUpdated);
    QObject::connect(m_bidsConverter, &BidsConverter::conversionFinished, this, &MainViewController::onConversionFinished);
    
    // 初始化 Docker 预处理运行器
    setupDockerPrepRunner();
    
    // 应用退出时停止进程并停止日志轮询
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() {
                    stopFmriprepProcess();
                    stopDeepprepProcess();
                });
    }
}

void MainViewController::setupDockerPrepRunner()
{
    m_dockerPrepRunner = new DockerPrepRunner(this);
    
    // 连接日志输出信号
    connect(m_dockerPrepRunner, &DockerPrepRunner::outputLog, this, [this](const QString& line) {
        appendPreAnalysisLog(line);
    });
    
    // 连接错误信号
    connect(m_dockerPrepRunner, &DockerPrepRunner::runError, this, [this](const QString& error) {
        appendPreAnalysisLog(QStringLiteral("\n>>> 错误：%1\n").arg(error));
    });
    
    // 连接 fMRIPrep 完成信号
    connect(m_dockerPrepRunner, &DockerPrepRunner::fmriPrepFinished, this, [this](int exitCode, const QString& message) {
        if (exitCode == 0) {
            qDebug() << QStringLiteral("fMRIPrep 运行成功！");
            appendPreAnalysisLog(QStringLiteral("\n>>> fMRIPrep 运行成功！\n"));
            writeMetadataFile(m_preAnalysisOutputPath, m_currentProcessingPairs);
        } else {
            qDebug() << QStringLiteral("fMRIPrep 运行失败！") << message;
            appendPreAnalysisLog(QStringLiteral("\n>>> fMRIPrep 运行失败！%1\n").arg(message));
            appendPreAnalysisLog(QStringLiteral(">>> 正在异步删除失败的输出目录...\n"));
            
            QString dirToDelete = m_preAnalysisOutputPath;
            QtConcurrent::run([dirToDelete]() {
                QDir dir(dirToDelete);
                if (dir.exists()) {
                    dir.removeRecursively();
                }
            });
        }
        stopPrepLogTimer();
        setisPreAnalysisRunning(false);
        appendPreAnalysisLog(QStringLiteral("\n========== 预处理完成 ==========\n"));
    });
    
    // 连接 DeepPrep 完成信号
    connect(m_dockerPrepRunner, &DockerPrepRunner::deepPrepFinished, this, [this](int exitCode, const QString& message) {
        if (exitCode == 0) {
            qDebug() << QStringLiteral("DeepPrep 运行成功！");
            appendPreAnalysisLog(QStringLiteral("\n>>> DeepPrep 运行成功！\n"));
            writeMetadataFile(m_preAnalysisOutputPath, m_currentProcessingPairs);
        } else {
            qDebug() << QStringLiteral("DeepPrep 运行失败！") << message;
            appendPreAnalysisLog(QStringLiteral("\n>>> DeepPrep 运行失败！%1\n").arg(message));
            appendPreAnalysisLog(QStringLiteral(">>> 正在异步删除失败的输出目录...\n"));

            QString dirToDelete = m_preAnalysisOutputPath;
            QtConcurrent::run([dirToDelete]() {
                QDir dir(dirToDelete);
                if (dir.exists()) {
                    dir.removeRecursively();
                }
            });
        }
        stopPrepLogTimer();
        setisPreAnalysisRunning(false);
        appendPreAnalysisLog(QStringLiteral("\n========== 预处理完成 ==========\n"));
    });
}

void MainViewController::clearPrepOutputsOnFailure(const QString& outputDir, bool isFmriPrep)
{
    const QString runnerName = isFmriPrep ? QStringLiteral("fMRIPrep") : QStringLiteral("DeepPrep");
    const QString absOut = QFileInfo(outputDir).absoluteFilePath();

    if (absOut.trimmed().isEmpty()) {
        appendPreAnalysisLog(QStringLiteral("\n>>> 清理跳过：输出目录为空\n"));
        return;
    }

    QDir out(absOut);
    if (!out.exists()) {
        appendPreAnalysisLog(QStringLiteral("\n>>> 清理跳过：输出目录不存在：%1\n").arg(absOut));
        return;
    }

    appendPreAnalysisLog(QStringLiteral("\n>>> %1 失败：开始清理输出目录（仅清理预处理产物）\n").arg(runnerName));
    appendPreAnalysisLog(QStringLiteral(">>> 输出目录：%1\n").arg(absOut));

    // 仅删除已知产物，避免误删用户其他文件
    const QStringList dirsToRemove = {
        "work", "logs", "sourcedata", "WorkDir", "BOLD", "QC", "Recon"
    };

    int removedDirs = 0;
    for (const QString& d : dirsToRemove) {
        const QString p = out.filePath(d);
        QDir dir(p);
        if (!dir.exists()) continue;
        if (dir.removeRecursively()) {
            removedDirs++;
            appendPreAnalysisLog(QStringLiteral(">>> 已删除目录：%1\n").arg(p));
        } else {
            appendPreAnalysisLog(QStringLiteral(">>> 删除目录失败：%1\n").arg(p));
        }
    }

    // 删除 sub-* 结果目录（被试输出）
    int removedSubDirs = 0;
    const QStringList entries = out.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& e : entries) {
        if (!e.startsWith("sub-")) continue;
        const QString p = out.filePath(e);
        QDir dir(p);
        if (dir.exists() && dir.removeRecursively()) {
            removedSubDirs++;
            appendPreAnalysisLog(QStringLiteral(">>> 已删除被试目录：%1\n").arg(p));
        } else {
            appendPreAnalysisLog(QStringLiteral(">>> 删除被试目录失败：%1\n").arg(p));
        }
    }

    // 删除日志/报告类文件（保守删除：只删我们已知会生成的）
    int removedFiles = 0;
    const QStringList patterns = {
        "*fmriprep-docker.log",
        "*deepprep-docker.log",
        "*.html",
        "desc-*_dseg.tsv",
        ".bidsignore"
    };

    for (const QString& pat : patterns) {
        const QStringList files = out.entryList(QStringList() << pat, QDir::Files, QDir::Name);
        for (const QString& f : files) {
            const QString p = out.filePath(f);
            if (QFile::remove(p)) {
                removedFiles++;
                appendPreAnalysisLog(QStringLiteral(">>> 已删除文件：%1\n").arg(p));
            } else {
                appendPreAnalysisLog(QStringLiteral(">>> 删除文件失败：%1\n").arg(p));
            }
        }
    }

    appendPreAnalysisLog(QStringLiteral(">>> 清理完成：删除目录=%1（其中 sub-*=%2），删除文件=%3\n")
                         .arg(removedDirs + removedSubDirs)
                         .arg(removedSubDirs)
                         .arg(removedFiles));
}

void MainViewController::writeMetadataFile(const QString& outputDir, const QList<MriPairResult>& pairs)
{
    QString filePath = QDir(outputDir).filePath("metadata.json");
    
    QJsonObject root;
    root["processDate"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    root["method"] = (m_preAnalysisMethod == 0) ? "fmriprep" : "deepprep";
    
    QJsonArray subjectsArr;
    for (const auto& pair : pairs) {
        QJsonObject sub;
        sub["patientName"] = pair.patientName;
        sub["patientId"] = pair.patientId;
        sub["patientSex"] = pair.patientSex;
        sub["patientBirthDate"] = pair.patientBirthDate;
        sub["studyDate"] = pair.studyDate;
        sub["t1SeriesDesc"] = pair.t1SeriesDesc;
        sub["boldSeriesDesc"] = pair.boldSeriesDesc;
        subjectsArr.append(sub);
    }
    root["subjects"] = subjectsArr;
    
    QJsonDocument doc(root);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        appendPreAnalysisLog(QStringLiteral(">>> 已生成映射文件：metadata.json\n"));
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 错误：无法生成映射文件\n"));
    }
}

QVariantMap MainViewController::readMetadataFile(const QString& outputDir)
{
    QString filePath = QDir(outputDir).filePath("metadata.json");
    QVariantMap result;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open metadata file:" << filePath;
        return result;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        result = doc.toVariant().toMap();
    }
    
    return result;
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
        qDebug() << QStringLiteral("路径为空");
        emit brainAnalysisFinished(false);
        return;
    }
    
    QString dirPath = url;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }
    
    QDir baseDir(dirPath);
    if (!baseDir.exists()) {
        qDebug() << QStringLiteral("路径不存在: ") << dirPath;
        emit brainAnalysisFinished(false);
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
                
                if (imageFiles.count() == 117) {
                    // ========== 符合逻辑一：已有完整的处理结果 ==========
                    qDebug() << QStringLiteral("检测到完整的脑网络分析结果!!!");
                    
                    if (loadOutputData(outputDir.absolutePath())) {
                        emit brainAnalysisStarted();
                        emit brainAnalysisFinished(true);
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
    // 首先检查fMRIPrep格式
    QString boldPath = baseDir.filePath("sub-01/func/sub-01_task-rest_space-MNI152NLin2009cAsym_desc-preproc_bold.nii.gz");
    QString confoundsPath = baseDir.filePath("sub-01/func/sub-01_task-rest_desc-confounds_timeseries.tsv");
    
    // 如果fMRIPrep格式不存在，检查DeepPrep格式
    if (!QFile::exists(boldPath) || !QFile::exists(confoundsPath)) {
        boldPath = baseDir.filePath("BOLD/sub-01/func/sub-01_task-rest_space-MNI152NLin6Asym_res-02_desc-preproc_bold.nii.gz");
        confoundsPath = baseDir.filePath("BOLD/sub-01/func/sub-01_task-rest_desc-confounds_timeseries.tsv");
        if (QFile::exists(boldPath) && QFile::exists(confoundsPath)) {
            qDebug() << QStringLiteral("检测到DeepPrep格式的脑功能数据文件!!!");
        }
    }
    
    if (QFile::exists(boldPath) && QFile::exists(confoundsPath)) {
        // ========== 符合逻辑二：原始数据文件存在，需要处理 ==========
        qDebug() << QStringLiteral("检测到原始脑功能数据文件!!!");
        // 创建输出目录
        QString outputDir = baseDir.filePath("outputDir");
        QDir outputDirObj(outputDir);
        if (!outputDirObj.exists()) {
            if (!outputDirObj.mkpath(".")) {
                qDebug() << QStringLiteral("无法创建输出目录: ") << outputDir;
                emit brainAnalysisFinished(false);
                return;
            }
        }
        
        // 调用 Python 脚本进行脑网络分析
        processBrainNetworkAnalysis(boldPath, confoundsPath, outputDir);
        
        return;
    }
    
    // ========== 不符合任何逻辑，发出错误信号 ==========
    qDebug() << QStringLiteral("未找到有效的脑功能数据!!!");
    emit brainAnalysisFinished(false);
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


void MainViewController::stopFmriprepProcess()
{
    if (m_dockerPrepRunner && m_dockerPrepRunner->isRunning()) {
        m_dockerPrepRunner->stop();
    }
    stopPrepLogTimer();
}

BrainRegionTableModel* MainViewController::getBrainRegionTableModel() const
{
    return m_brainRegionTableModel;
}

BrainSegmentationTableModel* MainViewController::getBrainSegmentationTableModel() const
{
    return m_brainSegmentationTableModel;
}

MriPairResultModel* MainViewController::getMriPairResultModel() const
{
    return m_mriPairResultModel;
}

void MainViewController::startAnalysisBrainAge(const QString& path, bool preprocess)
{
    if (path.isEmpty()) {
        qDebug() << QStringLiteral("路径为空");
        return;
    }

    QString inputPath = path;
    if (inputPath.startsWith("file:///")) {
        inputPath = inputPath.mid(8);
    }
    inputPath = QDir::toNativeSeparators(inputPath);

    if (!QFileInfo::exists(inputPath)) {
        qDebug() << QStringLiteral("路径不存在: %1").arg(inputPath);
        return;
    }

    // 清空上一次的结果
    setpredictedBrainAge(0.0);
    setbrainAgeProcessing(true);

    const QString exePath = QStringLiteral("Scripts/brain_age.exe");
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString outputPath = QStringLiteral("AppData/brain_age/Prediction_%1.csv").arg(timestamp);
    const QString modelPath = QStringLiteral("Scripts/model/DBN_model.h5");
    QStringList arguments;

    arguments << "--input" << inputPath
        << "--output" << outputPath
        << "--model" << modelPath
        << "--docker-image" << "deepbrain";
    if (preprocess) {
        arguments << "--preprocess";
    }
    QProcess* process = new QProcess(this);

    connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        Q_UNUSED(error);
        qDebug() << QStringLiteral("脑龄预测任务启动失败: %1").arg(process->errorString());
        setbrainAgeProcessing(false);
        process->deleteLater();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [=](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString stdOut = QString::fromUtf8(process->readAllStandardOutput());
            const QString stdErr = QString::fromUtf8(process->readAllStandardError());

            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                bool ok = false;
                double predictedAge = 0.0;

                // 优先从标准输出解析
                QRegularExpression re(QStringLiteral("Pred_Age\\s*=\\s*([\\d\\.]+)"));
                QRegularExpressionMatch match = re.match(stdOut);
                if (match.hasMatch()) {
                    predictedAge = match.captured(1).toDouble(&ok);
                }

                // 如果 stdout 没解析到，则尝试读取 csv
                if (!ok) {
                    QFile csvFile(outputPath);
                    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream ts(&csvFile);
                        while (!ts.atEnd()) {
                            const QString line = ts.readLine().trimmed();
                            if (line.isEmpty() || line.toLower().startsWith("id"))
                                continue;
                            const QStringList parts = line.split(',', Qt::KeepEmptyParts);
                            if (parts.size() >= 2) {
                                QString ageStr = parts[1].trimmed();
                                ageStr.remove('"');
                                predictedAge = ageStr.toDouble(&ok);
                                if (ok) break;
                            }
                        }
                    }
                }

                if (ok) {
                    setpredictedBrainAge(predictedAge);
                    qDebug() << QStringLiteral("脑龄预测完成，Pred_Age =") << predictedAge;
                } else {
                    qDebug() << QStringLiteral("脑龄预测完成，但未能解析结果。\n输出文件: %1\n输出信息: %2")
                                  .arg(outputPath, stdOut);
                }

                if (!stdErr.isEmpty()) {
                    qWarning() << QStringLiteral("脑龄预测警告/错误输出:") << stdErr;
                }
            } else {
                const QString errOutput = stdErr.isEmpty() ? process->errorString() : stdErr;
                qDebug() << QStringLiteral("脑龄预测失败！\n错误代码: %1\n%2")
                              .arg(exitCode)
                              .arg(errOutput);
            }

            setbrainAgeProcessing(false);
            process->deleteLater();
        });

    qDebug() << QStringLiteral("启动脑龄预测程序:") << exePath;
    qDebug() << QStringLiteral("参数:") << arguments;
    process->start(exePath, arguments);
}

void MainViewController::startPrepLogTimer(const QString& logFilePath)
{
    m_prepLogFilePath = logFilePath;
    m_prepLogReadPos = 0;
    
    if (!m_prepLogTimer) {
        m_prepLogTimer = new QTimer(this);
        m_prepLogTimer->setInterval(1000);
        connect(m_prepLogTimer, &QTimer::timeout, this, [this]() {
            if (m_prepLogFilePath.isEmpty()) return;
            QFile f(m_prepLogFilePath);
            if (!f.exists()) return;
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            if (m_prepLogReadPos > f.size()) {
                m_prepLogReadPos = 0;
            }
            if (!f.seek(m_prepLogReadPos)) return;
            QByteArray data = f.readAll();
            m_prepLogReadPos = f.pos();
            if (!data.isEmpty()) {
                appendPreAnalysisLog(QString::fromUtf8(data));
            }
        });
    }
    if (!m_prepLogTimer->isActive()) {
        m_prepLogTimer->start();
    }
}

void MainViewController::stopPrepLogTimer()
{
    if (m_prepLogTimer && m_prepLogTimer->isActive()) {
        m_prepLogTimer->stop();
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
                    qDebug() << QStringLiteral("脑网络分析完成，但加载结果失败");
                    emit brainAnalysisFinished(false);
                }
            } else {
                QString errorOutput = QString::fromUtf8(process->readAllStandardError());
                qWarning() << QStringLiteral("脑网络分析失败！退出代码:") << exitCode;
                qWarning() << QStringLiteral("错误信息:") << errorOutput;
                
                qDebug() << QStringLiteral("脑网络分析失败！错误代码: %1，信息: %2").arg(exitCode).arg(errorOutput.isEmpty() ? QStringLiteral("未知错误") : errorOutput);
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
        qDebug() << errorMsg;
        emit brainAnalysisFinished(false);
        process->deleteLater();
    });
    
    // 启动进程
    qDebug() << QStringLiteral("启动脑网络分析程序:") << scriptPath;
    
    process->start(scriptPath, arguments);
}

void MainViewController::generatePdfReport(const QString& savePath)
{
    QString pdfPath = savePath;
    if (pdfPath.startsWith("file:///")) {
        pdfPath = pdfPath.mid(8);
    }
    
    qDebug() << QStringLiteral("生成 PDF 报告:") << pdfPath;
    
    // 创建 PDF Writer
    QPdfWriter writer(pdfPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(96);
    QPageLayout layout = writer.pageLayout();
    layout.setMargins(QMarginsF(0, 0, 0, 0));
    writer.setPageLayout(layout);
    
    QPainter painter(&writer);
    if (!painter.isActive()) {
        qWarning() << QStringLiteral("无法创建 PDF 文件");
        return;
    }
    //背景图
    QRect targetRect = painter.viewport();
    QColor backgroundColor("#EFFAFF");
    painter.fillRect(targetRect, backgroundColor);
    //logo1
    QImage logo1(":/image/pdf-logo1.png");
    QRect targetRectLogo1(0, 0, logo1.width(), logo1.height());
    painter.drawImage(targetRectLogo1, logo1);
    //logo2
    QImage logo2(":/image/pdf-logo2.png");
    QRect targetRectLogo2(313, 137, logo2.width(), logo2.height());
    painter.drawImage(targetRectLogo2, logo2);
    //logo3
    QImage logo3(":/image/pdf-logo3.png");
    QRect targetRectLogo3(563, 867, logo3.width(), logo3.height());
    painter.drawImage(targetRectLogo3, logo3);
    //title1 - 创建列布局：上方图片，下方文字
    QImage title1(":/image/pdf-title1.png");
    int columnX = 90;  // 列的起始X坐标
    int columnY = 738;  // 列的起始Y坐标
    
    // 绘制title1图片
    QRect targetRectTitle1(columnX, columnY, title1.width(), title1.height());
    painter.drawImage(targetRectTitle1, title1);

    int textY = columnY + title1.height() + 13 + 30; 
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Normal));
    painter.setPen(QColor("#273967"));
    painter.drawText(columnX, textY, QStringLiteral("检测医院:") + QStringLiteral("南京脑科医院"));

    textY = textY + 50 + 20  + 20;
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Medium));
    painter.setPen(QColor("#273967"));
    painter.drawText(columnX, textY, QStringLiteral("患者姓名: ") + QStringLiteral("xxxxx"));

    textY = textY + 13 + 20 + 20;
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Medium));
    painter.setPen(QColor("#273967"));
    painter.drawText(columnX, textY, QStringLiteral("报告时间: ") + QDate::currentDate().toString("yyyy-MM-dd"));
    //第二页
    writer.newPage();

    QImage backgroundImage(":/image/pdf-background.png");
    painter.drawImage(targetRect, backgroundImage);

    int contentWidth = 563;
    int contentHeight = 932;
    int contentX = (targetRect.width() - contentWidth) / 2;
    int contentY = targetRect.height() - contentHeight;
    
    // 绘制只有上方圆角的白色背景矩形（无边框）
    QPainterPath path;
    path.moveTo(contentX, contentY + contentHeight); // 左下角
    path.lineTo(contentX, contentY + 24); // 左边线到圆角开始
    path.arcTo(contentX, contentY, 48, 48, 180, -90); // 左上圆角
    path.lineTo(contentX + contentWidth - 24, contentY); // 上边线
    path.arcTo(contentX + contentWidth - 48, contentY, 48, 48, 90, -90); // 右上圆角
    path.lineTo(contentX + contentWidth, contentY + contentHeight); // 右边线到底部
    path.closeSubpath();
    
    painter.setPen(Qt::NoPen); // 无边框
    painter.setBrush(QColor(Qt::white)); // 白色背景
    painter.drawPath(path);

    QImage contentImage(":/image/pdf-content.png");
    QRect contentRec(contentX + (contentWidth - contentImage.width()) / 2 ,163 , contentImage.width(), contentImage.height());
    painter.drawImage(contentRec, contentImage);


    int firstContentX = (contentWidth - 447) / 2 + contentX;
    int firstContentY = contentY + 130;
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Medium));
    painter.setPen(QColor("#000000"));
    painter.drawText(firstContentX, firstContentY, QStringLiteral("一、脑测量综合评估"));

    // 目录项绘制函数（自动右对齐）
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Normal));
    painter.setPen(QColor("#454545"));
    
    auto drawCatalogItem = [&](int yOffset, const QString& text, const QString& pageNum) {
        int lineY = firstContentY + yOffset;
        int catalogWidth = 447; // 目录总宽度
        
        // 保存当前字体和颜色
        QFont originalFont = painter.font();
        QPen originalPen = painter.pen();
        
        // 绘制左侧文本
        painter.drawText(firstContentX, lineY, text);
        
        // 计算文本宽度
        QFontMetrics fm = painter.fontMetrics();
        int textWidth = fm.horizontalAdvance(text);
        
        // 计算页码宽度（使用实际页码字体）
        QFont pageNumFont("Alibaba PuHuiTi 3.0", 11, QFont::Normal); // 16px
        QFontMetrics pageNumFm(pageNumFont);
        int pageNumWidth = pageNumFm.horizontalAdvance(pageNum);
        
        // 设置点号的字体（更小、更淡）
        QFont dotFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal); // 14px
        painter.setFont(dotFont);
        painter.setPen(QColor("#CCCCCC")); // 淡灰色
        
        // 计算点号区域宽度
        QFontMetrics dotFm(dotFont);
        int dotWidth = dotFm.horizontalAdvance(QStringLiteral("·"));
        int availableWidth = catalogWidth - textWidth - pageNumWidth - dotWidth; // 留一个点的间隙
        int dotCount = availableWidth / dotWidth;
        
        // 绘制点号（需要调整 y 坐标以对齐基线）
        QString dots;
        for (int i = 0; i < dotCount; i++) {
            dots += QStringLiteral("·");
        }
        painter.drawText(firstContentX + textWidth, lineY - 2, dots); // 微调 y 坐标使点号居中
        
        // 绘制页码（16px, #4E5969）
        painter.setFont(pageNumFont);
        painter.setPen(QColor("#4E5969"));
        painter.drawText(firstContentX + catalogWidth - pageNumWidth, lineY - 1, pageNum); // 微调y坐标对齐
        
        // 恢复原始字体和颜色
        painter.setFont(originalFont);
        painter.setPen(originalPen);
    };
    
    drawCatalogItem(70, QStringLiteral("1、综合评估结果"), QStringLiteral("3"));
    drawCatalogItem(120, QStringLiteral("2、异常区域分析及建议措施"), QStringLiteral("3"));

    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Medium));
    painter.setPen(QColor("#000000"));
    painter.drawText(firstContentX, firstContentY + 190, QStringLiteral("二、脑测量详细数据情况"));
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 15, QFont::Normal));
    painter.setPen(QColor("#454545"));
    drawCatalogItem(260, QStringLiteral("1、脑测量数据总览"), QStringLiteral("4"));
    drawCatalogItem(310, QStringLiteral("2、脑测量详细数据"), QStringLiteral("4"));
    drawCatalogItem(360, QStringLiteral("3、脑网络分析总览"), QStringLiteral("8"));
    drawCatalogItem(410, QStringLiteral("4、脑网络分析结果"), QStringLiteral("9"));
    drawCatalogItem(460, QStringLiteral("5、脑网络区域详细数据"), QStringLiteral("9"));
    drawCatalogItem(510, QStringLiteral("6、脑龄预测AI分析结果"), QStringLiteral("13"));

    // 第二页页码（居中，距底部40px）
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString page2Number = QStringLiteral("2/13");
    QFontMetrics page2Fm = painter.fontMetrics();
    int page2NumberWidth = page2Fm.horizontalAdvance(page2Number);
    int page2NumberX = (targetRect.width() - page2NumberWidth) / 2;
    int page2NumberY = targetRect.height() - 20;
    painter.drawText(page2NumberX, page2NumberY, page2Number);

    //第三页
            writer.newPage();
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 22, QFont::Medium));
    painter.setPen(QColor("#000000"));
    
    // 计算标题居中位置
    QString titleText = QStringLiteral("脑测量分析报告");
    QFontMetrics titleFm = painter.fontMetrics();
    int titleWidth = titleFm.horizontalAdvance(titleText);
    int titleX = (targetRect.width() - titleWidth) / 2;
    painter.drawText(titleX, 100, titleText);

    // 标题下方信息行（key和value使用不同字体）
    int infoY = 160;
    
    // 定义字体
    QFont keyFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal);
    QFont valueFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal);
    QColor keyColor("#86909C");
    QColor valueColor("#000000");
    
    // 构建信息数据（带固定宽度）
    struct InfoItem {
        QString key;
        QString value;
        int fixedWidth; // 固定宽度
    };
    
    QVector<InfoItem> infoData = {  
        {QStringLiteral("患者姓名："), QStringLiteral("xxx"), 130},
        {QStringLiteral("ID："), QStringLiteral("xxxxxxxxxx"), 140},
        {QStringLiteral("性别："), QStringLiteral("x"), 60},
        {QStringLiteral("年龄："), QStringLiteral("xx"), 70},
        {QStringLiteral("检查时间："), QStringLiteral("xxxx/xx/xx"), 170},
        {QStringLiteral("报告时间："), QDate::currentDate().toString("yyyy/MM/dd"), 160}
    };
    
    QFontMetrics keyFm(keyFont);
    QFontMetrics valueFm(valueFont);
    
    // 计算总宽度（固定宽度之和）
    int totalWidth = 0;
    for (const auto& item : infoData) {
        totalWidth += item.fixedWidth;
    }
    
    // 起始x坐标（居中）
    int currentX = (targetRect.width() - totalWidth) / 2;
    
    // 逐个绘制key-value对
    for (const auto& item : infoData) {
        // 绘制key
        painter.setFont(keyFont);
        painter.setPen(keyColor);
        painter.drawText(currentX, infoY, item.key);
        
        // 计算value的起始位置（紧跟key后）
        int valueX = currentX + keyFm.horizontalAdvance(item.key);
        
        // 绘制value
        painter.setFont(valueFont);
        painter.setPen(valueColor);
        painter.drawText(valueX, infoY, item.value);
        
        // 移动到下一个固定宽度位置
        currentX += item.fixedWidth;
    }

    QImage title2(":/image/pdf-title2.png");

    // 绘制title1图片
    QRect targetRectTitle2(0, 190, title2.width() / 2, title2.height() / 2);
    painter.drawImage(targetRectTitle2, title2);

    QImage logo4(":/image/pdf-logo4.png");
    // 绘制logo4图片
    QRect targetRectlogo4(50, 250, logo4.width() / 2, logo4.height() / 2);
    painter.drawImage(targetRectlogo4, logo4);

    // 绘制"脑龄预测"文字（居中在logo4的X方向中心）
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#FFFFFF"));
    QString brainAgeText = QStringLiteral("脑龄预测");
    QFontMetrics brainAgeFm = painter.fontMetrics();
    int brainAgeTextWidth = brainAgeFm.horizontalAdvance(brainAgeText);
    int logo4CenterX = 50 + logo4.width() / 4; // logo4的X方向中心
    int brainAgeTextX = logo4CenterX - brainAgeTextWidth / 2; // 文字居中
    painter.drawText(brainAgeTextX, 290, brainAgeText);

    // 绘制两位数数字（居中在logo4的X方向中心，Y=340）
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 24, QFont::Bold));
    painter.setPen(QColor("#FFFFFF"));
    QString ageNumber = QString::number(qRound(getpredictedBrainAge())); // 示例两位数
    QFontMetrics ageNumberFm = painter.fontMetrics();
    int ageNumberWidth = ageNumberFm.horizontalAdvance(ageNumber);
    int ageNumberX = logo4CenterX - ageNumberWidth / 2; // 数字居中
    painter.drawText(ageNumberX, 325, ageNumber);

    // 绘制delta值
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Bold));
    painter.setPen(QColor("#FFFFFF"));
    QString deltaAgeNumber = QStringLiteral("+x"); // 示例两位数
    QFontMetrics deltaAgeNumberFm = painter.fontMetrics();
    int deltaAgeNumberWidth = deltaAgeNumberFm.horizontalAdvance(deltaAgeNumber);
    int deltaAgeNumberX = logo4CenterX - deltaAgeNumberWidth / 2; // 数字居中
    painter.drawText(deltaAgeNumberX + 32, 344, deltaAgeNumber);

    // 绘制"综合评估"标题
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 18, QFont::Medium));
    painter.setPen(QColor("#273967"));
    int evaluationX = 50 + logo4.width() / 2 + 30;
    int evaluationY = 280;
    painter.drawText(evaluationX, evaluationY, QStringLiteral("综合评估"));

    // 绘制评估文字段落（550*60，自动换行，混合样式）
    int textBoxX = evaluationX;
    int textBoxY = evaluationY + 30; // 下方30px
    int textBoxWidth = 550;
    
    // 定义不同样式的字体
    QFont normalFont("Alibaba PuHuiTi 3.0", 11, QFont::Normal);
    QFont highlightFont("Alibaba PuHuiTi 3.0", 16, QFont::Medium);
    QColor normalColor("#1D2129");
    QColor highlightColor("#4080FF");
    
    // 定义文本段落（交替的普通文本和高亮文本）
    struct TextSegment {
        QString text;
        QFont font;
        QColor color;
    };
    
    QVector<TextSegment> segments = {
        {QStringLiteral("根据系统监测，脑龄预测年龄为"), normalFont, normalColor},
        {QString::number(qRound(getpredictedBrainAge())) + QStringLiteral("岁"), highlightFont, highlightColor},
        {QStringLiteral("，较实际年龄长"), normalFont, normalColor},
        {QStringLiteral("x年"), highlightFont, highlightColor}, 
        {QStringLiteral("。"), normalFont, normalColor},
    };
    
    // 逐段绘制文本
    int segmentX = textBoxX;
    int segmentY = textBoxY;
    int lineHeight = 20; // 行高
    
    for (const auto& segment : segments) {
        painter.setFont(segment.font);
        painter.setPen(segment.color);
        QFontMetrics fm(segment.font);
        
        // 获取文本宽度
        int segmentWidth = fm.horizontalAdvance(segment.text);
        
        // 检查是否需要换行
        if (segmentX + segmentWidth > textBoxX + textBoxWidth && segmentX > textBoxX) {
            segmentX = textBoxX;
            segmentY += lineHeight;
        }
        
        // 绘制文本
        painter.drawText(segmentX, segmentY, segment.text);
        segmentX += segmentWidth;
    }

    // 在logo4下方20px绘制圆角矩形背景
    int boxX = 32;
    int boxY = 250 + logo4.height() / 2 + 30; // logo4底部 + 20px
    int boxWidth = 730;
    int boxHeight = 686;
    int boxRadius = 4; // 圆角半径
    
    QPainterPath boxPath;
    boxPath.addRoundedRect(boxX, boxY, boxWidth, boxHeight, boxRadius, boxRadius);
    painter.setPen(Qt::NoPen); // 无边框
    painter.setBrush(QColor("#ECF4FF")); // 背景色
    painter.drawPath(boxPath);

    // 在boxPath中间添加pdf-logo5图片
    QImage logo5(":/image/pdf-logo5.png");
    int logo5X = boxX + (boxWidth - logo5.width() / 2 ) / 2; // 水平居中
    int logo5Y = boxY + 10; // 相比boxPath向下20px
    QRect targetRectLogo5(logo5X, logo5Y, logo5.width() / 2, logo5.height() / 2);
    painter.drawImage(targetRectLogo5, logo5);

    // 在距boxPath底部20的地方添加pdf-result图片（居中）
    QImage pdfResult(":/image/pdf-result.png");
    int resultX = boxX + (boxWidth - pdfResult.width() / 2) / 2; // 水平居中
    int resultY = boxY + boxHeight - 30 - pdfResult.height() / 2; // 距底部30
    QRect targetRectResult(resultX, resultY, pdfResult.width() / 2, pdfResult.height() / 2);
    painter.drawImage(targetRectResult, pdfResult);

    // 在pdf-result的右下角添加pdf-human图片
    QImage pdfHuman(":/image/pdf-human.png");
    int humanX = resultX + pdfResult.width() / 2 - pdfHuman.width() / 2; // 右对齐
    int humanY = resultY + pdfResult.height() / 2 - pdfHuman.height() / 2; // 底部对齐
    QRect targetRectHuman(humanX - 10, humanY - 20, pdfHuman.width() / 2, pdfHuman.height() / 2);
    painter.drawImage(targetRectHuman, pdfHuman);

    // 第三页页码（居中，距底部40px）
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString page3Number = QStringLiteral("3/13");
    QFontMetrics page3Fm = painter.fontMetrics();
    int page3NumberWidth = page3Fm.horizontalAdvance(page3Number);
    int page3NumberX = (targetRect.width() - page3NumberWidth) / 2;
    int page3NumberY = targetRect.height() - 20;
    painter.drawText(page3NumberX, page3NumberY, page3Number);

    // 第四页
                writer.newPage();
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString pageTopName = QStringLiteral("脑测量分析报告");
    QFontMetrics pageTopNameFm = painter.fontMetrics();
    int pageTopNameWidth = pageTopNameFm.horizontalAdvance(pageTopName);
    int pageTopNameX = (targetRect.width() - pageTopNameWidth) / 2;
    int pageTopNameY = 50;
    painter.drawText(pageTopNameX, pageTopNameY, pageTopName);

    QImage title3(":/image/pdf-title3.png");
    // 绘制title3图片
    QRect targetRectTitle3(0, 80, title3.width() / 2, title3.height() / 2);
    painter.drawImage(targetRectTitle3, title3);

    // 生成四张切片图片到临时文件夹
    QString tempDir = QDir::tempPath() + "/brain_seg_images";
    QDir().mkpath(tempDir);
    
    QString axialPath, coronalPath, sagittalPath, seg3dPath;
    GET_SINGLETON(DicomDataModel)->generateSegDataPNGs(tempDir, axialPath, coronalPath, sagittalPath, seg3dPath);
    
    // 在title3下方并列显示四张图片
    int imageStartY = 80 + title3.height() / 2; // 紧贴title3
    int imageSpacing = 10; // 图片间距
    int availableWidth = targetRect.width() - 40; // 左右各留20px边距
    int imageWidth = (availableWidth - 3 * imageSpacing) / 4; // 四张图片均分宽度
    
    // 加载并绘制四张图片
    QStringList imagePaths = { seg3dPath, axialPath, sagittalPath, coronalPath };
    QStringList imageLabels = { "", "Axial View", "Sagittal View", "Coronal View" };
    int currentImageX = 20; // 起始X坐标（左边距）
    
    for (int i = 0; i < imagePaths.size(); ++i) {
        const QString& imgPath = imagePaths[i];
        if (QFile::exists(imgPath)) {
            QImage segImage(imgPath);
            if (!segImage.isNull()) {
                // 保持宽高比缩放
                int imageHeight = imageWidth * segImage.height() / segImage.width();
                // 3D图需要20px间隙，其他切面视图不需要
                int actualImageY = (i == 0) ? imageStartY + 20 : imageStartY;
                QRect imageRect(currentImageX, actualImageY, imageWidth, imageHeight);
                painter.drawImage(imageRect, segImage);
                
                // 为后三张图片添加标识文字
                if (i > 0) { // 跳过第一张（3D视图）
                    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
                    
                    QFontMetrics labelFm = painter.fontMetrics();
                    int labelWidth = labelFm.horizontalAdvance(imageLabels[i]);
                    int labelHeight = labelFm.height();
                    int labelAscent = labelFm.ascent();
                    
                    // 背景框尺寸和位置
                    int padding = 6; // 文字周围的内边距
                    int boxWidth = labelWidth + padding * 2;
                    int boxHeight = labelHeight + padding * 2;
                    int boxX = currentImageX + (imageWidth - boxWidth) / 2; // 框居中
                    int boxY = actualImageY + imageHeight; // 紧贴图片底部
                    
                    // 绘制圆角背景框
                    QPainterPath labelBoxPath;
                    labelBoxPath.addRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, 8);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor("#F7F8FA"));
                    painter.drawPath(labelBoxPath);
                    
                    // 绘制文字（垂直居中在框中）
                    int labelX = boxX + padding;
                    int labelY = boxY + padding + labelAscent; // 使用ascent确保文字基线正确
                    painter.setPen(QColor("#1D2129"));
                    painter.drawText(labelX, labelY, imageLabels[i]);
                }
                
                currentImageX += imageWidth + imageSpacing;
            }
        }
    }

    QImage title4(":/image/pdf-title4.png");
    // 绘制title4图片
    QRect targetRectTitle4(0, imageStartY + 220, title4.width() / 2, title4.height() / 2);
    painter.drawImage(targetRectTitle4, title4);

    // ========== 脑分割表格 ==========
    int tableStartY = imageStartY + 220 + title4.height() / 2 + 10; // title4下方10px
    
    // 表格列宽设置
    int tableAvailableWidth = targetRect.width() - 64; // 左右各留40px边距
    int segColWidths[5];
    // 中文名35%, 位置15%, 容积17%, 全脑占比17%, 不对称16%
    segColWidths[0] = (int)(tableAvailableWidth * 0.35);  // 中文名称
    segColWidths[1] = (int)(tableAvailableWidth * 0.15);  // 位置
    segColWidths[2] = (int)(tableAvailableWidth * 0.17);  // 容积(cm³)
    segColWidths[3] = (int)(tableAvailableWidth * 0.17);  // 全脑占比
    segColWidths[4] = (int)(tableAvailableWidth * 0.16);  // 不对称指数
    
    int segTotalWidth = 0;
    for (int w : segColWidths) segTotalWidth += w;
    
    QStringList segHeaders = {QStringLiteral("中文名称"), QStringLiteral("位置"), 
                              QStringLiteral("容积(cm³)"), QStringLiteral("全脑占比"), 
                              QStringLiteral("不对称指数")};
    
    // 设置表格字体和行高
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Medium));
    QFontMetrics tableFm = painter.fontMetrics();
    int rowHeight = tableFm.height() * 2.5 - 10; // 行高为字体高度的2.5倍减10px
    
    // 绘制表头
    int tableStartX = (targetRect.width() - segTotalWidth) / 2;
    if (tableStartX < 32) tableStartX = 32; // 至少保留40的左边距
    int tableX = tableStartX;
    int tableY = tableStartY;
    
    // 表头背景和文字
    painter.setPen(QPen(QColor("#5B5B5B"), 1));
    for (int i = 0; i < segHeaders.size(); i++) {
        painter.setBrush(QColor("#F7F8FA"));
        painter.drawRect(tableX, tableY, segColWidths[i], rowHeight);
        painter.setPen(QColor("#000000"));
        painter.drawText(QRect(tableX + 5, tableY, segColWidths[i] - 10, rowHeight), 
                        Qt::AlignCenter | Qt::AlignVCenter, segHeaders[i]);
        painter.setPen(QPen(QColor("#5B5B5B"), 1));
        tableX += segColWidths[i];
    }
    tableY += rowHeight;
    
    // 页码计数器（第四页开始）
    int currentPageNumber = 4;
    
    // 绘制脑分割数据
    if (m_brainSegmentationTableModel) {
        int segRowCount = m_brainSegmentationTableModel->rowCount();
        // 计算当前页面可以容纳的最大行数（底部留50px用于页码）
        int maxRowsPerPage = (targetRect.height() - tableY - 50) / rowHeight;
        int currentRow = 0;
        
        for (int row = 0; row < segRowCount; row++) {
            if (currentRow >= maxRowsPerPage) {
                // 绘制当前页页码（底部居中）
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
                painter.setPen(QColor("#C9CDD4"));
                QString pageNumber = QString("%1/13").arg(currentPageNumber);
                QFontMetrics pageNumberFm = painter.fontMetrics();
                int pageNumberWidth = pageNumberFm.horizontalAdvance(pageNumber);
                int pageNumberX = (targetRect.width() - pageNumberWidth) / 2;
                int pageNumberY = targetRect.height() - 20;
                painter.drawText(pageNumberX, pageNumberY, pageNumber);
                
                // 换页
                writer.newPage();
                currentPageNumber++; // 页码递增
                
                // 绘制页面顶部标题
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
                painter.setPen(QColor("#C9CDD4"));
                QString pageTopName = QStringLiteral("脑测量分析报告");
                QFontMetrics pageTopNameFm = painter.fontMetrics();
                int pageTopNameWidth = pageTopNameFm.horizontalAdvance(pageTopName);
                int pageTopNameX = (targetRect.width() - pageTopNameWidth) / 2;
                painter.drawText(pageTopNameX, 50, pageTopName);
                
                // 绘制title4图片（在标题下方）
                QRect targetRectTitle4NewPage(0, 80, title4.width() / 2, title4.height() / 2);
                painter.drawImage(targetRectTitle4NewPage, title4);
                
                tableY = 80 + title4.height() / 2 + 10; // title4下方10px
                currentRow = 0;
                
                // 重新绘制表头
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Medium));
                tableX = tableStartX;
                painter.setPen(QPen(QColor("#5B5B5B"), 1));
                for (int i = 0; i < segHeaders.size(); i++) {
                    painter.setBrush(QColor("#F7F8FA"));
                    painter.drawRect(tableX, tableY, segColWidths[i], rowHeight);
                    painter.setPen(QColor("#000000"));
                    painter.drawText(QRect(tableX + 5, tableY, segColWidths[i] - 10, rowHeight), 
                                    Qt::AlignCenter | Qt::AlignVCenter, segHeaders[i]);
                    painter.setPen(QPen(QColor("#5B5B5B"), 1));
                    tableX += segColWidths[i];
                }
                tableY += rowHeight;
                
                // 换页后重新计算可容纳的最大行数（新页面从title4下方开始）
                maxRowsPerPage = (targetRect.height() - tableY - 50) / rowHeight;
            }
            
            tableX = tableStartX;
            QModelIndex idx = m_brainSegmentationTableModel->index(row, 0);
            
            // 数据行背景（交替颜色）
            painter.setBrush(row % 2 == 0 ? QColor("#FFFFFF") : QColor("#F7F8FA"));
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
            
            // 中文名称
            QString chName = m_brainSegmentationTableModel->data(idx, BrainSegmentationTableModel::ChineseNameRole).toString();
            painter.drawRect(tableX, tableY, segColWidths[0], rowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(tableX + 5, tableY, segColWidths[0] - 10, rowHeight), 
                            Qt::AlignCenter | Qt::AlignVCenter, chName);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            tableX += segColWidths[0];
            
            // 位置
            QString hemisphere = m_brainSegmentationTableModel->data(idx, BrainSegmentationTableModel::HemisphereRole).toString();
            painter.drawRect(tableX, tableY, segColWidths[1], rowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(tableX + 5, tableY, segColWidths[1] - 10, rowHeight), 
                            Qt::AlignCenter | Qt::AlignVCenter, hemisphere);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            tableX += segColWidths[1];
            
            // 容积
            QString volume = m_brainSegmentationTableModel->data(idx, BrainSegmentationTableModel::VolumeRole).toString();
            painter.drawRect(tableX, tableY, segColWidths[2], rowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(tableX + 5, tableY, segColWidths[2] - 10, rowHeight), 
                            Qt::AlignCenter | Qt::AlignVCenter, volume);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            tableX += segColWidths[2];
            
            // 全脑占比
            QString volumePercent = m_brainSegmentationTableModel->data(idx, BrainSegmentationTableModel::VolumePercentRole).toString();
            painter.drawRect(tableX, tableY, segColWidths[3], rowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(tableX + 5, tableY, segColWidths[3] - 10, rowHeight), 
                            Qt::AlignCenter | Qt::AlignVCenter, volumePercent);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            tableX += segColWidths[3];
            
            // 不对称指数
            QString asymmetry = m_brainSegmentationTableModel->data(idx, BrainSegmentationTableModel::AsymmetryIndexRole).toString();
            painter.drawRect(tableX, tableY, segColWidths[4], rowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(tableX + 5, tableY, segColWidths[4] - 10, rowHeight), 
                            Qt::AlignCenter | Qt::AlignVCenter, asymmetry);
            
            tableY += rowHeight;
            currentRow++;
        }
    }
    
    // ========== 绘制title5图片 ==========
    QImage title5(":/image/pdf-title5.png");
    int title5Spacing = 20; // title5与表格的间距
    int title5Height = title5.height() / 2;
    
    // 判断当前页是否有足够空间显示title5（需要title5高度 + 上下间距 + 页码空间）
    int requiredSpace = title5Spacing + title5Height + 50; // 50px留给页码
    if (tableY + requiredSpace > targetRect.height()) {
        // 空间不足，绘制当前页页码并换页
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString pageNumber = QString("%1/13").arg(currentPageNumber);
        QFontMetrics pageNumberFm = painter.fontMetrics();
        int pageNumberWidth = pageNumberFm.horizontalAdvance(pageNumber);
        int pageNumberX = (targetRect.width() - pageNumberWidth) / 2;
        int pageNumberY = targetRect.height() - 20;
        painter.drawText(pageNumberX, pageNumberY, pageNumber);
        
        // 换页
        writer.newPage();
        currentPageNumber++;
        
        // 绘制页面顶部标题
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString pageTopName = QStringLiteral("脑测量分析报告");
        QFontMetrics pageTopNameFm = painter.fontMetrics();
        int pageTopNameWidth = pageTopNameFm.horizontalAdvance(pageTopName);
        int pageTopNameX = (targetRect.width() - pageTopNameWidth) / 2;
        painter.drawText(pageTopNameX, 50, pageTopName);
        
        tableY = 80; // 从页面顶部开始
    } else {
        tableY += title5Spacing; // 添加间距
    }
    
    // 绘制title5图片（居左）
    QRect targetRectTitle5(0, tableY, title5.width() / 2, title5Height);
    painter.drawImage(targetRectTitle5, title5);
    tableY += title5Height;
    
    // ========== 绘制四张图片（两行两列）==========
    // 尝试从currentAlffUrl等属性推导输出目录，如果没有则使用默认路径
    QString outputDir;
    if (!getcurrentAlffUrl().isEmpty()) {
        // 从alff.png的路径推导出输出目录
        QString alffUrl = getcurrentAlffUrl();
        // 去掉 file:/// 前缀
        if (alffUrl.startsWith("file:///")) {
            alffUrl = alffUrl.mid(8); // 移除 "file:///"
        }
        outputDir = QFileInfo(alffUrl).absolutePath();
    } else {
        // 使用默认路径（可能需要根据实际情况调整）
        outputDir = "E:/output";
    }
    
    // 构建图片路径
    QStringList imageNetPaths;
    imageNetPaths << outputDir + "/region_plots/001_Precentral_L_transparent.png"  // 左上
               << outputDir + "/alff_transparent.png"                                        // 右上
               << outputDir + "/covariance_transparent.png"                                  // 左下
               << outputDir + "/viewConnectome.png";                                                             // 右下（空）
    
    // 两行两列布局参数
    int imagesSpacing = 20; // 图片间距
    int imagesStartY = tableY + 20; // title5下方20px
    int imageMargin = 32; // 左右边距
    int availableWidthForImages = targetRect.width() - 2 * imageMargin;
    int singleImageWidth = (availableWidthForImages - imagesSpacing) / 2 - 20; // 两列，中间一个间距，减小20px
    int singleImageHeight = 180; // 每张图片的高度（从200减小到180）
    int rowSpacing = 20; // 行间距
    
    // 绘制四张图片（两行两列）- 不进行换页判断
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            int index = row * 2 + col;
            if (index >= imageNetPaths.size()) break;
            
            QString imgPath = imageNetPaths[index];
            if (imgPath.isEmpty()) continue; // 跳过空位置
            
            // 计算图片位置
            int imgX = imageMargin + col * (singleImageWidth + imagesSpacing);
            int imgY = imagesStartY + row * (singleImageHeight + rowSpacing);
            
            // 后面两张图片（索引2和3）添加背景矩形
            if (index == 2) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor("#F7F8FA"));
                painter.drawRect(imgX, imgY, singleImageWidth, singleImageHeight);
            }
            
            // 加载并绘制图片
            if (QFile::exists(imgPath)) {
                QImage image(imgPath);
                if (!image.isNull()) {
                    // 计算保持宽高比的目标尺寸
                    QSize imageSize = image.size();
                    QSize targetSize(singleImageWidth, singleImageHeight);
                    imageSize.scale(targetSize, Qt::KeepAspectRatio);
                    
                    // 计算居中位置
                    int drawX = imgX + (singleImageWidth - imageSize.width()) / 2;
                    int drawY = imgY + (singleImageHeight - imageSize.height()) / 2;
                    
                    // 直接绘制原图到目标矩形，让QPainter进行高质量缩放
                    QRect targetRect(drawX, drawY, imageSize.width(), imageSize.height());
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    painter.setRenderHint(QPainter::Antialiasing, true);
                    painter.drawImage(targetRect, image);
                } else {
                    // 图片加载失败，绘制占位框
                    painter.setPen(QPen(QColor("#C9CDD4"), 1, Qt::DashLine));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(imgX, imgY, singleImageWidth, singleImageHeight);
                    
                    // 绘制提示文字
                    painter.setPen(QColor("#86909C"));
                    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
                    painter.drawText(QRect(imgX, imgY, singleImageWidth, singleImageHeight),
                                   Qt::AlignCenter, QStringLiteral("图片加载失败"));
                }
            } else {
                // 图片不存在，绘制占位框
                painter.setPen(QPen(QColor("#C9CDD4"), 1, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(imgX, imgY, singleImageWidth, singleImageHeight);
                
                // 绘制提示文字
                painter.setPen(QColor("#86909C"));
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
                painter.drawText(QRect(imgX, imgY, singleImageWidth, singleImageHeight),
                               Qt::AlignCenter, QStringLiteral("图片不存在"));
            }
        }
    }
    
    // 绘制当前页的页码
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString finalPageNumber = QString("%1/13").arg(currentPageNumber);
    QFontMetrics finalPageNumberFm = painter.fontMetrics();
    int finalPageNumberWidth = finalPageNumberFm.horizontalAdvance(finalPageNumber);
    int finalPageNumberX = (targetRect.width() - finalPageNumberWidth) / 2;
    int finalPageNumberY = targetRect.height() - 20;
    painter.drawText(finalPageNumberX, finalPageNumberY, finalPageNumber);

    writer.newPage();
    currentPageNumber++;

    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString pageTopName2 = QStringLiteral("脑测量分析报告");
    QFontMetrics pageTopNameFm2 = painter.fontMetrics();
    int pageTopNameWidth2 = pageTopNameFm2.horizontalAdvance(pageTopName2);
    int pageTopNameX2 = (targetRect.width() - pageTopNameWidth2) / 2;
    painter.drawText(pageTopNameX2, 50, pageTopName2);
    
    // ========== 绘制title6图片 ==========
    QImage title6(":/image/pdf-title6.png");
    int title6Y = 80;
    QRect targetRectTitle6(0, title6Y, title6.width() / 2, title6.height() / 2);
    painter.drawImage(targetRectTitle6, title6);
    
    // ========== 显示脑网络指标表格 ==========
    int indicatorsStartY = title6Y + title6.height() / 2 + 20; // title6下方20px
    int indicatorMargin = 32; // 左右边距
    
    // 定义六个指标
    QStringList indicatorLabels = {
        QStringLiteral("全局效率"),
        QStringLiteral("平均局部效率"),
        QStringLiteral("平均聚类系数"),
        QStringLiteral("富俱乐部连接"),
        QStringLiteral("桥接连接"),
        QStringLiteral("局部连接")
    };
    
    QStringList indicatorValues = {
        QString::number(getglobalEfficiency(), 'f', 4),
        QString::number(getaverageLocalEfficiency(), 'f', 4),
        QString::number(getaverageClusteringCoefficient(), 'f', 4),
        QString::number(getrichClubConnections(), 'f', 4),
        QString::number(getbridgeConnections(), 'f', 4),
        QString::number(getlocalConnections(), 'f', 4)
    };
    
    // 计算表格列宽
    int indicatorTableWidth = targetRect.width() - 2 * indicatorMargin;
    int indicatorColWidth = indicatorTableWidth / 6; // 6列均分
    
    // 设置表格字体和行高
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Medium));
    QFontMetrics indicatorFm = painter.fontMetrics();
    int indicatorRowHeight = indicatorFm.height() * 2.5 - 10;
    
    int indicatorTableX = indicatorMargin;
    int indicatorTableY = indicatorsStartY;
    
    // 绘制表头
    painter.setPen(QPen(QColor("#5B5B5B"), 1));
    for (int i = 0; i < indicatorLabels.size(); i++) {
        painter.setBrush(QColor("#F7F8FA"));
        painter.drawRect(indicatorTableX, indicatorTableY, indicatorColWidth, indicatorRowHeight);
        painter.setPen(QColor("#000000"));
        painter.drawText(QRect(indicatorTableX + 5, indicatorTableY, indicatorColWidth - 10, indicatorRowHeight),
                        Qt::AlignCenter | Qt::AlignVCenter, indicatorLabels[i]);
        painter.setPen(QPen(QColor("#5B5B5B"), 1));
        indicatorTableX += indicatorColWidth;
    }
    indicatorTableY += indicatorRowHeight;
    
    // 绘制数据行
    indicatorTableX = indicatorMargin;
    painter.setPen(QPen(QColor("#5B5B5B"), 1));
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
    for (int i = 0; i < indicatorValues.size(); i++) {
        painter.setBrush(QColor("#FFFFFF"));
        painter.drawRect(indicatorTableX, indicatorTableY, indicatorColWidth, indicatorRowHeight);
        painter.setPen(QColor("#1D2129"));
        painter.drawText(QRect(indicatorTableX + 5, indicatorTableY, indicatorColWidth - 10, indicatorRowHeight),
                        Qt::AlignCenter | Qt::AlignVCenter, indicatorValues[i]);
        painter.setPen(QPen(QColor("#5B5B5B"), 1));
        indicatorTableX += indicatorColWidth;
    }
    
    // ========== 绘制title7图片 ==========
    QImage title7(":/image/pdf-title7.png");
    int title7Spacing = 20;
    int title7Height = title7.height() / 2;
    int title7Y = indicatorTableY + indicatorRowHeight + title7Spacing; // 指标表格下方20px
    
    // 判断是否需要换页显示title7
    int requiredSpaceForTitle7 = title7Height + 100; // title7高度 + 表格表头最小高度
    if (title7Y + requiredSpaceForTitle7 > targetRect.height() - 50) {
        // 空间不足，绘制当前页页码并换页
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString pageNumber = QString("%1/13").arg(currentPageNumber);
        QFontMetrics pageNumberFm = painter.fontMetrics();
        int pageNumberWidth = pageNumberFm.horizontalAdvance(pageNumber);
        int pageNumberX = (targetRect.width() - pageNumberWidth) / 2;
        int pageNumberY = targetRect.height() - 20;
        painter.drawText(pageNumberX, pageNumberY, pageNumber);
        
        // 换页
        writer.newPage();
        currentPageNumber++;
        
        // 绘制新页面顶部标题
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString newPageTopName = QStringLiteral("脑测量分析报告");
        QFontMetrics newPageTopNameFm = painter.fontMetrics();
        int newPageTopNameWidth = newPageTopNameFm.horizontalAdvance(newPageTopName);
        int newPageTopNameX = (targetRect.width() - newPageTopNameWidth) / 2;
        painter.drawText(newPageTopNameX, 50, newPageTopName);
        
        title7Y = 80; // 从新页面顶部开始
    }
    
    // 绘制title7图片
    QRect targetRectTitle7(0, title7Y, title7.width() / 2, title7Height);
    painter.drawImage(targetRectTitle7, title7);
    
    // ========== 绘制脑网络列表表格 ==========
    int networkTableStartY = title7Y + title7Height + 20; // title7下方20px
    
    // 表格列宽设置 - 6列：序号、中文名称、英文名称、度、聚类系数、局部效率
    int networkTableWidth = targetRect.width() - 64; // 左右各留32px边距
    int networkColWidths[6];
    networkColWidths[0] = (int)(networkTableWidth * 0.08);  // 序号
    networkColWidths[1] = (int)(networkTableWidth * 0.28);  // 中文名称
    networkColWidths[2] = (int)(networkTableWidth * 0.28);  // 英文名称
    networkColWidths[3] = (int)(networkTableWidth * 0.12);  // 度
    networkColWidths[4] = (int)(networkTableWidth * 0.12);  // 聚类系数
    networkColWidths[5] = (int)(networkTableWidth * 0.12);  // 局部效率
    
    int networkTotalWidth = 0;
    for (int w : networkColWidths) networkTotalWidth += w;
    
    QStringList networkHeaders = {
        QStringLiteral("序号"),
        QStringLiteral("中文名称"),
        QStringLiteral("英文名称"),
        QStringLiteral("度"),
        QStringLiteral("聚类系数"),
        QStringLiteral("局部效率")
    };
    
    // 设置表格字体和行高
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Medium));
    QFontMetrics networkTableFm = painter.fontMetrics();
    int networkRowHeight = networkTableFm.height() * 2.5 - 10;
    
    // 绘制表头
    int networkTableStartX = (targetRect.width() - networkTotalWidth) / 2;
    if (networkTableStartX < 32) networkTableStartX = 32;
    int networkTableX = networkTableStartX;
    int networkTableY = networkTableStartY;
    
    painter.setPen(QPen(QColor("#5B5B5B"), 1));
    for (int i = 0; i < networkHeaders.size(); i++) {
        painter.setBrush(QColor("#F7F8FA"));
        painter.drawRect(networkTableX, networkTableY, networkColWidths[i], networkRowHeight);
        painter.setPen(QColor("#000000"));
        painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[i] - 10, networkRowHeight),
                        Qt::AlignCenter | Qt::AlignVCenter, networkHeaders[i]);
        painter.setPen(QPen(QColor("#5B5B5B"), 1));
        networkTableX += networkColWidths[i];
    }
    networkTableY += networkRowHeight;
    
    // 绘制脑网络数据
    if (m_brainRegionTableModel) {
        int networkRowCount = m_brainRegionTableModel->rowCount();
        int maxNetworkRowsPerPage = (targetRect.height() - networkTableY - 50) / networkRowHeight;
        int currentNetworkRow = 0;

        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));

        for (int row = 0; row < networkRowCount; row++) {
            if (currentNetworkRow >= maxNetworkRowsPerPage) {
                // 绘制当前页页码
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
                painter.setPen(QColor("#C9CDD4"));
                QString pageNumber = QString("%1/13").arg(currentPageNumber);
                QFontMetrics pageNumberFm = painter.fontMetrics();
                int pageNumberWidth = pageNumberFm.horizontalAdvance(pageNumber);
                int pageNumberX = (targetRect.width() - pageNumberWidth) / 2;
                int pageNumberY = targetRect.height() - 20;
                painter.drawText(pageNumberX, pageNumberY, pageNumber);

                // 换页
                writer.newPage();
                currentPageNumber++;

                // 绘制页面顶部标题
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
                painter.setPen(QColor("#C9CDD4"));
                QString pageTopName = QStringLiteral("脑测量分析报告");
                QFontMetrics pageTopNameFm = painter.fontMetrics();
                int pageTopNameWidth = pageTopNameFm.horizontalAdvance(pageTopName);
                int pageTopNameX = (targetRect.width() - pageTopNameWidth) / 2;
                painter.drawText(pageTopNameX, 50, pageTopName);

                // 绘制title7图片
                QRect targetRectTitle7NewPage(0, 80, title7.width() / 2, title7Height);
                painter.drawImage(targetRectTitle7NewPage, title7);

                networkTableY = 80 + title7Height + 20;
                currentNetworkRow = 0;

                // 重新绘制表头
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Medium));
                networkTableX = networkTableStartX;
                painter.setPen(QPen(QColor("#5B5B5B"), 1));
                for (int i = 0; i < networkHeaders.size(); i++) {
                    painter.setBrush(QColor("#F7F8FA"));
                    painter.drawRect(networkTableX, networkTableY, networkColWidths[i], networkRowHeight);
                    painter.setPen(QColor("#000000"));
                    painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[i] - 10, networkRowHeight),
                        Qt::AlignCenter | Qt::AlignVCenter, networkHeaders[i]);
                    painter.setPen(QPen(QColor("#5B5B5B"), 1));
                    networkTableX += networkColWidths[i];
                }
                networkTableY += networkRowHeight;

                // 换页后重新计算最大行数
                maxNetworkRowsPerPage = (targetRect.height() - networkTableY - 50) / networkRowHeight;
                painter.setFont(QFont("Alibaba PuHuiTi 3.0", 10, QFont::Normal));
            }

            networkTableX = networkTableStartX;
            QModelIndex idx = m_brainRegionTableModel->index(row, 0);

            // 数据行背景（交替颜色）
            painter.setBrush(row % 2 == 0 ? QColor("#FFFFFF") : QColor("#F7F8FA"));
            painter.setPen(QPen(QColor("#5B5B5B"), 1));

            // 序号
            painter.drawRect(networkTableX, networkTableY, networkColWidths[0], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[0] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, QString::number(row + 1));
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            networkTableX += networkColWidths[0];

            // 中文名称
            QString chName = m_brainRegionTableModel->data(idx, BrainRegionTableModel::ChineseNameRole).toString();
            painter.drawRect(networkTableX, networkTableY, networkColWidths[1], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[1] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, chName);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            networkTableX += networkColWidths[1];

            // 英文名称
            QString enName = m_brainRegionTableModel->data(idx, BrainRegionTableModel::EnglishNameRole).toString();
            painter.drawRect(networkTableX, networkTableY, networkColWidths[2], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[2] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, enName);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            networkTableX += networkColWidths[2];

            // 度
            QString degree = m_brainRegionTableModel->data(idx, BrainRegionTableModel::DegreeRole).toString();
            painter.drawRect(networkTableX, networkTableY, networkColWidths[3], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[3] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, degree);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            networkTableX += networkColWidths[3];

            // 聚类系数
            QString clustering = m_brainRegionTableModel->data(idx, BrainRegionTableModel::ClusteringRole).toString();
            painter.drawRect(networkTableX, networkTableY, networkColWidths[4], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[4] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, clustering);
            painter.setPen(QPen(QColor("#5B5B5B"), 1));
            networkTableX += networkColWidths[4];

            // 局部效率
            QString localEff = m_brainRegionTableModel->data(idx, BrainRegionTableModel::LocalEfficiencyRole).toString();
            painter.drawRect(networkTableX, networkTableY, networkColWidths[5], networkRowHeight);
            painter.setPen(QColor("#1D2129"));
            painter.drawText(QRect(networkTableX + 5, networkTableY, networkColWidths[5] - 10, networkRowHeight),
                Qt::AlignCenter | Qt::AlignVCenter, localEff);

            networkTableY += networkRowHeight;
            currentNetworkRow++;
        }
    }
    
    // ========== 绘制title8图片 ==========
    QImage title8(":/image/pdf-title8.png");
    int title8Spacing = 20;
    int title8Height = title8.height() / 2;
    int title8Y = networkTableY + title8Spacing; // 脑网络表格下方20px
    
    // 判断是否需要换页显示title8
    int requiredSpaceForTitle8 = title8Height + 40 + 30 + 50; // title8高度 + 文字背景高度(40px) + 备注高度(30px) + 页码空间
    if (title8Y + requiredSpaceForTitle8 > targetRect.height() - 50) {
        // 空间不足，绘制当前页页码并换页
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString pageNumber = QString("%1/13").arg(currentPageNumber);
        QFontMetrics pageNumberFm = painter.fontMetrics();
        int pageNumberWidth = pageNumberFm.horizontalAdvance(pageNumber);
        int pageNumberX = (targetRect.width() - pageNumberWidth) / 2;
        int pageNumberY = targetRect.height() - 20;
        painter.drawText(pageNumberX, pageNumberY, pageNumber);
        
        // 换页
        writer.newPage();
        currentPageNumber++;
        
        // 绘制新页面顶部标题
        painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
        painter.setPen(QColor("#C9CDD4"));
        QString newPageTopName = QStringLiteral("脑测量分析报告");
        QFontMetrics newPageTopNameFm = painter.fontMetrics();
        int newPageTopNameWidth = newPageTopNameFm.horizontalAdvance(newPageTopName);
        int newPageTopNameX = (targetRect.width() - newPageTopNameWidth) / 2;
        painter.drawText(newPageTopNameX, 50, newPageTopName);
        
        title8Y = 80; // 从新页面顶部开始
    }
    
    // 绘制title8图片
    QRect targetRectTitle8(0, title8Y, title8.width() / 2, title8Height);
    painter.drawImage(targetRectTitle8, title8);
    
    // ========== 绘制脑龄预测文字背景 ==========
    int textBgY = title8Y + title8Height + 20; // title8下方20px
    int textBgWidth = 698;
    int textBgHeight = 40;
    int textBgRadius = 8; // 圆角半径
    int textBgX = (targetRect.width() - textBgWidth) / 2; // 背景居中
    
    // 绘制圆角背景矩形
    QPainterPath textBgPath;
    textBgPath.addRoundedRect(textBgX, textBgY, textBgWidth, textBgHeight, textBgRadius, textBgRadius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#F2F3F5"));
    painter.drawPath(textBgPath);
    
    // 准备文字内容
    QString normalText = QStringLiteral("根据AI分析测算，该组数据脑龄预测为：");
    QString predictedAge = QString::number(getpredictedBrainAge(), 'f', 0); // 保留0位小数（整数）
    
    // 计算文字位置（靠左显示）
    QFont brainAgeNormalFont("Alibaba PuHuiTi 3.0", 12, QFont::Normal); // 普通文字12px
    QFont brainAgeBoldFont("Alibaba PuHuiTi 3.0", 18, QFont::Bold); // 年龄字体18px
    
    QFontMetrics brainAgeNormalFm(brainAgeNormalFont);
    QFontMetrics brainAgeBoldFm(brainAgeBoldFont);
    
    int normalTextWidth = brainAgeNormalFm.horizontalAdvance(normalText);
    int textLeftMargin = 20; // 左边距
    int textStartX = textBgX + textLeftMargin; // 靠左对齐
    
    // 计算普通文字的Y坐标（在背景中垂直居中，向上偏移3px）
    int normalTextY = textBgY + (textBgHeight + brainAgeNormalFm.ascent()) / 2 - 2;
    
    // 计算蓝色数字的Y坐标（在背景中垂直居中，向上偏移3px）
    int boldTextY = textBgY + (textBgHeight + brainAgeBoldFm.ascent()) / 2 - 4;
    
    // 绘制普通文字
    painter.setFont(brainAgeNormalFont);
    painter.setPen(QColor("#1D2129"));
    painter.drawText(textStartX, normalTextY, normalText);
    
    // 绘制预测数字（加粗，蓝色，18px字体，垂直居中）
    painter.setFont(brainAgeBoldFont);
    painter.setPen(QColor("#165DFF"));
    painter.drawText(textStartX + normalTextWidth, boldTextY, predictedAge);
    
    // 绘制备注文字（背景下方，左对齐）
    QString remarkText = QStringLiteral("备注: 本报告非医疗诊断文书，任何医疗行为请遵从医嘱。");
    QFont remarkFont("Alibaba PuHuiTi 3.0", 8, QFont::Normal);
    painter.setFont(remarkFont);
    painter.setPen(QColor("#86909C"));
    int remarkY = textBgY + textBgHeight + 20; // 背景下方20px
    painter.drawText(textBgX, remarkY, remarkText);
    
    // 绘制最后一页的页码
    painter.setFont(QFont("Alibaba PuHuiTi 3.0", 9, QFont::Normal));
    painter.setPen(QColor("#C9CDD4"));
    QString lastPageNumber = QString("%1/13").arg(currentPageNumber);
    QFontMetrics lastPageNumberFm = painter.fontMetrics();
    int lastPageNumberWidth = lastPageNumberFm.horizontalAdvance(lastPageNumber);
    int lastPageNumberX = (targetRect.width() - lastPageNumberWidth) / 2;
    int lastPageNumberY = targetRect.height() - 20;
    painter.drawText(lastPageNumberX, lastPageNumberY, lastPageNumber);
    
    painter.end();
    qDebug() << QStringLiteral("PDF 报告生成成功: ") << pdfPath;
}

void MainViewController::stopDeepprepProcess()
{
    if (m_dockerPrepRunner && m_dockerPrepRunner->isRunning()) {
        m_dockerPrepRunner->stop();
    }
    stopPrepLogTimer();
}


bool MainViewController::isDeepprepOutput(const QString& outputPath)
{
    if (outputPath.isEmpty()) {
        return false;
    }
    
    QString path = outputPath;
    if (path.startsWith("file:///")) {
        path = path.mid(8);
    }
    
    // 检查DeepPrep特有的目录结构
    QDir outputDir(path);
    
    // DeepPrep有QC、BOLD、Recon这三个主要文件夹
    bool hasQC = outputDir.exists("QC/sub-01/figures");
    bool hasBOLD = outputDir.exists("BOLD/sub-01/func");
    bool hasRecon = outputDir.exists("Recon/fsaverage/mri");
    
    // 如果至少有两个特征目录存在，就认为是DeepPrep输出
    int score = (hasQC ? 1 : 0) + (hasBOLD ? 1 : 0) + (hasRecon ? 1 : 0);
    bool isDeepPrep = score >= 2;
    
    return isDeepPrep;
}

void MainViewController::updateAnnotationText(int orientation, int index, const QString& text)
{
    // 获取对应方向的interactor style
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->UpdateAnnotationText(index, text.toStdString());
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::deleteAnnotation(int orientation, int index)
{
    // 获取对应方向的interactor style
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->DeleteAnnotation(index);
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::updateCircleAnnotationText(int orientation, int index, const QString& text)
{ 
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->UpdateCircleAnnotationText(index, text.toStdString());
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::deleteCircleAnnotation(int orientation, int index)
{   
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->DeleteCircleAnnotation(index);
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::updatePenAnnotationText(int orientation, int index, const QString& text)
{  
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->UpdatePenAnnotationText(index, text.toStdString());
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::deletePenAnnotation(int orientation, int index)
{  
    SliceInteractorStyle* style = SliceVtkItemBase::GetInteractorStyle(static_cast<SliceOrientation>(orientation));
    if (style) {
        style->DeletePenAnnotation(index);
    }
    emit GET_SINGLETON(DicomDataModel)->segRefreshRenderer();
}

void MainViewController::captureViewScreenshot(int viewType, const QString& filePath)
{
    // 通过DicomDataModel发送截图信号，由各个视图在渲染线程中处理
    auto dicomModel = GET_SINGLETON(DicomDataModel);
    if (dicomModel) {
        emit dicomModel->screenshotRequested(viewType, filePath);
    }
}

void MainViewController::scanFolder(const QString& inputDir)
{
    if (!m_mriScanner) {
        return;
    }
    m_mriScanner->startScan(inputDir);
}

void MainViewController::startPreAnalysis(int method, const QString& bidsPath, const QString& outputPath, const QString& licenseFile)
{
    // 获取选中的 MRI 配对结果
    QList<MriPairResult> checkedResults = m_mriPairResultModel->getCheckedResults();
    
    if (checkedResults.isEmpty()) {
        qWarning() << "No MRI pairs selected for analysis";
        return;
    }

    // 保存当前正在处理的配对信息（用于成功后写元数据）
    m_currentProcessingPairs = checkedResults;

    // 1. 生成带时间戳的输出路径
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString finalOutputPath = outputPath;
    if (finalOutputPath.endsWith("/") || finalOutputPath.endsWith("\\")) {
        finalOutputPath.chop(1);
    }
    finalOutputPath += "_" + timestamp;

    // 2. 判断目录状态
    QDir dir(finalOutputPath);
    if (dir.exists()) {
        QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!entries.isEmpty()) {
            appendPreAnalysisLog(QStringLiteral("Warning: 输出目录已存在且不为空：%1\n").arg(finalOutputPath));
        }
    }
    
    qDebug() << "Starting pre-analysis with" << checkedResults.size() << "selected pairs";
    qDebug() << "Method:" << (method == 0 ? "fmriprep" : "deepprep");
    qDebug() << "BIDS Path:" << bidsPath;
    qDebug() << "Output Path:" << finalOutputPath;
    qDebug() << "License File:" << licenseFile;
    
    // 保存参数，用于BIDS转换完成后启动fmriprep/deepprep
    m_preAnalysisMethod = method;
    m_preAnalysisBidsPath = bidsPath;
    m_preAnalysisOutputPath = finalOutputPath; // 使用带时间戳的路径
    m_preAnalysisLicenseFile = licenseFile;
    
    // 清空统一日志，准备显示
    clearPreAnalysisLog();
    setisPreAnalysisRunning(true);
    
    // 添加开始日志
    QString methodName = (method == 0) ? "fmriprep" : "deepprep";
    appendPreAnalysisLog(QStringLiteral("========== 开始预处理 ==========\n"));
    appendPreAnalysisLog(QStringLiteral("方法: %1\n").arg(methodName));
    appendPreAnalysisLog(QStringLiteral("BIDS 目录: %1\n").arg(bidsPath));
    appendPreAnalysisLog(QStringLiteral("输出目录: %1\n").arg(finalOutputPath));
    appendPreAnalysisLog(QStringLiteral("License 文件: %1\n\n").arg(licenseFile));
    appendPreAnalysisLog(QStringLiteral(">>> 开始 BIDS 转换...\n"));
    
    // 设置 BIDS 转换器参数
    m_bidsConverter->setOutputDirectory(bidsPath);
    m_bidsConverter->setDatasetName("BrainMRI_Study");
    m_bidsConverter->setTaskName("rest");
    
    // 启动转换，传入选中的结果
    m_bidsConverter->startConversion(checkedResults);
}

void MainViewController::onScanProgressUpdated(const ScanProgress& progress) {
    setisScanning(true);
    setscanTotalFolders(progress.totalFolders);
    setscanScannedFolders(progress.scannedFolders);
    setscanFoundT1Count(progress.foundT1Count);
    setscanFoundBoldCount(progress.foundBoldCount);
    setscanPairedCount(progress.pairedCount);
    setscanProgress(progress.percentage());
    setscanCurrentFolder(progress.currentFolder);
}

void MainViewController::onScanFinished(const QList<MriPairResult>& results)
{
    setisScanning(false);
    setscanProgress(1.0);
    
    // 加载结果到表格模型
    m_mriPairResultModel->loadResults(results);
    
    qDebug() << "Scan finished with" << results.size() << "paired results";
}

void MainViewController::onConverterProgressUpdated(const BidsConversionProgress& progress)
{
}

void MainViewController::onConversionFinished(const QList<BidsSubjectResult>& results)
{
    qDebug() << "BIDS conversion finished with" << results.size() << "subjects";
    
    appendPreAnalysisLog(QStringLiteral(">>> BIDS 转换完成，共 %1 个被试\n\n").arg(results.size()));
    
    // 根据method值启动对应的预处理程序
    if (m_preAnalysisMethod == 0) {
        appendPreAnalysisLog(QStringLiteral(">>> 开始运行 fmriprep...\n"));
        startFmriprepAfterBids();
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 开始运行 deepprep...\n"));
        startDeepprepAfterBids();
    }
}

void MainViewController::startFmriprepAfterBids()
{
    // 如果已有进程在跑，先停止
    stopFmriprepProcess();

    // 设置参数
    FmriPrepParams params;
    params.bidsDir = m_preAnalysisBidsPath;
    params.outputDir = m_preAnalysisOutputPath;
    params.licenseFile = m_preAnalysisLicenseFile;
    params.skipBidsValidation = true;
    params.fsNoReconall = false;  // 启用 FreeSurfer reconall
    params.ignoreFieldmaps = true;
    
    // 记录日志文件路径，启动日志轮询
    m_prepLogFilePath = m_preAnalysisOutputPath + "/fmriprep-docker.log";
    m_prepLogReadPos = 0;
    
    qDebug() << "Starting fMRIPrep via DockerPrepRunner";
    appendPreAnalysisLog(QStringLiteral("启动 预处理方法...\n\n"));
    
    startPrepLogTimer(m_prepLogFilePath);
    
    // 使用 DockerPrepRunner 异步运行
    m_dockerPrepRunner->runFmriPrep(params);
}

void MainViewController::startDeepprepAfterBids()
{
    // 如果已有进程在跑，先停止
    stopDeepprepProcess();

    // 设置参数
    DeepPrepParams params;
    params.bidsDir = m_preAnalysisBidsPath;
    params.outputDir = m_preAnalysisOutputPath;
    params.licenseFile = m_preAnalysisLicenseFile;
    params.skipBidsValidation = true;
    params.boldSdc = false;  // 禁用 SDC
    params.device = "auto";
    
    // 记录日志文件路径，启动日志轮询
    m_prepLogFilePath = m_preAnalysisOutputPath + "/deepprep-docker.log";
    m_prepLogReadPos = 0;
    
    qDebug() << "Starting DeepPrep via DockerPrepRunner";
    appendPreAnalysisLog(QStringLiteral("使用 DockerPrepRunner 启动 DeepPrep...\n\n"));
    
    startPrepLogTimer(m_prepLogFilePath);
    
    // 使用 DockerPrepRunner 异步运行
    m_dockerPrepRunner->runDeepPrep(params);
}

void MainViewController::appendPreAnalysisLog(const QString& text)
{
    if (text.isEmpty())
        return;
    m_preAnalysisLog.append(text);
    // 限制日志长度，避免TextArea渲染大量文本导致UI卡顿
    const int maxLogLength = 10000;
    if (m_preAnalysisLog.length() > maxLogLength) {
        // 保留后半部分日志，从换行符处截断以保持完整行
        int cutPos = m_preAnalysisLog.indexOf('\n', m_preAnalysisLog.length() - maxLogLength);
        if (cutPos > 0) {
            m_preAnalysisLog = m_preAnalysisLog.mid(cutPos + 1);
        } else {
            m_preAnalysisLog = m_preAnalysisLog.right(maxLogLength);
        }
    }
    // 使用节流机制，避免频繁触发UI更新
    if (!m_preAnalysisLogUpdateTimer) {
        m_preAnalysisLogUpdateTimer = new QTimer(this);
        m_preAnalysisLogUpdateTimer->setSingleShot(true);
        m_preAnalysisLogUpdateTimer->setInterval(300); // 300ms节流
        connect(m_preAnalysisLogUpdateTimer, &QTimer::timeout, this, [this]() {
            emit preAnalysisLogUpdated();
        });
    }
    if (!m_preAnalysisLogUpdateTimer->isActive()) {
        m_preAnalysisLogUpdateTimer->start();
    }
}

void MainViewController::clearPreAnalysisLog()
{
    m_preAnalysisLog.clear();
    emit preAnalysisLogUpdated();
}
