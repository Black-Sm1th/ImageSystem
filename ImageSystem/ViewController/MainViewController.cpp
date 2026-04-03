#include "MainViewController.h"
#include <algorithm>
#include <vtkTransform.h>
#include <QQuickItemGrabResult>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDirIterator>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QFont>
#include <QPainterPath>
#include <QtConcurrent/QtConcurrent>
#include "Modules/LogManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include "Modules/BrainNetworkData.h"
#include "Model/DicomDataModel.h"
#include "Modules/SliceVtkItemBase.h"
#include "Modules/BidsConverter.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

QString managedStorageRoot()
{
    return QStringLiteral("D:/AetherDesk");
}

QString managedInputRoot()
{
    return QDir(managedStorageRoot()).filePath(QStringLiteral("InputData"));
}

QString managedOutputRoot()
{
    return QDir(managedStorageRoot()).filePath(QStringLiteral("OutputResults"));
}

QString normalizedPathOrEmpty(const QString& path)
{
    return QDir::fromNativeSeparators(path.trimmed());
}

void ensureDirectoryExists(const QString& path)
{
    if (!path.trimmed().isEmpty())
        QDir().mkpath(path);
}

QDate parseDicomDate(const QString& rawDate)
{
    const QString text = rawDate.trimmed();
    if (text.isEmpty())
        return QDate();

    QDate date = QDate::fromString(text, "yyyyMMdd");
    if (!date.isValid())
        date = QDate::fromString(text, "yyyy-MM-dd");
    if (!date.isValid())
        date = QDate::fromString(text, Qt::ISODate);
    return date;
}

int calculateAgeFromDates(const QString& birthDateText, const QString& examDateText)
{
    const QDate birthDate = parseDicomDate(birthDateText);
    const QDate examDate = parseDicomDate(examDateText);
    if (!birthDate.isValid() || !examDate.isValid() || examDate < birthDate)
        return 0;

    int age = examDate.year() - birthDate.year();
    const QDate birthdayThisYear(examDate.year(), birthDate.month(), birthDate.day());
    if (birthdayThisYear.isValid() && examDate < birthdayThisYear)
        --age;
    return qMax(age, 0);
}

QString buildCompletedCaseSeriesUid(const MriPairResult& pair, int method)
{
    Q_UNUSED(method);

    const QString seriesUid = pair.primarySeriesUid().trimmed();
    if (!seriesUid.isEmpty())
        return seriesUid;

    const QString nameKey = pair.patientName.trimmed().replace('^', '_').replace(' ', '_');
    const QString patientKey = !pair.patientId.trimmed().isEmpty() ? pair.patientId.trimmed()
                                                                   : nameKey;
    const QString studyDate = !pair.studyDate.trimmed().isEmpty() ? pair.studyDate.trimmed()
                                                                  : QStringLiteral("nodate");
    return QStringLiteral("fallback_%1_%2").arg(patientKey, studyDate);
}

QString preprocessMethodLabel(int method)
{
    return method == 0 ? QStringLiteral("fMRIPrep")
                       : QStringLiteral("DeepPrep");
}

QString processingModeLabel(bool runBrainAge, bool runPreprocessing)
{
    if (runBrainAge && runPreprocessing)
        return QStringLiteral("全流程");
    if (runBrainAge)
        return QStringLiteral("仅脑龄预测");
    if (runPreprocessing)
        return QStringLiteral("仅预处理");
    return QStringLiteral("未知模式");
}

QString normalizeDisplayedSex(const QString& rawSex)
{
    const QString trimmed = rawSex.trimmed();
    const QString sex = trimmed.toUpper();
    if (sex == QStringLiteral("M") || sex == QStringLiteral("MALE") || trimmed == QStringLiteral("男"))
        return QStringLiteral("男");
    if (sex == QStringLiteral("F") || sex == QStringLiteral("FEMALE") || trimmed == QStringLiteral("女"))
        return QStringLiteral("女");
    if (trimmed == QStringLiteral("男女"))
        return {};
    return trimmed;
}

QString buildCaseIdentityKey(const QString& name, const QString& examDate, const QString& seriesUid)
{
    const QString uid = seriesUid.trimmed();
    if (!uid.isEmpty())
        return QStringLiteral("uid:%1").arg(uid);

    const QDate parsedDate = parseDicomDate(examDate);
    const QString normalizedName = name.trimmed().replace('^', ' ').simplified().toLower();
    const QString normalizedDate = parsedDate.isValid()
            ? parsedDate.toString(QStringLiteral("yyyyMMdd"))
            : examDate.trimmed();
    return QStringLiteral("name:%1|date:%2").arg(normalizedName, normalizedDate);
}

QString buildCaseIdentityKey(const MriPairResult& pair)
{
    return buildCaseIdentityKey(pair.patientName, pair.studyDate, pair.primarySeriesUid());
}

QString stripKnownFileExtensions(const QString& value)
{
    QString text = value.trimmed();
    if (text.endsWith(QStringLiteral(".nii.gz"), Qt::CaseInsensitive))
        text.chop(7);
    else if (text.endsWith(QStringLiteral(".nii"), Qt::CaseInsensitive)
             || text.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)
             || text.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)) {
        const int dotPos = text.lastIndexOf('.');
        if (dotPos > 0)
            text = text.left(dotPos);
    }
    return text;
}

QStringList brainAgeIdVariants(const QString& rawId)
{
    QStringList variants;
    const QString normalized = QDir::fromNativeSeparators(rawId.trimmed());
    if (normalized.isEmpty())
        return variants;

    const auto addVariant = [&variants](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !variants.contains(trimmed))
            variants.append(trimmed);
    };

    addVariant(normalized);

    const QFileInfo info(normalized);
    addVariant(info.fileName());

    QString base = stripKnownFileExtensions(info.fileName());
    addVariant(base);

    const QStringList suffixes = {
        QStringLiteral("_defaced"),
        QStringLiteral("_T1w"),
        QStringLiteral("_T1w_defaced"),
        QStringLiteral("_task-rest_bold"),
        QStringLiteral("_bold")
    };
    for (const QString& suffix : suffixes) {
        if (base.endsWith(suffix, Qt::CaseInsensitive)) {
            addVariant(base.left(base.size() - suffix.size()));
        }
    }

    const QStringList segments = normalized.split('/', Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        if (segment.startsWith(QStringLiteral("sub-"), Qt::CaseInsensitive))
            addVariant(segment);
    }

    return variants;
}

double resolvePredictedBrainAgeForPair(const MriPairResult& pair,
                                       const QMap<QString, double>& brainAgePredictions)
{
    const QStringList lookupKeys = brainAgeIdVariants(pair.subjectId) + brainAgeIdVariants(pair.patientId);
    for (const QString& key : lookupKeys) {
        auto it = brainAgePredictions.constFind(key);
        if (it != brainAgePredictions.constEnd())
            return it.value();
    }
    return -1.0;
}

QString findBrainAgePredictionCsvPath(const QString& basePath)
{
    const QDir dir(basePath);
    const QStringList candidates = {
        QStringLiteral("brain_age_predictions.csv"),
        QStringLiteral("BatchPrediction.csv")
    };
    for (const QString& fileName : candidates) {
        const QString path = dir.filePath(fileName);
        if (QFile::exists(path))
            return path;
    }
    return {};
}

QVariantList discoverSubjectsFromOutputDirectory(const QString& outputDir)
{
    const QDir baseDir(outputDir);
    const QStringList candidateRoots = {
        QStringLiteral("Recon"),
        QStringLiteral("sourcedata/freesurfer"),
        QStringLiteral("BOLD"),
        QString()
    };

    QSet<QString> seenSubjects;
    QVariantList subjects;

    for (const QString& root : candidateRoots) {
        const QDir dir(root.isEmpty() ? baseDir.filePath(QStringLiteral(".")) : baseDir.filePath(root));
        if (!dir.exists())
            continue;

        const QStringList subDirs = dir.entryList(QStringList() << QStringLiteral("sub-*"),
                                                  QDir::Dirs | QDir::NoDotAndDotDot,
                                                  QDir::Name);
        for (const QString& subDir : subDirs) {
            if (seenSubjects.contains(subDir))
                continue;
            seenSubjects.insert(subDir);

            QVariantMap subject;
            subject.insert(QStringLiteral("subjectId"), subDir);
            subject.insert(QStringLiteral("patientName"), subDir);
            subject.insert(QStringLiteral("patientId"), subDir);
            subject.insert(QStringLiteral("patientSex"), QString());
            subject.insert(QStringLiteral("patientBirthDate"), QString());
            subject.insert(QStringLiteral("studyDate"), QString());
            subjects.append(subject);
        }
    }

    return subjects;
}

bool outputDirectoryContainsSubject(const QString& outputDir, const QString& subjectId)
{
    const QString normalizedSubjectId = subjectId.trimmed();
    if (normalizedSubjectId.isEmpty())
        return false;

    const QStringList candidatePaths = {
        QDir(outputDir).filePath(QStringLiteral("Recon/%1").arg(normalizedSubjectId)),
        QDir(outputDir).filePath(QStringLiteral("BOLD/%1").arg(normalizedSubjectId)),
        QDir(outputDir).filePath(QStringLiteral("sourcedata/freesurfer/%1").arg(normalizedSubjectId)),
        QDir(outputDir).filePath(QStringLiteral("outputDir/%1").arg(normalizedSubjectId)),
        QDir(outputDir).filePath(QStringLiteral("brain_regions/%1").arg(normalizedSubjectId)),
        QDir(outputDir).filePath(QStringLiteral("QC/%1").arg(normalizedSubjectId))
    };

    for (const QString& path : candidatePaths) {
        if (QFileInfo::exists(path))
            return true;
    }

    return false;
}

} // namespace

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
    QObject::connect(m_mriScanner, &BatchMriScanner::scanLog, this, [this](const QString& message) {
        appendPreAnalysisLog(message);
    });
    QObject::connect(m_mriScanner, &BatchMriScanner::scanError, this, [this](const QString& error) {
        appendPreAnalysisLog(QStringLiteral("扫描错误：%1\n").arg(error));
    });
    QObject::connect(m_bidsConverter, &BidsConverter::progressUpdated, this, &MainViewController::onConverterProgressUpdated);
    QObject::connect(m_bidsConverter, &BidsConverter::conversionFinished, this, &MainViewController::onConversionFinished);
    
    // 初始化 Docker 预处理运行器
    setupDockerPrepRunner();

    // 初始化数据库
    m_dbManager = new DatabaseManager();
    
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, [this]() {
                    stopFmriprepProcess();
                    stopDeepprepProcess();
                    if (m_dockerPrepRunner) {
                        m_dockerPrepRunner->stopDeface();
                        m_dockerPrepRunner->stopBap();
                    }
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

    connect(m_dockerPrepRunner, &DockerPrepRunner::fmriPrepFinished, this, [this](int exitCode, const QString& message) {
        stopPrepLogTimer();
        if (exitCode == 0) {
            appendPreAnalysisLog(QStringLiteral("\n>>> fMRIPrep 运行成功！\n"));
        } else {
            appendPreAnalysisLog(QStringLiteral("\n>>> fMRIPrep 运行失败：%1\n").arg(message));
        }
        onPrepFinished(exitCode == 0, message);
    });

    connect(m_dockerPrepRunner, &DockerPrepRunner::deepPrepFinished, this, [this](int exitCode, const QString& message) {
        stopPrepLogTimer();
        if (exitCode == 0) {
            appendPreAnalysisLog(QStringLiteral("\n>>> DeepPrep 运行成功！\n"));
        } else {
            appendPreAnalysisLog(QStringLiteral("\n>>> DeepPrep 运行失败：%1\n").arg(message));
        }
        onPrepFinished(exitCode == 0, message);
    });

    connect(m_dockerPrepRunner, &DockerPrepRunner::defaceFinished, this, [this](int exitCode, const QString& message) {
        if (exitCode == 0) {
            appendPreAnalysisLog(QStringLiteral("\n>>> Deface 完成！\n"));
            if (!applyDefacedDataToCurrentBids()) {
                appendPreAnalysisLog(QStringLiteral(">>> Deface 结果整理失败，未切换到去脸后的输入数据\n"));
                onDefaceFinished(false, QStringLiteral("failed to replace original T1 with defaced copies"));
                return;
            }
            if (m_currentItem.runPreprocessing) {
                if (m_preAnalysisMethod == 0) {
                    appendPreAnalysisLog(QStringLiteral(">>> Deface 后启动 fMRIPrep...\n"));
                    startFmriprepAfterBids();
                } else {
                    appendPreAnalysisLog(QStringLiteral(">>> Deface 后启动 DeepPrep...\n"));
                    startDeepprepAfterBids();
                }
            } else {
                appendPreAnalysisLog(QStringLiteral(">>> [%1] 跳过预处理（fmriprep/deepprep）\n")
                                     .arg(processingModeLabel(m_currentItem.runBrainAge, m_currentItem.runPreprocessing)));
                m_currentPrepDone = true;
                m_currentPrepSuccess = true;
            }
            if (m_currentItem.runBrainAge) {
                startBapAfterDeface();
            } else {
                appendPreAnalysisLog(QStringLiteral(">>> [%1] 跳过脑龄预测(BAP)\n")
                                     .arg(processingModeLabel(m_currentItem.runBrainAge, m_currentItem.runPreprocessing)));
            }
        } else {
            appendPreAnalysisLog(QStringLiteral("\n>>> Deface 失败：%1\n").arg(message));
            onDefaceFinished(false, message);
        }
    });

    connect(m_dockerPrepRunner, &DockerPrepRunner::bapFinished, this, [this](int exitCode, const QString& message) {
        setbrainAgeProcessing(false);
        if (exitCode == 0) {
            appendPreAnalysisLog(QStringLiteral("\n>>> 脑龄预测(BAP) 完成！\n"));
            if (persistBrainAgePredictionsToOutput() && loadBrainAgePredictions(m_preAnalysisOutputPath))
                appendPreAnalysisLog(QStringLiteral(">>> 已加载脑龄结果表\n"));
            else
                appendPreAnalysisLog(QStringLiteral(">>> 警告：脑龄结果未能保存到输出目录或加载失败\n"));
        } else {
            appendPreAnalysisLog(QStringLiteral("\n>>> 脑龄预测(BAP) 失败：%1\n").arg(message));
        }
        onBrainAgePredictionFinished(exitCode == 0 && QFile::exists(findBrainAgePredictionCsvPath(m_preAnalysisOutputPath)));
    });
}

void MainViewController::setupBrainRegionProcessor()
{
    if (m_brainRegionProcessor) {
        return;
    }
    
    m_brainRegionProcessor = new BrainRegionProcessor(this);
    
    // 设置进度回调（注意：回调在后台线程中执行，需要切换到主线程）
    m_brainRegionProcessor->setProgressCallback([this](int percent, const QString& message) {
        QString logText = QStringLiteral(">>> [脑区处理 %1%] %2\n").arg(percent).arg(message);
        QMetaObject::invokeMethod(this, [this, logText]() {
            appendPreAnalysisLog(logText);
        }, Qt::QueuedConnection);
    });
    
    // 连接单个处理完成信号
    connect(m_brainRegionProcessor, &BrainRegionProcessor::processFinished, this, [this](const ProcessingResult& result) {
        if (result.success) {
            appendPreAnalysisLog(QStringLiteral(">>> 被试处理完成: %1, 共 %2 个脑区, %3 个 STL 文件\n")
                .arg(result.outputDir)
                .arg(result.regionCount)
                .arg(result.stlFileCount));
        } else {
            appendPreAnalysisLog(QStringLiteral(">>> 被试处理失败: %1\n").arg(result.message));
        }
    });
    
    // 连接批量处理进度信号
    connect(m_brainRegionProcessor, &BrainRegionProcessor::batchProgress, this, [this](int current, int total, const QString& subject) {
        appendPreAnalysisLog(QStringLiteral(">>> 批量处理进度: %1/%2 - %3\n").arg(current + 1).arg(total).arg(subject));
    });
    
    // 连接批量处理完成信号
    connect(m_brainRegionProcessor, &BrainRegionProcessor::batchFinished, this, [this](int successCount, int failCount) {
        appendPreAnalysisLog(QStringLiteral("\n>>> 脑区处理完成: 成功 %1 个, 失败 %2 个\n").arg(successCount).arg(failCount));
        onBrainRegionPostProcessingFinished(failCount == 0 && successCount > 0, successCount, failCount);
    });
    
    // 连接错误信号
    connect(m_brainRegionProcessor, &BrainRegionProcessor::processError, this, [this](const QString& error) {
        appendPreAnalysisLog(QStringLiteral(">>> 脑区处理错误: %1\n").arg(error));
    });
}

void MainViewController::startBrainRegionProcessing()
{
    setupBrainRegionProcessor();
    
    // 查找分割结果文件
    // fMRIPrep 输出结构：输出文件夹/sourcedata/freesurfer/sub-XXX/mri/aparc+aseg.mgz
    // DeepPrep 输出结构：输出文件夹/Recon/sub-XXX/mri/aparc+aseg.mgz
    // 都需要先用 mgz2nii Python 模块转换成 nii.gz
    
    QDir outputDir(m_preAnalysisOutputPath);
    if (!outputDir.exists()) {
        appendPreAnalysisLog(QStringLiteral(">>> 错误: 输出目录不存在\n"));
        setisPreAnalysisRunning(false);
        appendPreAnalysisLog(QStringLiteral("\n========== 预处理完成 ==========\n"));
        return;
    }
    
    QString colorTablePath = "Scripts/tsv/desc-aseg_dseg_with_chinese.tsv";
    if (!QFileInfo::exists(colorTablePath)) {
        appendPreAnalysisLog(QStringLiteral(">>> 警告: 未找到颜色表文件，将使用默认颜色\n"));
        colorTablePath = "";
    }
    
    QString basePath;
    if (m_preAnalysisMethod == 0) {
        basePath = outputDir.filePath("sourcedata/freesurfer");
    } else {
        basePath = outputDir.filePath("Recon");
    }
    
    QDir baseDir(basePath);
    if (!baseDir.exists()) {
        appendPreAnalysisLog(QStringLiteral(">>> 错误: 未找到预处理输出目录: %1\n").arg(basePath));
        setisPreAnalysisRunning(false);
        appendPreAnalysisLog(QStringLiteral("\n========== 预处理完成 ==========\n"));
        return;
    }

    QString basePathCopy = basePath;
    QString outputPathCopy = m_preAnalysisOutputPath;

    QtConcurrent::run([=]() {
        QDir bgBaseDir(basePathCopy);
        QStringList subDirs = bgBaseDir.entryList(QStringList() << "sub-*", QDir::Dirs | QDir::NoDotAndDotDot);
        QList<std::tuple<QString, QString, QString>> subjects;

        for (const QString& subDir : subDirs) {
            QString mriPath = bgBaseDir.filePath(subDir + "/mri");
            QString mgzFile = QDir(mriPath).filePath("aparc+aseg.mgz");

            if (!QFileInfo::exists(mgzFile)) {
                QMetaObject::invokeMethod(this, [=]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 警告: 未找到被试 %1 的分割文件，跳过\n").arg(subDir));
                }, Qt::QueuedConnection);
                continue;
            }

            QString niiFile = QDir(mriPath).filePath("aparc+aseg.nii.gz");

            if (!QFileInfo::exists(niiFile)) {
                QMetaObject::invokeMethod(this, [=]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 正在转换 %1 的 aparc+aseg.mgz 文件...\n").arg(subDir));
                }, Qt::QueuedConnection);

                bool convertOk = false;
                try {
                    py::gil_scoped_acquire acquire;
                    py::module_ mgz2nii = py::module_::import("mgz2nii");
                    mgz2nii.attr("mgz_to_nii")(mgzFile.toStdString(), niiFile.toStdString());
                    convertOk = true;
                } catch (const py::error_already_set& e) {
                    QString errMsg = QString::fromUtf8(e.what());
                    QMetaObject::invokeMethod(this, [=]() {
                        appendPreAnalysisLog(QStringLiteral(">>> 警告: 被试 %1 的 aparc+aseg.mgz 转换失败: %2\n")
                            .arg(subDir).arg(errMsg));
                    }, Qt::QueuedConnection);
                    continue;
                }

                if (!convertOk || !QFileInfo::exists(niiFile)) {
                    QMetaObject::invokeMethod(this, [=]() {
                        appendPreAnalysisLog(QStringLiteral(">>> 警告: 被试 %1 转换后的 aparc+aseg.nii.gz 文件不存在，跳过\n").arg(subDir));
                    }, Qt::QueuedConnection);
                    continue;
                }

                QMetaObject::invokeMethod(this, [=]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 转换完成: %1\n").arg(niiFile));
                }, Qt::QueuedConnection);
            }

            QString t1MgzFile = QDir(mriPath).filePath("T1.mgz");
            QString t1NiiFile = QDir(mriPath).filePath("T1.nii.gz");

            if (QFileInfo::exists(t1MgzFile) && !QFileInfo::exists(t1NiiFile)) {
                QMetaObject::invokeMethod(this, [=]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 正在转换 %1 的 T1.mgz 文件...\n").arg(subDir));
                }, Qt::QueuedConnection);

                try {
                    py::gil_scoped_acquire acquire;
                    py::module_ mgz2nii = py::module_::import("mgz2nii");
                    mgz2nii.attr("mgz_to_nii")(t1MgzFile.toStdString(), t1NiiFile.toStdString());

                    if (QFileInfo::exists(t1NiiFile)) {
                        QMetaObject::invokeMethod(this, [=]() {
                            appendPreAnalysisLog(QStringLiteral(">>> T1 转换完成: %1\n").arg(t1NiiFile));
                        }, Qt::QueuedConnection);
                    }
                } catch (const py::error_already_set& e) {
                    QString errMsg = QString::fromUtf8(e.what());
                    QMetaObject::invokeMethod(this, [=]() {
                        appendPreAnalysisLog(QStringLiteral(">>> 警告: 被试 %1 的 T1.mgz 转换失败: %2\n")
                            .arg(subDir).arg(errMsg));
                    }, Qt::QueuedConnection);
                }
            }

            subjects.append(std::make_tuple(niiFile, QString(), subDir));
            QMetaObject::invokeMethod(this, [=]() {
                appendPreAnalysisLog(QStringLiteral(">>> 找到被试: %1\n    分割文件: %2\n").arg(subDir).arg(niiFile));
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this, [=]() {
            if (subjects.isEmpty()) {
                appendPreAnalysisLog(QStringLiteral(">>> 错误: 未找到任何有效的分割文件\n"));
                setisPreAnalysisRunning(false);
                appendPreAnalysisLog(QStringLiteral("\n========== 预处理完成 ==========\n"));
                return;
            }

            appendPreAnalysisLog(QStringLiteral("\n>>> 开始批量处理 %1 个被试的脑区数据...\n").arg(subjects.size()));

            QDir outDir(outputPathCopy);
            QString brainRegionsBaseDir = outDir.filePath("brain_regions");
            m_brainRegionProcessor->processBatchAsync(subjects, colorTablePath, brainRegionsBaseDir);
        }, Qt::QueuedConnection);
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
    QFile existingFile(filePath);
    if (existingFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument existingDoc = QJsonDocument::fromJson(existingFile.readAll());
        if (existingDoc.isObject())
            root = existingDoc.object();
        existingFile.close();
    }

    root["processDate"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    root["method"] = (m_preAnalysisMethod == 0) ? "fmriprep" : "deepprep";

    QJsonArray subjectsArr = root.value(QStringLiteral("subjects")).toArray();
    QMap<QString, int> subjectIndexByKey;
    for (int i = 0; i < subjectsArr.size(); ++i) {
        const QJsonObject sub = subjectsArr.at(i).toObject();
        const QString key = buildCaseIdentityKey(sub.value(QStringLiteral("patientName")).toString(),
                                                 sub.value(QStringLiteral("studyDate")).toString(),
                                                 sub.value(QStringLiteral("primarySeriesUid")).toString());
        if (!key.isEmpty())
            subjectIndexByKey.insert(key, i);
    }

    for (const auto& pair : pairs) {
        QJsonObject sub;
        sub["subjectId"] = pair.subjectId;
        sub["patientName"] = pair.patientName;
        sub["patientId"] = pair.patientId;
        sub["patientSex"] = normalizeDisplayedSex(pair.patientSex);
        sub["patientBirthDate"] = pair.patientBirthDate;
        sub["studyDate"] = pair.studyDate;
        sub["t1SeriesUid"] = pair.t1SeriesUid;
        sub["boldSeriesUid"] = pair.boldSeriesUid;
        sub["primarySeriesUid"] = pair.primarySeriesUid();
        sub["t1SeriesDesc"] = pair.t1SeriesDesc;
        sub["boldSeriesDesc"] = pair.boldSeriesDesc;

        const QString key = buildCaseIdentityKey(pair);
        if (subjectIndexByKey.contains(key)) {
            subjectsArr.replace(subjectIndexByKey.value(key), sub);
        } else {
            subjectIndexByKey.insert(key, subjectsArr.size());
            subjectsArr.append(sub);
        }
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
        const QVariantList discoveredSubjects = discoverSubjectsFromOutputDirectory(outputDir);
        if (!discoveredSubjects.isEmpty()) {
            result.insert(QStringLiteral("subjects"), discoveredSubjects);
            result.insert(QStringLiteral("method"), isDeepprepOutput(outputDir) ? QStringLiteral("deepprep")
                                                                               : QStringLiteral("fmriprep"));
        }
        return result;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        result = doc.toVariant().toMap();
    }

    QVariantList subjects = result.value(QStringLiteral("subjects")).toList();
    const QVariantList discoveredSubjects = discoverSubjectsFromOutputDirectory(outputDir);

    bool metadataSubjectsMismatch = false;
    for (const QVariant& subjectValue : subjects) {
        const QVariantMap subject = subjectValue.toMap();
        const QString subjectId = subject.value(QStringLiteral("subjectId")).toString();
        if (!outputDirectoryContainsSubject(outputDir, subjectId)) {
            metadataSubjectsMismatch = true;
            qWarning() << "Metadata subject does not exist in output directory, fallback to discovered subjects:"
                       << subjectId << outputDir;
            break;
        }
    }

    if (subjects.isEmpty() || metadataSubjectsMismatch) {
        if (!discoveredSubjects.isEmpty()) {
            result.insert(QStringLiteral("subjects"), discoveredSubjects);
            if (metadataSubjectsMismatch) {
                result.insert(QStringLiteral("metadataRecovered"), true);
            }
        }
    }
    
    return result;
}

void MainViewController::calculateKidney() {
    if (gett2() == -1 || getskin() == -1 || getmicro() == -1 ||
        getsei() == -1 || getader() == -1 || getdisp() == -1) {
        qWarning() << QStringLiteral("部分参数未设置，无法计算");
        return;
    }

    int T2 = gett2(), skin = getskin(), micro = getmicro();
    int SEI = getsei(), ADER = getader(), dispersion = getdisp();

    qDebug() << QStringLiteral("启动肾脏计算 (pybind11), 参数:") << T2 << skin << micro << SEI << ADER << dispersion;

    QtConcurrent::run([=]() {
        double ccls_value = 0.0;
        double ccrcc_value = 0.0;
        bool success = false;

        try {
            py::gil_scoped_acquire acquire;
            py::module_ kidney = py::module_::import("kidney_processor");

            py::object ccls_result = kidney.attr("calculate_CCLS")(T2, skin, micro, SEI, ADER, dispersion);
            ccls_value = ccls_result.cast<double>();
            qDebug() << QStringLiteral("CCLS结果:") << ccls_value;

            py::object ccrcc_result = kidney.attr("calculate_CCRCC")(T2, skin, micro, SEI, ADER, dispersion, ccls_value);
            std::string ccrcc_str = ccrcc_result.cast<std::string>();
            ccrcc_value = std::stod(ccrcc_str);
            qDebug() << QStringLiteral("CCRCC结果:") << ccrcc_value;
            success = true;
        } catch (const py::error_already_set& e) {
            qWarning() << QStringLiteral("Python 计算错误:") << e.what();
        } catch (const std::exception& e) {
            qWarning() << QStringLiteral("计算错误:") << e.what();
        }

        if (success) {
            QMetaObject::invokeMethod(this, [=]() {
                setcclsResult(ccls_value);
                setccrccResult(ccrcc_value);
            }, Qt::QueuedConnection);
        }
    });
}

bool MainViewController::loadBrainAgePredictions(const QString& basePath)
{
    QString dirPath = basePath;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }
    const QString p = QDir::fromNativeSeparators(dirPath);
    const QString csvPath = findBrainAgePredictionCsvPath(p);

    m_brainAgePredictions.clear();
    m_currentBrainAgeDataPath.clear();

    QFile file(csvPath);
    if (!file.exists()) {
        qDebug() << QStringLiteral("脑龄预测 CSV 不存在: brain_age_predictions.csv / BatchPrediction.csv 于") << p;
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << QStringLiteral("无法打开脑龄预测文件:") << csvPath;
        return false;
    }

    m_currentBrainAgeDataPath = p;

    QTextStream in(&file);
    bool isHeader = true;
    int idColumn = -1;
    int ageColumn = -1;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(',');
        if (isHeader) {
            for (int i = 0; i < fields.size(); ++i) {
                QString header = fields[i].trimmed().toLower();
                if (header == "id" || header == "subject" || header == "participant_id")
                    idColumn = i;
                else if (header == "pred_age" || header == "predicted_age" || header == "prediction")
                    ageColumn = i;
            }
            isHeader = false;
            if (idColumn < 0 || ageColumn < 0) {
                qDebug() << QStringLiteral("脑龄 CSV 表头需含 subject/id 与 predicted_age/pred_age");
                file.close();
                return false;
            }
            continue;
        }

        if (fields.size() > idColumn && fields.size() > ageColumn) {
            QString id = fields[idColumn].trimmed();
            bool ok = false;
            double age = fields[ageColumn].trimmed().toDouble(&ok);
            if (ok && !id.isEmpty()) {
                const QStringList variants = brainAgeIdVariants(id);
                for (const QString& variant : variants) {
                    m_brainAgePredictions[variant] = age;
                }
                qDebug() << QStringLiteral("加载脑龄预测: %1 -> %2（键数=%3）")
                            .arg(id)
                            .arg(age)
                            .arg(variants.size());
            }
        }
    }

    file.close();
    qDebug() << QStringLiteral("共加载 %1 条脑龄预测数据").arg(m_brainAgePredictions.size());
    return !m_brainAgePredictions.isEmpty();
}

void MainViewController::importBrainData(const QString& url, const QString& subjectId, const QString& patientId)
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
    
    // 使用传入的 subjectId，默认为 "sub-01"
    QString subId = subjectId.isEmpty() ? "sub-01" : subjectId;
    qDebug() << QStringLiteral("导入脑数据，被试ID:") << subId;
    
    QDir baseDir(dirPath);
    if (!baseDir.exists()) {
        qDebug() << QStringLiteral("路径不存在: ") << dirPath;
        emit brainAnalysisFinished(false);
        return;
    }

    if (m_currentBrainAgeDataPath != dirPath) {
        loadBrainAgePredictions(dirPath);
    }
    
    setpredictedBrainAge(0.0);

    // 优先按 subjectId，再按 patientId 匹配脑龄结果
    QString matchedKey;
    const QStringList lookupKeys = brainAgeIdVariants(subId) + brainAgeIdVariants(patientId);
    for (const QString& key : lookupKeys) {
        if (!m_brainAgePredictions.contains(key))
            continue;
        matchedKey = key;
        break;
    }

    if (!matchedKey.isEmpty()) {
        const double predictedAge = m_brainAgePredictions.value(matchedKey);
        setpredictedBrainAge(predictedAge);
        qDebug() << QStringLiteral("设置脑龄预测值: %1 -> %2").arg(matchedKey).arg(predictedAge);
    } else {
        qDebug() << QStringLiteral("未找到被试 %1 / %2 的脑龄预测数据").arg(subId, patientId);
    }
    
    // ========== 逻辑一：检查是否存在完整的输出结果 ==========
    QDir outputDir(baseDir.filePath("outputDir/" + subId));
    if (outputDir.exists()) {
        bool hasAllFiles = true;
        
        // 检查必需的文件
        QStringList requiredFiles = {
            "alff.png",
            "alff_transparent.png",
            "brain_network_results.json",
            "covariance.png",
            "covariance_transparent.png",
            "viewConnectome.html",
            "viewConnectome.png"
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
                
                if (imageFiles.count() == 117
                        && QFile::exists(regionPlotsDir.filePath("001_Precentral_L_transparent.png"))) {
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
    // 首先检查fMRIPrep格式，使用动态被试ID
    QString boldPath = baseDir.filePath(subId + "/func/" + subId + "_task-rest_space-MNI152NLin2009cAsym_res-2_desc-preproc_bold.nii.gz");
    QString confoundsPath = baseDir.filePath(subId + "/func/" + subId + "_task-rest_desc-confounds_timeseries.tsv");
    qDebug() << QFile::exists(boldPath) << QFile::exists(confoundsPath);
    // 如果fMRIPrep格式不存在，检查DeepPrep格式
    if (!QFile::exists(boldPath) || !QFile::exists(confoundsPath)) {
        boldPath = baseDir.filePath("BOLD/" + subId + "/func/" + subId + "_task-rest_space-MNI152NLin6Asym_res-02_desc-preproc_bold.nii.gz");
        confoundsPath = baseDir.filePath("BOLD/" + subId + "/func/" + subId + "_task-rest_desc-confounds_timeseries.tsv");
        if (QFile::exists(boldPath) && QFile::exists(confoundsPath)) {
            qDebug() << QStringLiteral("检测到DeepPrep格式的脑功能数据文件!!!");
        }
    }
    
    if (QFile::exists(boldPath) && QFile::exists(confoundsPath)) {
        // ========== 符合逻辑二：原始数据文件存在，需要处理 ==========
        qDebug() << QStringLiteral("检测到原始脑功能数据文件!!!");
        // 创建输出目录
        QString outputDir = baseDir.filePath("outputDir/" + subId);
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

QString MainViewController::resolveDefaultBidsPath(const QString& bidsPath) const
{
    const QString normalized = normalizedPathOrEmpty(bidsPath);
    if (!normalized.isEmpty()) {
        return normalized;
    }

    return defaultProcessingPaths().value(QStringLiteral("bidsPath")).toString();
}

QString MainViewController::resolveDefaultOutputPath(const QString& outputPath) const
{
    const QString normalized = normalizedPathOrEmpty(outputPath);
    if (!normalized.isEmpty()) {
        return normalized;
    }

    return defaultProcessingPaths().value(QStringLiteral("outputPath")).toString();
}

QVariantMap MainViewController::defaultProcessingPaths() const
{
    const QString rootPath = QDir::fromNativeSeparators(managedStorageRoot());
    const QString inputRootPath = QDir::fromNativeSeparators(managedInputRoot());
    const QString outputRootPath = QDir::fromNativeSeparators(managedOutputRoot());
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));

    ensureDirectoryExists(rootPath);
    ensureDirectoryExists(inputRootPath);
    ensureDirectoryExists(outputRootPath);

    QVariantMap result;
    result.insert(QStringLiteral("rootPath"), rootPath);
    result.insert(QStringLiteral("inputRootPath"), inputRootPath);
    result.insert(QStringLiteral("outputRootPath"), outputRootPath);
    result.insert(QStringLiteral("bidsPath"),
                  QDir::fromNativeSeparators(QDir(inputRootPath).filePath(QStringLiteral("BIDS_%1").arg(timestamp))));
    result.insert(QStringLiteral("outputPath"),
                  QDir::fromNativeSeparators(QDir(outputRootPath).filePath(QStringLiteral("Output_%1").arg(timestamp))));
    return result;
}

QString MainViewController::defaultLicenseFilePath() const
{
    const QDir currentDir(QDir::currentPath());
    const QStringList candidates = {
        QStringLiteral("license.txt"),
        QStringLiteral("fs_license.txt"),
        QStringLiteral("license")
    };

    for (const auto &name : candidates) {
        const QString path = currentDir.filePath(name);
        if (QFileInfo::exists(path))
            return QDir::fromNativeSeparators(path);
    }

    return QDir::fromNativeSeparators(currentDir.filePath(QStringLiteral("license.txt")));
}

void MainViewController::processBrainNetworkAnalysis(const QString& boldPath, const QString& confoundsPath, const QString& outputDir)
{
    emit brainAnalysisStarted();

    qDebug() << QStringLiteral("启动脑网络分析 (pybind11)");
    qDebug() << QStringLiteral("  BOLD:") << boldPath;
    qDebug() << QStringLiteral("  Confounds:") << confoundsPath;
    qDebug() << QStringLiteral("  Output:") << outputDir;

    QtConcurrent::run([=]() {
        bool success = false;
        try {
            py::gil_scoped_acquire acquire;
            py::module_ brain_network = py::module_::import("brain_network");

            py::module_ sys = py::module_::import("sys");
            py::list argv;
            argv.append("brain_network");
            argv.append("--bold");
            argv.append(boldPath.toStdString());
            argv.append("--confounds");
            argv.append(confoundsPath.toStdString());
            argv.append("--tr");
            argv.append("2.0");
            argv.append("--output");
            argv.append(outputDir.toStdString());
            sys.attr("argv") = argv;

            brain_network.attr("main")();
            success = true;
            qDebug() << QStringLiteral("脑网络分析完成！");
        } catch (const py::error_already_set& e) {
            qWarning() << QStringLiteral("脑网络分析 Python 错误:") << e.what();
        } catch (const std::exception& e) {
            qWarning() << QStringLiteral("脑网络分析错误:") << e.what();
        }

        QMetaObject::invokeMethod(this, [=]() {
            if (success) {
                if (loadOutputData(outputDir)) {
                    qDebug() << QStringLiteral("结果加载成功");
                    emit brainAnalysisFinished(true);
                } else {
                    qWarning() << QStringLiteral("结果加载失败");
                    emit brainAnalysisFinished(false);
                }
            } else {
                emit brainAnalysisFinished(false);
            }
        }, Qt::QueuedConnection);
    });
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
    painter.drawText(columnX, textY, QStringLiteral("检测医院:") + QStringLiteral("xxxxx"));

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

    QString axialPath, coronalPath, sagittalPath, seg3dPath;

    QString networkOutputDir;
    if (!getcurrentAlffUrl().isEmpty()) {
        networkOutputDir = getcurrentAlffUrl();
        if (networkOutputDir.startsWith("file:///")) {
            networkOutputDir = networkOutputDir.mid(8);
        }
        networkOutputDir = QFileInfo(networkOutputDir).absolutePath();
    }

    if (!networkOutputDir.isEmpty()) {
        const QString subjectId = QFileInfo(networkOutputDir).fileName();
        QDir baseDir(networkOutputDir);
        baseDir.cdUp(); // outputDir
        baseDir.cdUp(); // 预处理输出根目录
        const QString slicesDir = baseDir.filePath("brain_regions/" + subjectId + "/slices");

        axialPath = QDir(slicesDir).filePath("axial_mid.png");
        coronalPath = QDir(slicesDir).filePath("coronal_mid.png");
        sagittalPath = QDir(slicesDir).filePath("sagittal_mid.png");
        seg3dPath = QDir(slicesDir).filePath("seg3d_superior.png");
    }

    if (!QFile::exists(axialPath) || !QFile::exists(coronalPath)
            || !QFile::exists(sagittalPath) || !QFile::exists(seg3dPath)) {
        const QString tempDir = QDir::tempPath() + "/brain_seg_images";
        QDir().mkpath(tempDir);
        GET_SINGLETON(DicomDataModel)->generateSegDataPNGs(tempDir, axialPath, coronalPath, sagittalPath, seg3dPath);
    }
    
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
    segColWidths[2] = (int)(tableAvailableWidth * 0.17);  // 容积(cm^3)
    segColWidths[3] = (int)(tableAvailableWidth * 0.17);  // 全脑占比
    segColWidths[4] = (int)(tableAvailableWidth * 0.16);  // 不对称指数
    
    int segTotalWidth = 0;
    for (int w : segColWidths) segTotalWidth += w;
    
    QStringList segHeaders = {QStringLiteral("中文名称"), QStringLiteral("位置"), 
                              QStringLiteral("容积(mm3)"), QStringLiteral("全脑占比"), 
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

QString MainViewController::estimateProcessingTime(int method, int subjectCount)
{
    if (subjectCount <= 0) {
        return QStringLiteral("请选择受训者");
    }
    
    double totalMinutes = 0.0;
    double avgPerSubject = 0.0;
    
    if (method == 0) {
        // fmriprep: 有初始化开销的线性模型（批量处理有效率提升）
        // 根据实测数据：
        // 1人: 110分钟（1小时50分钟）
        // 23人: 1426.7分钟（23小时46分43秒），平均62分钟/人
        // 拟合公式：T = 50 + 60 * n 分钟
        const double baseMinutes = 50.0;        // 固定开销
        const double minutesPerSubject = 60.0;  // 每人处理时间
        totalMinutes = baseMinutes + minutesPerSubject * subjectCount;
        avgPerSubject = totalMinutes / subjectCount;
    } else {
        // deepprep: 有初始化开销的线性模型（并行处理效率更高）
        // 根据实测数据：
        // 1人: 24.5分钟
        // 8人: 87分钟 (平均10.9分钟/人)
        // 18人: 195分钟 (平均10.8分钟/人)
        // 53人: 628分钟 (平均11.9分钟/人)
        // 拟合公式：T = 13 + 11.5 * n 分钟
        const double baseMinutes = 13.0;        // 初始化开销
        const double minutesPerSubject = 11.5;  // 每人处理时间
        totalMinutes = baseMinutes + minutesPerSubject * subjectCount;
        avgPerSubject = totalMinutes / subjectCount;
    }
    
    // 转换为时分秒格式
    int totalSeconds = static_cast<int>(totalMinutes * 60);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    
    QString result;
    if (hours > 0) {
        result = QStringLiteral("约 %1 小时 %2 分钟").arg(hours).arg(minutes);
    } else {
        result = QStringLiteral("约 %1 分钟").arg(minutes);
    }
    
    // 添加平均每人耗时提示
    int avgMinutes = static_cast<int>(avgPerSubject);
    result += QStringLiteral(" (平均每人 %1 分钟)").arg(avgMinutes);
    
    return result;
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
    
    // 辅助函数：检查目录下是否存在任何以 sub- 开头的子目录，并且该子目录下存在指定的子路径
    auto hasSubDirWithPath = [&outputDir](const QString& parentDir, const QString& subPath) -> bool {
        QDir parent(outputDir.filePath(parentDir));
        if (!parent.exists()) {
            return false;
        }
        QStringList subDirs = parent.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subDir : subDirs) {
            if (subDir.startsWith("sub-")) {
                QDir subjectDir(parent.filePath(subDir));
                if (subjectDir.exists(subPath)) {
                    return true;
                }
            }
        }
        return false;
    };
    
    // DeepPrep有QC、BOLD、Recon这三个主要文件夹
    bool hasQC = hasSubDirWithPath("QC", "figures");
    bool hasBOLD = hasSubDirWithPath("BOLD", "func");
    bool hasRecon = outputDir.exists("Recon") && 
                   (outputDir.exists("Recon/fsaverage/mri") || hasSubDirWithPath("Recon", "mri"));
    
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

void MainViewController::scanFolder(const QString& inputDir, int mode)
{
    if (!m_mriScanner) {
        return;
    }
    clearPreAnalysisLog();
    // mode: 0 = 配对模式 (T1W+BOLD), 1 = 脑龄模式 (仅T1W)
    m_mriScanner->startScan(inputDir, 5, mode);
}

void MainViewController::startPreAnalysis(int method, const QString& bidsPath, const QString& outputPath, const QString& licenseFile)
{
    Q_UNUSED(bidsPath);
    Q_UNUSED(outputPath);

    // 获取选中的 MRI 配对结果
    const QList<MriPairResult> selectedResults = m_mriPairResultModel->getCheckedResults();
    QList<MriPairResult> checkedResults;
    QSet<QString> selectedCaseKeys;
    int skippedProcessing = 0;
    int skippedQueued = 0;
    int skippedCompleted = 0;
    int skippedDuplicatedSelection = 0;

    auto *appDataModel = GET_SINGLETON(AppDataModel);
    for (const auto& pair : selectedResults) {
        const QString caseKey = buildCaseIdentityKey(pair);
        if (selectedCaseKeys.contains(caseKey)) {
            ++skippedDuplicatedSelection;
            continue;
        }

        const QString status = appDataModel
            ? appDataModel->matchingRecordStatus(pair.patientName, pair.studyDate, pair.primarySeriesUid())
            : QString();

        if (status == QStringLiteral("processing")) {
            ++skippedProcessing;
            continue;
        }
        if (status == QStringLiteral("queued")) {
            ++skippedQueued;
            continue;
        }
        selectedCaseKeys.insert(caseKey);
        checkedResults.append(pair);
    }

    if (checkedResults.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral(">>> 当前勾选病例均已分析完成、分析中或排队中，未加入新任务\n"));
        return;
    }

    const bool startsAsActive = !m_isProcessing && m_processingQueue.isEmpty();
    const QString initialStatus = startsAsActive ? QStringLiteral("processing")
                                                 : QStringLiteral("queued");

    addPendingTasks(checkedResults, method, QString(), QString(), initialStatus, true, true);

    // 按批次添加到处理队列
    addToProcessingQueue(checkedResults, method, QString(), QString(), licenseFile, true, true);

    appendPreAnalysisLog(QStringLiteral("\n>>> 已添加 1 个批次到队列（本批次 %1 个被试，当前共 %2 个待处理批次）\n")
                         .arg(checkedResults.size()).arg(getQueueSize()));
    if (skippedProcessing > 0 || skippedQueued > 0 || skippedCompleted > 0 || skippedDuplicatedSelection > 0) {
        appendPreAnalysisLog(QStringLiteral(">>> 已自动跳过重复病例：分析中 %1 条，排队中 %2 条，已完成 %3 条，当前勾选重复 %4 条（按 seriesUID 优先，其次姓名+拍摄日期匹配）\n")
                             .arg(skippedProcessing)
                             .arg(skippedQueued)
                             .arg(skippedCompleted)
                             .arg(skippedDuplicatedSelection));
    }

    // 如果队列未运行，启动它
    if (!m_isProcessing) {
        appendPreAnalysisLog(QStringLiteral(">>> 启动队列处理...\n"));
        startProcessingQueue();
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 队列正在运行，新任务已加入队列\n"));
    }
}

void MainViewController::startBrainAgeOnly()
{
    const QList<MriPairResult> selectedResults = m_mriPairResultModel->getCheckedResults();
    QList<MriPairResult> checkedResults;
    QSet<QString> selectedCaseKeys;

    auto *appDataModel = GET_SINGLETON(AppDataModel);
    for (const auto& pair : selectedResults) {
        const QString caseKey = buildCaseIdentityKey(pair);
        if (selectedCaseKeys.contains(caseKey)) continue;

        const QString status = appDataModel
            ? appDataModel->matchingRecordStatus(pair.patientName, pair.studyDate, pair.primarySeriesUid())
            : QString();

        if (status == QStringLiteral("processing") ||
            status == QStringLiteral("queued")) continue;

        selectedCaseKeys.insert(caseKey);
        checkedResults.append(pair);
    }

    if (checkedResults.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral(">>> 当前勾选病例均已分析完成、分析中或排队中，未加入新任务\n"));
        return;
    }

    const bool startsAsActive = !m_isProcessing && m_processingQueue.isEmpty();
    const QString initialStatus = startsAsActive ? QStringLiteral("processing")
                                                 : QStringLiteral("queued");
    addPendingTasks(checkedResults, 0, QString(), QString(), initialStatus, true, false);

    QueueItem item;
    item.method = 0;
    item.runBrainAge = true;
    item.runPreprocessing = false;
    item.pairResults = checkedResults;
    m_processingQueue.append(item);

    appendPreAnalysisLog(QStringLiteral("\n>>> [仅脑龄预测] 已添加 %1 个被试到队列\n").arg(checkedResults.size()));

    if (!m_isProcessing) {
        startProcessingQueue();
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 队列正在运行，新任务已加入队列\n"));
    }
}

void MainViewController::startPreprocessingOnly(int method, const QString& bidsPath, const QString& outputPath, const QString& licenseFile)
{
    Q_UNUSED(bidsPath);
    Q_UNUSED(outputPath);

    const QList<MriPairResult> selectedResults = m_mriPairResultModel->getCheckedResults();
    QList<MriPairResult> checkedResults;
    QSet<QString> selectedCaseKeys;

    auto *appDataModel = GET_SINGLETON(AppDataModel);
    for (const auto& pair : selectedResults) {
        const QString caseKey = buildCaseIdentityKey(pair);
        if (selectedCaseKeys.contains(caseKey))
            continue;

        const QString status = appDataModel
            ? appDataModel->matchingRecordStatus(pair.patientName, pair.studyDate, pair.primarySeriesUid())
            : QString();

        if (status == QStringLiteral("processing") || status == QStringLiteral("queued"))
            continue;

        selectedCaseKeys.insert(caseKey);
        checkedResults.append(pair);
    }

    if (checkedResults.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral(">>> 当前勾选病例均在分析中或排队中，未加入新任务\n"));
        return;
    }

    const bool startsAsActive = !m_isProcessing && m_processingQueue.isEmpty();
    const QString initialStatus = startsAsActive ? QStringLiteral("processing")
                                                 : QStringLiteral("queued");

    addPendingTasks(checkedResults, method, QString(), QString(), initialStatus, false, true);
    addToProcessingQueue(checkedResults, method, QString(), QString(), licenseFile, false, true);

    appendPreAnalysisLog(QStringLiteral("\n>>> [仅预处理] 已添加 %1 个被试到队列\n").arg(checkedResults.size()));

    if (!m_isProcessing) {
        startProcessingQueue();
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 队列正在运行，新任务已加入队列\n"));
    }
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
    appendPreAnalysisLog(QStringLiteral(">>> BIDS 转换完成，共 %1 个被试\n").arg(results.size()));

    // 回填当前批次的 subjectId，保持和转换结果一致
    const int count = qMin(m_currentQueuePairs.size(), results.size());
    for (int i = 0; i < count; ++i) {
        m_currentQueuePairs[i].subjectId = results[i].subjectId;
    }
    m_currentProcessingPairs = m_currentQueuePairs;
    writeMetadataFile(m_preAnalysisOutputPath, m_currentQueuePairs);

    appendPreAnalysisLog(QStringLiteral(">>> [Step 2/3] 启动 Deface...\n"));
    appendPreAnalysisLog(QStringLiteral(">>> Deface 完成后将启动模式：%1\n")
                         .arg(processingModeLabel(m_currentItem.runBrainAge, m_currentItem.runPreprocessing)));
    startDefaceAfterBids();
}

QStringList MainViewController::currentQueueSubjectIds() const
{
    QStringList subjects;
    for (const auto& pair : m_currentQueuePairs) {
        const QString subjectId = pair.subjectId.trimmed();
        if (!subjectId.isEmpty() && !subjects.contains(subjectId))
            subjects.append(subjectId);
    }
    return subjects;
}

bool MainViewController::applyDefacedDataToCurrentBids()
{
    const QStringList subjects = currentQueueSubjectIds();
    if (subjects.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral(">>> 警告：当前批次没有 subjectId，无法切换 defaced 数据\n"));
        return false;
    }

    int replacedCount = 0;
    QStringList failures;
    const QDir bidsDir(m_preAnalysisBidsPath);
    for (const QString& subjectId : subjects) {
        const QDir anatDir(bidsDir.filePath(subjectId + QStringLiteral("/anat")));
        if (!anatDir.exists()) {
            failures.append(QStringLiteral("%1: anat 目录不存在").arg(subjectId));
            continue;
        }

        const QStringList defacedFiles = anatDir.entryList(QStringList() << QStringLiteral("*_defaced.nii.gz"),
                                                           QDir::Files,
                                                           QDir::Name);
        if (defacedFiles.isEmpty()) {
            failures.append(QStringLiteral("%1: 未找到 *_defaced.nii.gz").arg(subjectId));
            continue;
        }

        for (const QString& defacedFile : defacedFiles) {
            const QString defacedPath = anatDir.filePath(defacedFile);
            QString originalFile = defacedFile;
            originalFile.replace(QStringLiteral("_defaced.nii.gz"), QStringLiteral(".nii.gz"), Qt::CaseInsensitive);
            const QString originalPath = anatDir.filePath(originalFile);

            if (QFile::exists(originalPath) && !QFile::remove(originalPath)) {
                failures.append(QStringLiteral("%1: 无法覆盖 %2").arg(subjectId, originalFile));
                continue;
            }
            if (!QFile::copy(defacedPath, originalPath)) {
                failures.append(QStringLiteral("%1: 复制 defaced 文件失败 %2").arg(subjectId, defacedFile));
                continue;
            }
            ++replacedCount;
        }
    }

    if (!failures.isEmpty()) {
        for (const QString& failure : failures)
            appendPreAnalysisLog(QStringLiteral(">>> %1\n").arg(failure));
    }

    if (replacedCount > 0) {
        appendPreAnalysisLog(QStringLiteral(">>> 已将 %1 个去脸后的 T1 文件同步为下游预处理输入\n").arg(replacedCount));
    }

    return replacedCount > 0 && failures.isEmpty();
}

bool MainViewController::persistBrainAgePredictionsToOutput()
{
    const QString sourcePath = findBrainAgePredictionCsvPath(m_preAnalysisBidsPath);
    if (sourcePath.isEmpty())
        return false;

    QDir().mkpath(m_preAnalysisOutputPath);
    const QString targetPath = QDir(m_preAnalysisOutputPath).filePath(QStringLiteral("brain_age_predictions.csv"));
    if (QFileInfo(sourcePath).absoluteFilePath() == QFileInfo(targetPath).absoluteFilePath())
        return true;
    if (QFile::exists(targetPath) && !QFile::remove(targetPath))
        return false;
    if (!QFile::copy(sourcePath, targetPath))
        return false;

    appendPreAnalysisLog(QStringLiteral(">>> 脑龄预测结果已保存到输出目录：%1\n").arg(targetPath));
    return true;
}


void MainViewController::startDefaceAfterBids()
{
    setbrainAgeProcessing(m_currentItem.runBrainAge);
    m_dockerPrepRunner->runDeface(m_preAnalysisBidsPath, currentQueueSubjectIds());
}

void MainViewController::startBapAfterDeface()
{
    appendPreAnalysisLog(QStringLiteral(">>> 启动脑龄预测 (Docker bap:0312-v1.0)...\n"));
    m_dockerPrepRunner->runBap(m_preAnalysisBidsPath, currentQueueSubjectIds());
}

void MainViewController::startFmriprepAfterBids()
{
    stopFmriprepProcess();

    FmriPrepParams params;
    params.bidsDir = m_preAnalysisBidsPath;
    params.outputDir = m_preAnalysisOutputPath;
    params.licenseFile = m_preAnalysisLicenseFile;
    params.skipBidsValidation = true;
    params.fsNoReconall = false;
    params.ignoreFieldmaps = true;
    params.subjects = currentQueueSubjectIds();

    m_prepLogFilePath = m_preAnalysisOutputPath + "/fmriprep-docker.log";
    m_prepLogReadPos = 0;
    startPrepLogTimer(m_prepLogFilePath);

    appendPreAnalysisLog(QStringLiteral(">>> fMRIPrep 已启动\n"));
    m_dockerPrepRunner->runFmriPrep(params);
}

void MainViewController::startDeepprepAfterBids()
{
    stopDeepprepProcess();

    DeepPrepParams params;
    params.bidsDir = m_preAnalysisBidsPath;
    params.outputDir = m_preAnalysisOutputPath;
    params.licenseFile = m_preAnalysisLicenseFile;
    params.skipBidsValidation = true;
    params.boldSdc = false;
    params.device = "auto";
    params.subjects = currentQueueSubjectIds();

    m_prepLogFilePath = m_preAnalysisOutputPath + "/deepprep-docker.log";
    m_prepLogReadPos = 0;
    startPrepLogTimer(m_prepLogFilePath);

    appendPreAnalysisLog(QStringLiteral(">>> DeepPrep 已启动\n"));
    m_dockerPrepRunner->runDeepPrep(params);
}

void MainViewController::onDefaceFinished(bool success, const QString& message)
{
    if (!success) {
        setbrainAgeProcessing(false);
        if (m_currentItem.runPreprocessing)
            onPrepFinished(false, message);
        if (m_currentItem.runBrainAge)
            onBrainAgePredictionFinished(false);
    }
}

void MainViewController::onBrainAgePredictionFinished(bool success)
{
    m_currentBrainAgeDone = true;
    m_currentBrainAgeSuccess = success;

    // 脑龄预测完成后，先把结果匹配到当前批次，待完成病例入库时一并落库。
    if (success) {
        if (loadBrainAgePredictions(m_preAnalysisOutputPath)) {
            appendPreAnalysisLog(QStringLiteral(">>> 已加载脑龄预测结果\n"));

            if (!m_brainAgePredictions.isEmpty()) {
                for (MriPairResult& pair : m_currentQueuePairs) {
                    const double predictedAge = resolvePredictedBrainAgeForPair(pair, m_brainAgePredictions);
                    if (predictedAge < 0)
                        continue;

                    pair.predictedBrainAge = predictedAge;
                    qDebug() << QStringLiteral("匹配脑龄预测结果: %1 / %2 = %3")
                                    .arg(pair.subjectId, pair.patientId)
                                    .arg(predictedAge);
                }
            }
        } else {
            appendPreAnalysisLog(QStringLiteral(">>> 警告：脑龄预测结果加载失败\n"));
        }
    }

    tryFinishCurrentQueueItem();
}

void MainViewController::onPrepFinished(bool success, const QString& message)
{
    Q_UNUSED(message);

    if (!success) {
        m_currentPrepDone = true;
        m_currentPrepSuccess = false;
        tryFinishCurrentQueueItem();
        return;
    }

    appendPreAnalysisLog(QStringLiteral(">>> 预处理完成，开始执行输出目录后处理（脑区分割 + 脑网络）...\n"));
    m_currentBrainRegionDone = false;
    m_currentBrainRegionSuccess = false;
    m_currentBrainNetworkDone = false;
    m_currentBrainNetworkSuccess = false;

    startBrainRegionProcessing();
    startBrainNetworkPostProcessing();
}

void MainViewController::startBrainNetworkPostProcessing()
{
    const QList<MriPairResult> pairs = m_currentQueuePairs;
    const QString outputBase = m_preAnalysisOutputPath;
    const int method = m_preAnalysisMethod;

    if (pairs.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理失败：当前批次为空\n"));
        onBrainNetworkPostProcessingFinished(false);
        return;
    }

    QtConcurrent::run([this, pairs, outputBase, method]() {
        int successCount = 0;
        int failCount = 0;
        QString firstSuccessOutputDir;

        for (const auto& pair : pairs) {
            const QString subjectId = pair.subjectId.trimmed();
            if (subjectId.isEmpty()) {
                ++failCount;
                QMetaObject::invokeMethod(this, [this]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理跳过：缺少 subjectId\n"));
                }, Qt::QueuedConnection);
                continue;
            }

            QString boldPath;
            QString confoundsPath;
            if (method == 0) {
                boldPath = QDir(outputBase).filePath(subjectId + "/func/" + subjectId + "_task-rest_space-MNI152NLin2009cAsym_res-2_desc-preproc_bold.nii.gz");
                confoundsPath = QDir(outputBase).filePath(subjectId + "/func/" + subjectId + "_task-rest_desc-confounds_timeseries.tsv");
            } else {
                boldPath = QDir(outputBase).filePath("BOLD/" + subjectId + "/func/" + subjectId + "_task-rest_space-MNI152NLin6Asym_res-02_desc-preproc_bold.nii.gz");
                confoundsPath = QDir(outputBase).filePath("BOLD/" + subjectId + "/func/" + subjectId + "_task-rest_desc-confounds_timeseries.tsv");
            }

            if (!QFileInfo::exists(boldPath) || !QFileInfo::exists(confoundsPath)) {
                ++failCount;
                QMetaObject::invokeMethod(this, [this, subjectId, boldPath, confoundsPath]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理失败：%1 缺少输入文件\n    BOLD: %2\n    Confounds: %3\n")
                                         .arg(subjectId, boldPath, confoundsPath));
                }, Qt::QueuedConnection);
                continue;
            }

            const QString networkOutputDir = QDir(outputBase).filePath("outputDir/" + subjectId);
            QDir().mkpath(networkOutputDir);

            bool ok = false;
            try {
                py::gil_scoped_acquire acquire;
                py::module_ brain_network = py::module_::import("brain_network");
                py::module_ sys = py::module_::import("sys");
                py::list argv;
                argv.append("brain_network");
                argv.append("--bold");
                argv.append(boldPath.toStdString());
                argv.append("--confounds");
                argv.append(confoundsPath.toStdString());
                argv.append("--tr");
                argv.append("2.0");
                argv.append("--output");
                argv.append(networkOutputDir.toStdString());
                sys.attr("argv") = argv;
                brain_network.attr("main")();
                ok = QFileInfo::exists(QDir(networkOutputDir).filePath("alff.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("alff_transparent.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("covariance.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("covariance_transparent.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("viewConnectome.html"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("viewConnectome.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("region_plots/001_Precentral_L_transparent.png"))
                        && QFileInfo::exists(QDir(networkOutputDir).filePath("brain_network_results.json"));
            } catch (const py::error_already_set& e) {
                QMetaObject::invokeMethod(this, [this, subjectId, err = QString::fromUtf8(e.what())]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理 Python 错误 [%1]: %2\n").arg(subjectId, err));
                }, Qt::QueuedConnection);
            } catch (const std::exception& e) {
                QMetaObject::invokeMethod(this, [this, subjectId, err = QString::fromUtf8(e.what())]() {
                    appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理错误 [%1]: %2\n").arg(subjectId, err));
                }, Qt::QueuedConnection);
            }

            if (ok) {
                ++successCount;
                if (firstSuccessOutputDir.isEmpty())
                    firstSuccessOutputDir = networkOutputDir;
            } else {
                ++failCount;
            }
        }

        QMetaObject::invokeMethod(this, [this, successCount, failCount, firstSuccessOutputDir]() {
            if (!firstSuccessOutputDir.isEmpty()) {
                loadOutputData(firstSuccessOutputDir);
            }
            appendPreAnalysisLog(QStringLiteral(">>> 脑网络后处理完成: 成功 %1 个, 失败 %2 个\n")
                                 .arg(successCount).arg(failCount));
            onBrainNetworkPostProcessingFinished(failCount == 0 && successCount > 0);
        }, Qt::QueuedConnection);
    });
}

void MainViewController::onBrainRegionPostProcessingFinished(bool success, int successCount, int failCount)
{
    Q_UNUSED(successCount);
    Q_UNUSED(failCount);
    m_currentBrainRegionDone = true;
    m_currentBrainRegionSuccess = success;
    tryFinishPrepPostProcessing();
}

void MainViewController::onBrainNetworkPostProcessingFinished(bool success)
{
    m_currentBrainNetworkDone = true;
    m_currentBrainNetworkSuccess = success;
    tryFinishPrepPostProcessing();
}

void MainViewController::tryFinishPrepPostProcessing()
{
    if (!m_currentBrainRegionDone || !m_currentBrainNetworkDone) {
        return;
    }

    m_currentPrepDone = true;
    m_currentPrepSuccess = m_currentBrainRegionSuccess && m_currentBrainNetworkSuccess;

    if (m_currentPrepSuccess) {
        appendPreAnalysisLog(QStringLiteral(">>> 输出目录后处理完成：脑区分割与脑网络均成功\n"));
    } else {
        appendPreAnalysisLog(QStringLiteral(">>> 输出目录后处理未全部成功，本批次不计为完成病例\n"));
    }

    tryFinishCurrentQueueItem();
}

void MainViewController::tryFinishCurrentQueueItem()
{
    if (!m_currentPrepDone || !m_currentBrainAgeDone) {
        appendPreAnalysisLog(QStringLiteral(">>> [完成判定] 等待中：prepDone=%1, brainAgeDone=%2\n")
                             .arg(m_currentPrepDone ? 1 : 0)
                             .arg(m_currentBrainAgeDone ? 1 : 0));
        return;
    }

    const bool hasPrepResult = m_currentItem.runPreprocessing ? m_currentPrepSuccess : false;
    const bool hasBrainAgeResult = m_currentItem.runBrainAge ? m_currentBrainAgeSuccess : false;

    if ((hasPrepResult || hasBrainAgeResult) && m_dbManager && !m_currentQueuePairs.isEmpty()) {
        int insertedCount = 0;
        int skippedCount = 0;

        for (const auto& pair : m_currentQueuePairs) {
            const QString name = pair.patientName;
            const QString patientId = pair.patientId;
            const QString examDate = pair.studyDate;
            const QString seriesUid = buildCompletedCaseSeriesUid(pair, m_preAnalysisMethod);
            const int age = calculateAgeFromDates(pair.patientBirthDate, pair.studyDate);
            const QString sex = normalizeDisplayedSex(pair.patientSex);
            const QString bidsPath = m_preAnalysisBidsPath;
            const QString outputPath = m_preAnalysisOutputPath;
            const double predictedBrainAge = pair.predictedBrainAge >= 0
                    ? pair.predictedBrainAge
                    : resolvePredictedBrainAgeForPair(pair, m_brainAgePredictions);
            const bool pairHasBrainAgeResult = predictedBrainAge >= 0 || hasBrainAgeResult;
            const QString preprocessMethod = hasPrepResult ? preprocessMethodLabel(m_preAnalysisMethod)
                                                           : QString();

            const bool ok = m_dbManager->insertCompletedCase(name, patientId, examDate, seriesUid,
                                                             age, sex, bidsPath, outputPath,
                                                             predictedBrainAge,
                                                             pairHasBrainAgeResult,
                                                             hasPrepResult,
                                                             preprocessMethod);
            if (ok) {
                ++insertedCount;
            } else {
                ++skippedCount;
                qWarning() << "Insert completed case skipped/failed for patient"
                           << patientId << ":" << m_dbManager->lastError();
            }
        }

        if (insertedCount > 0)
            refreshAppDataModel();

        appendPreAnalysisLog(QStringLiteral(">>> 已写入完成病例表：成功 %1 条，跳过/失败 %2 条\n")
                             .arg(insertedCount).arg(skippedCount));
    } else {
        // 详细输出未满足条件的原因，便于定位
        const bool hasDb = (m_dbManager != nullptr);
        const bool hasPairs = !m_currentQueuePairs.isEmpty();
        appendPreAnalysisLog(QStringLiteral(">>> [完成判定] 未满足条件，跳过入库。\n"));
        appendPreAnalysisLog(QStringLiteral("    - prepDone=%1, prepSuccess=%2\n")
                             .arg(m_currentPrepDone ? 1 : 0)
                             .arg(m_currentPrepSuccess ? 1 : 0));
        appendPreAnalysisLog(QStringLiteral("    - brainAgeDone=%1, brainAgeSuccess=%2\n")
                             .arg(m_currentBrainAgeDone ? 1 : 0)
                             .arg(m_currentBrainAgeSuccess ? 1 : 0));
        appendPreAnalysisLog(QStringLiteral("    - dbManager=%1\n").arg(hasDb ? QStringLiteral("OK") : QStringLiteral("NULL")));
        appendPreAnalysisLog(QStringLiteral("    - currentQueuePairs=%1\n").arg(hasPairs ? QStringLiteral("OK") : QStringLiteral("EMPTY")));
        if (!hasPairs) {
            appendPreAnalysisLog(QStringLiteral("    - 提示：当前批次 pairs 为空，可能是队列状态/删除/清空导致。\n"));
        }
        if (m_currentItem.runPreprocessing && !m_currentPrepSuccess) {
            appendPreAnalysisLog(QStringLiteral("    - 提示：prepSuccess=false 通常是脑区分割或脑网络后处理失败。\n"));
        }
        if (m_currentItem.runBrainAge && !m_currentBrainAgeSuccess) {
            appendPreAnalysisLog(QStringLiteral("    - 提示：brainAgeSuccess=false 通常是 BAP 失败或未找到脑龄结果 CSV。\n"));
        }
    }

    removePendingTasks(m_currentQueuePairs, m_preAnalysisMethod);
    m_currentQueuePairs.clear();
    processNextInQueue();
}

void MainViewController::addPendingTasks(const QList<MriPairResult>& pairs, int method,
                                         const QString& bidsPath, const QString& outputPath,
                                         const QString& status,
                                         bool runBrainAge, bool runPreprocessing)
{
    auto *model = GET_SINGLETON(AppDataModel);
    if (!model)
        return;

    QString checkType;
    if (runBrainAge && runPreprocessing)
        checkType = QStringLiteral("FullPipeline");
    else if (runBrainAge)
        checkType = QStringLiteral("BrainAgeOnly");
    else if (runPreprocessing)
        checkType = QStringLiteral("PrepOnly");
    else
        checkType = QStringLiteral("Unknown");

    for (const auto &pair : pairs) {
        HardwareScanResult item;
        item.name = pair.patientName;
        item.patientId = pair.patientId;
        item.examDate = pair.studyDate;
        item.checkType = checkType;
        item.sex = normalizeDisplayedSex(pair.patientSex);
        item.age = calculateAgeFromDates(pair.patientBirthDate, pair.studyDate);
        item.seriesUid = pair.primarySeriesUid();
        item.sliceCount = pair.t1ImageCount + pair.boldImageCount;
        item.status = status;
        item.outputPath = outputPath;
        item.bidsPath = bidsPath;
        model->addPendingItem(item);
    }
}

void MainViewController::removePendingTasks(const QList<MriPairResult>& pairs, int method)
{
    Q_UNUSED(method);

    auto *model = GET_SINGLETON(AppDataModel);
    if (!model)
        return;

    for (const auto &pair : pairs) {
        model->removePendingItem(pair.patientName, pair.studyDate, pair.primarySeriesUid());
    }
}

void MainViewController::updatePendingTasksStatus(const QList<MriPairResult>& pairs, const QString& status)
{
    auto *model = GET_SINGLETON(AppDataModel);
    if (!model)
        return;

    for (const auto &pair : pairs) {
        model->setPendingItemStatus(pair.patientName, pair.studyDate, pair.primarySeriesUid(), status);
    }
}

void MainViewController::addToProcessingQueue(const QList<MriPairResult>& pairs, int method,
                                              const QString& bidsPath, const QString& outputPath,
                                              const QString& licenseFile,
                                              bool runBrainAge, bool runPreprocessing)
{
    QueueItem item;
    item.method = method;
    item.bidsPath = bidsPath;
    item.outputPath = outputPath;
    item.licenseFile = licenseFile;
    item.runBrainAge = runBrainAge;
    item.runPreprocessing = runPreprocessing;
    item.pairResults = pairs;
    m_processingQueue.append(item);
    appendPreAnalysisLog(QStringLiteral(">>> [队列] 添加 1 个批次（%1 个被试，模式：%2），当前队列长度：%3\n")
                         .arg(pairs.size())
                         .arg(processingModeLabel(runBrainAge, runPreprocessing))
                         .arg(m_processingQueue.size()));
}

void MainViewController::startProcessingQueue()
{
    if (m_isProcessing || m_processingQueue.isEmpty()) return;
    m_queuePaused = false;
    processNextInQueue();
}

void MainViewController::pauseProcessingQueue()
{
    m_queuePaused = true;
    appendPreAnalysisLog(QStringLiteral(">>> 队列将在当前任务完成后暂停\n"));
}

void MainViewController::clearProcessingQueue()
{
    for (const auto &item : m_processingQueue) {
        removePendingTasks(item.pairResults, item.method);
    }
    const int removed = m_processingQueue.size();
    m_processingQueue.clear();
    appendPreAnalysisLog(QStringLiteral(">>> [队列] 已清空队列，移除 %1 个批次\n").arg(removed));
}

void MainViewController::processNextInQueue()
{
    if (m_queuePaused || m_processingQueue.isEmpty()) {
        m_isProcessing = false;
        setisPreAnalysisRunning(false);
        if (m_processingQueue.isEmpty()) {
            appendPreAnalysisLog(QStringLiteral("\n========== 所有任务处理完成 ==========\n"));
        }
        return;
    }

    m_isProcessing = true;
    m_currentItem = m_processingQueue.takeFirst();

    auto *model = GET_SINGLETON(AppDataModel);
    const auto firstPairStatus = [model](const QList<MriPairResult>& pairs) -> QString {
        if (!model || pairs.isEmpty())
            return {};
        const MriPairResult &pair = pairs.first();
        return model->matchingRecordStatus(pair.patientName, pair.studyDate, pair.primarySeriesUid());
    };

    const bool startingQueuedBacklog = (firstPairStatus(m_currentItem.pairResults) == QStringLiteral("queued"));
    int mergedBatchCount = 1;
    int incompatibleBatchCount = 0;

    if (startingQueuedBacklog && !m_processingQueue.isEmpty()) {
        QList<QueueItem> remainingQueue;
        for (const auto &queuedItem : m_processingQueue) {
            const bool sameExecutionConfig = queuedItem.method == m_currentItem.method
                    && queuedItem.licenseFile == m_currentItem.licenseFile
                    && queuedItem.runBrainAge == m_currentItem.runBrainAge
                    && queuedItem.runPreprocessing == m_currentItem.runPreprocessing;
            if (sameExecutionConfig) {
                m_currentItem.pairResults.append(queuedItem.pairResults);
                ++mergedBatchCount;
            } else {
                remainingQueue.append(queuedItem);
                ++incompatibleBatchCount;
            }
        }
        m_processingQueue = remainingQueue;
    }

    m_currentQueuePairs = m_currentItem.pairResults;
    m_currentProcessingPairs = m_currentQueuePairs;

    updatePendingTasksStatus(m_currentQueuePairs, QStringLiteral("processing"));

    m_currentPrepDone = !m_currentItem.runPreprocessing;
    m_currentPrepSuccess = !m_currentItem.runPreprocessing;
    m_currentBrainAgeDone = !m_currentItem.runBrainAge;
    m_currentBrainAgeSuccess = !m_currentItem.runBrainAge;
    m_currentBrainRegionDone = false;
    m_currentBrainRegionSuccess = false;
    m_currentBrainNetworkDone = false;
    m_currentBrainNetworkSuccess = false;

    QString firstSubId;
    QString firstName;
    if (!m_currentQueuePairs.isEmpty()) {
        const MriPairResult& firstPair = m_currentQueuePairs.first();
        firstSubId = firstPair.subjectId.isEmpty() ? firstPair.patientId : firstPair.subjectId;
        firstName = firstPair.patientName;
    }

    appendPreAnalysisLog(QStringLiteral("\n>>> 开始处理队列批次：%1 个被试")
                         .arg(m_currentQueuePairs.size()));
    if (!firstName.isEmpty() || !firstSubId.isEmpty()) {
        appendPreAnalysisLog(QStringLiteral("（首个被试 [%1]: %2）")
                             .arg(firstName, firstSubId));
    }
    appendPreAnalysisLog(QStringLiteral("，剩余 %1 个批次\n").arg(m_processingQueue.size()));
    appendPreAnalysisLog(QStringLiteral(">>> 当前模式：%1\n")
                         .arg(processingModeLabel(m_currentItem.runBrainAge, m_currentItem.runPreprocessing)));

    if (startingQueuedBacklog && mergedBatchCount > 1) {
        appendPreAnalysisLog(QStringLiteral(">>> 当前分析任务完成后，已自动合并 %1 个排队批次并一次性执行\n")
                             .arg(mergedBatchCount));
    }
    if (startingQueuedBacklog) {
        appendPreAnalysisLog(QStringLiteral(">>> 已从列表移除本轮开始执行的“排队中”项\n"));
    }
    if (incompatibleBatchCount > 0) {
        appendPreAnalysisLog(QStringLiteral(">>> 有 %1 个排队批次因处理方式或 license 不同，保留在队列中等待后续执行\n")
                             .arg(incompatibleBatchCount));
    }

    m_preAnalysisMethod = m_currentItem.method;
    const QVariantMap runtimePaths = defaultProcessingPaths();
    m_preAnalysisBidsPath = runtimePaths.value(QStringLiteral("bidsPath")).toString();
    m_preAnalysisOutputPath = runtimePaths.value(QStringLiteral("outputPath")).toString();
    m_currentItem.bidsPath = m_preAnalysisBidsPath;
    m_currentItem.outputPath = m_preAnalysisOutputPath;
    m_preAnalysisLicenseFile = m_currentItem.licenseFile;
    setisPreAnalysisRunning(true);

    QDir().mkpath(m_preAnalysisBidsPath);
    QDir().mkpath(m_preAnalysisOutputPath);
    appendPreAnalysisLog(QStringLiteral(">>> 已为当前执行批次创建独立目录：\n    BIDS: %1\n    Output: %2\n")
                         .arg(m_preAnalysisBidsPath, m_preAnalysisOutputPath));

    appendPreAnalysisLog(QStringLiteral(">>> 预处理将继续执行；脑龄预测会在 Deface 完成后通过 Docker BAP 启动\n"));

    // Step 1: BIDS 转换（整批 subjects）
    appendPreAnalysisLog(QStringLiteral(">>> [Step 1/2] 开始 BIDS 转换...\n"));
    m_bidsConverter->setOutputDirectory(m_preAnalysisBidsPath);
    m_bidsConverter->setDatasetName("BrainMRI_Study");
    m_bidsConverter->setTaskName("rest");
    m_bidsConverter->startConversion(m_currentQueuePairs);
}

// ================================================================
// 数据库相关 Q_INVOKABLE 方法
// ================================================================

bool MainViewController::initDatabase(const QString& dbPath)
{
    if (!m_dbManager->openDatabase(dbPath)) return false;
    m_dbManager->createTable();
    return m_dbManager->createCompletedCasesTable();
}

bool MainViewController::addCompletedCase(const QString& name, const QString& patientId,
                                           const QString& examDate, const QString& seriesUid,
                                           int age, const QString& sex, const QString& checkType,
                                           const QString& bidsPath, const QString& outputPath)
{
    const bool hasBrainAge = (checkType == QStringLiteral("BrainAgeOnly")
                              || checkType == QStringLiteral("FullPipeline"));
    const bool hasPreprocessing = (checkType == QStringLiteral("PrepOnly")
                                   || checkType == QStringLiteral("FullPipeline")
                                   || checkType == QStringLiteral("fMRIPrep")
                                   || checkType == QStringLiteral("DeepPrep"));
    const QString preprocessMethod = (checkType == QStringLiteral("DeepPrep"))
            ? QStringLiteral("DeepPrep")
            : (hasPreprocessing ? QStringLiteral("fMRIPrep") : QString());

    bool ok = m_dbManager->insertCompletedCase(name, patientId, examDate, seriesUid,
                                                age, sex, bidsPath, outputPath, -1.0,
                                                hasBrainAge, hasPreprocessing, preprocessMethod);
    if (ok) refreshAppDataModel();
    return ok;
}

bool MainViewController::removeCompletedCase(int id)
{
    bool ok = m_dbManager->deleteCompletedCase(id);
    if (ok) refreshAppDataModel();
    return ok;
}

QVariantList MainViewController::searchCases(const QString& keyword)
{
    QList<QVariantMap> results = keyword.isEmpty()
        ? m_dbManager->getAllCompletedCases()
        : m_dbManager->searchCompletedCases(keyword);
    QVariantList list;
    for (const auto& r : results)
        list.append(r);
    return list;
}

void MainViewController::refreshAppDataModel()
{
    auto* model = GET_SINGLETON(AppDataModel);
    model->setCompletedItems(m_dbManager->getAllCompletedCases());
}

bool MainViewController::removeQueuedTask(const QString& name, const QString& examDate, const QString& seriesUid)
{
    bool removedFromQueue = false;

    auto normDate = [](const QString& d) -> QString {
        QString digits = d.trimmed();
        digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
        return digits.left(8);
    };
    const QString normExamDate = normDate(examDate);

    for (int i = m_processingQueue.size() - 1; i >= 0; --i) {
        QList<MriPairResult>& pairs = m_processingQueue[i].pairResults;
        for (int j = pairs.size() - 1; j >= 0; --j) {
            const auto& p = pairs[j];
            const bool uidMatch = !seriesUid.trimmed().isEmpty()
                    && p.primarySeriesUid().trimmed() == seriesUid.trimmed();
            const bool nameDate = p.patientName.trimmed() == name.trimmed()
                    && normDate(p.studyDate) == normExamDate;
            if (uidMatch || nameDate) {
                pairs.removeAt(j);
                removedFromQueue = true;
            }
        }
        if (pairs.isEmpty()) {
            m_processingQueue.removeAt(i);
        }
    }

    auto* model = GET_SINGLETON(AppDataModel);
    if (model) {
        model->removePendingItem(name, examDate, seriesUid);
    }

    if (removedFromQueue) {
        appendPreAnalysisLog(QStringLiteral(">>> [队列] 已删除排队任务：%1 (%2)\n").arg(name, examDate));
    }

    return removedFromQueue || model != nullptr;
}

void MainViewController::testPostProcessing(const QString& outputDir, int method)
{
    qDebug() << "=== TEST POST PROCESSING ===";
    qDebug() << "OutputDir:" << outputDir;
    qDebug() << "Method:" << (method == 0 ? "fMRIPrep" : "DeepPrep");

    m_preAnalysisOutputPath = outputDir;
    m_preAnalysisMethod = method;
    setisPreAnalysisRunning(true);
    clearPreAnalysisLog();

    // 从 metadata.json 恢复 m_currentQueuePairs
    QVariantMap meta = readMetadataFile(outputDir);
    QVariantList subjects = meta.value("subjects").toList();
    m_currentQueuePairs.clear();
    for (const auto& s : subjects) {
        QVariantMap sm = s.toMap();
        MriPairResult pair;
        pair.subjectId   = sm.value("subjectId").toString();
        pair.patientName = sm.value("patientName").toString();
        pair.patientId   = sm.value("patientId").toString();
        pair.patientSex  = sm.value("patientSex").toString();
        pair.patientBirthDate = sm.value("patientBirthDate").toString();
        pair.studyDate   = sm.value("studyDate").toString();
        m_currentQueuePairs.append(pair);
    }

    if (m_currentQueuePairs.isEmpty()) {
        // 没有 metadata，自动扫描 sub-* 目录
        QString basePath = (method == 0)
            ? QDir(outputDir).filePath("sourcedata/freesurfer")
            : QDir(outputDir).filePath("Recon");
        QDir baseDir(basePath);
        QStringList subDirs = baseDir.entryList(QStringList() << "sub-*", QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& sub : subDirs) {
            MriPairResult pair;
            pair.subjectId = sub;
            pair.patientId = sub;
            pair.patientName = sub;
            m_currentQueuePairs.append(pair);
        }
    }

    m_currentProcessingPairs = m_currentQueuePairs;

    appendPreAnalysisLog(QStringLiteral("=== 测试模式：直接执行后处理 ===\n"));
    appendPreAnalysisLog(QStringLiteral("输出目录: %1\n").arg(outputDir));
    appendPreAnalysisLog(QStringLiteral("方法: %1\n").arg(method == 0 ? "fMRIPrep" : "DeepPrep"));
    appendPreAnalysisLog(QStringLiteral("被试数: %1\n\n").arg(m_currentQueuePairs.size()));

    for (const auto& p : m_currentQueuePairs) {
        appendPreAnalysisLog(QStringLiteral("  - %1 (%2)\n").arg(p.subjectId, p.patientName));
    }

    // 直接模拟 onPrepFinished(true)
    m_currentBrainRegionDone = false;
    m_currentBrainRegionSuccess = false;
    m_currentBrainNetworkDone = false;
    m_currentBrainNetworkSuccess = false;
    m_currentPrepDone = false;
    m_currentPrepSuccess = false;
    m_currentBrainAgeDone = true;    // 跳过脑龄
    m_currentBrainAgeSuccess = true;

    appendPreAnalysisLog(QStringLiteral("\n>>> 开始脑区分割后处理...\n"));
    startBrainRegionProcessing();

    appendPreAnalysisLog(QStringLiteral(">>> 开始脑网络后处理...\n"));
    startBrainNetworkPostProcessing();
}

void MainViewController::appendPreAnalysisLog(const QString& text)
{
    if (text.isEmpty())
        return;

    // 完整日志 -> LogManager（无截断）
    GET_SINGLETON(LogManager)->appendLog(text);

    m_preAnalysisLog.append(text);
    const int maxLogLength = 10000;
    if (m_preAnalysisLog.length() > maxLogLength) {
        int cutPos = m_preAnalysisLog.indexOf('\n', m_preAnalysisLog.length() - maxLogLength);
        if (cutPos > 0) {
            m_preAnalysisLog = m_preAnalysisLog.mid(cutPos + 1);
        } else {
            m_preAnalysisLog = m_preAnalysisLog.right(maxLogLength);
        }
    }
    if (!m_preAnalysisLogUpdateTimer) {
        m_preAnalysisLogUpdateTimer = new QTimer(this);
        m_preAnalysisLogUpdateTimer->setSingleShot(true);
        m_preAnalysisLogUpdateTimer->setInterval(300);
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
