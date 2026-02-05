#include "DockerPrepRunner.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>

// ============================================================
// 构造与析构
// ============================================================

DockerPrepRunner::DockerPrepRunner(QObject* parent)
    : QObject(parent)
{
}

DockerPrepRunner::~DockerPrepRunner()
{
    stop();
}

// ============================================================
// Docker 检查
// ============================================================

QStringList DockerPrepRunner::getDockerCmd() const
{
#ifdef Q_OS_WIN
    return QStringList() << "docker";
#else
    return QStringList() << "sudo" << "docker";
#endif
}

DockerPrepRunner::DockerStatus DockerPrepRunner::checkDocker()
{
    QStringList cmd = getDockerCmd();
    cmd << "version";
    
    QProcess process;
    process.start(cmd.first(), cmd.mid(1));
    
    if (!process.waitForStarted(5000)) {
        return DockerNotFound;
    }
    
    process.waitForFinished(10000);
    
    QString stderrOutput = QString::fromUtf8(process.readAllStandardError());
    if (stderrOutput.contains("Cannot connect to the Docker daemon")) {
        return DockerNotRunning;
    }
    
    if (process.exitCode() != 0) {
        return DockerNotFound;
    }
    
    return DockerReady;
}

bool DockerPrepRunner::checkImage(const QString& imageName)
{
    QStringList cmd = getDockerCmd();
    cmd << "images" << "-q" << imageName;
    
    QProcess process;
    process.start(cmd.first(), cmd.mid(1));
    process.waitForFinished(30000);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return !output.isEmpty();
}

QString DockerPrepRunner::getDockerVersion()
{
    QStringList cmd = getDockerCmd();
    cmd << "version" << "--format" << "{{.Server.Version}}";
    
    QProcess process;
    process.start(cmd.first(), cmd.mid(1));
    process.waitForFinished(10000);
    
    if (process.exitCode() == 0) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    return QString();
}

// ============================================================
// BIDS 操作
// ============================================================

QStringList DockerPrepRunner::scanBidsSubjects(const QString& bidsDir)
{
    QStringList subjects;
    QDir dir(bidsDir);
    
    if (!dir.exists()) {
        qWarning() << "BIDS directory not found:" << bidsDir;
        return subjects;
    }
    
    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        if (entry.startsWith("sub-")) {
            subjects.append(entry);
        }
    }
    
    return subjects;
}

BidsValidationResult DockerPrepRunner::validateBidsStructure(const QString& bidsDir)
{
    BidsValidationResult result;
    QDir dir(bidsDir);
    
    if (!dir.exists()) {
        result.message = QString("BIDS directory not found: %1").arg(bidsDir);
        return result;
    }
    
    // 检查 dataset_description.json（可选但推荐）
    if (!QFileInfo::exists(dir.filePath("dataset_description.json"))) {
        qWarning() << "Warning: dataset_description.json not found (optional but recommended)";
    }
    
    // 扫描被试
    QStringList allSubjects = scanBidsSubjects(bidsDir);
    
    if (allSubjects.isEmpty()) {
        result.message = "No subjects (sub-*) found in BIDS directory";
        return result;
    }
    
    // 验证每个被试是否有 T1w 文件
    QStringList validSubjects;
    for (const QString& sub : allSubjects) {
        QString anatPath = dir.filePath(sub + "/anat");
        QDir anatDir(anatPath);
        
        if (anatDir.exists()) {
            QStringList t1Files = anatDir.entryList(QStringList() << "*_T1w.nii.gz", QDir::Files);
            if (!t1Files.isEmpty()) {
                validSubjects.append(sub);
            } else {
                qWarning() << "Warning:" << sub << "has no T1w file, will be skipped";
            }
        } else {
            qWarning() << "Warning:" << sub << "has no anat folder, will be skipped";
        }
    }
    
    if (validSubjects.isEmpty()) {
        result.message = "No valid subjects with T1w files found";
        return result;
    }
    
    result.isValid = true;
    result.message = QString("Found %1 valid subjects with T1w").arg(validSubjects.size());
    result.subjectCount = validSubjects.size();
    result.subjects = validSubjects;
    
    return result;
}

QStringList DockerPrepRunner::readParticipantsTsv(const QString& bidsDir)
{
    QStringList subjects;
    QString tsvPath = QDir(bidsDir).filePath("participants.tsv");
    
    QFile file(tsvPath);
    if (!file.exists()) {
        qDebug() << "participants.tsv not found";
        return subjects;
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open participants.tsv";
        return subjects;
    }
    
    QTextStream in(&file);
    QString header = in.readLine();
    
    // 查找 participant_id 列索引
    QStringList headers = header.split('\t');
    int participantIdx = headers.indexOf("participant_id");
    
    if (participantIdx < 0) {
        qWarning() << "participants.tsv has no 'participant_id' column";
        file.close();
        return subjects;
    }
    
    // 读取所有被试
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QStringList cols = line.split('\t');
        if (cols.size() > participantIdx) {
            QString subId = cols[participantIdx].trimmed();
            if (!subId.isEmpty()) {
                // 确保以 sub- 开头
                if (!subId.startsWith("sub-")) {
                    subId = "sub-" + subId;
                }
                if (!subjects.contains(subId)) {
                    subjects.append(subId);
                }
            }
        }
    }
    
    file.close();
    return subjects;
}

QStringList DockerPrepRunner::determineSubjects(const QString& bidsDir, const QStringList& specifiedSubjects)
{
    QStringList allSubjects = scanBidsSubjects(bidsDir);
    
    // 如果命令行指定了被试，使用命令行参数
    if (!specifiedSubjects.isEmpty()) {
        QStringList valid;
        for (const QString& s : specifiedSubjects) {
            if (allSubjects.contains(s)) {
                valid.append(s);
            } else {
                qWarning() << "Subject not found in BIDS:" << s;
            }
        }
        return valid;
    }
    
    // 尝试从 participants.tsv 读取
    QStringList tsvSubjects = readParticipantsTsv(bidsDir);
    if (!tsvSubjects.isEmpty()) {
        QStringList valid;
        for (const QString& s : tsvSubjects) {
            if (allSubjects.contains(s)) {
                valid.append(s);
            }
        }
        if (!valid.isEmpty()) {
            qDebug() << "Using subjects from participants.tsv:" << valid.size();
            return valid;
        }
    }
    
    // 返回空列表表示处理所有被试
    return QStringList();
}

// ============================================================
// 构建 Docker 命令
// ============================================================

QStringList DockerPrepRunner::buildDeepPrepCommand(const DeepPrepParams& params)
{
    QStringList command = getDockerCmd();
    
    QString dockerVersion = getDockerVersion();
    
    command << "run" << "--rm" << "--gpus" << "all";
    command << "-e" << QString("DOCKER_VERSION_8395080871=%1").arg(dockerVersion);
    
    // 挂载目录
    QString absBids = QFileInfo(params.bidsDir).absoluteFilePath();
    QString absOutput = QFileInfo(params.outputDir).absoluteFilePath();
    
    // 确保输出目录存在
    QDir().mkpath(absOutput);
    
    command << "-v" << QString("%1:/input").arg(absBids);
    command << "-v" << QString("%1:/output").arg(absOutput);
    
    // 挂载许可证文件
    QString absLicense = QFileInfo(params.licenseFile).absoluteFilePath();
    command << "-v" << QString("%1:/fs_license.txt:ro").arg(absLicense);
    
    // 镜像和参数
    command << params.image;
    command << "/input" << "/output" << "participant";
    command << "--fs_license_file" << "/fs_license.txt";
    command << "--bold_task_type" << params.boldTaskType;
    command << "--bold_sdc" << (params.boldSdc ? "true" : "false");
    command << "--device" << params.device;
    
    // 被试参数
    if (!params.subjects.isEmpty()) {
        QStringList labels;
        for (const QString& s : params.subjects) {
            QString label = s;
            label.remove("sub-");
            labels.append(label.trimmed());
        }
        command << "--participant_label" << labels.join(" ");
    }
    
    if (params.skipBidsValidation) {
        command << "--skip_bids_validation";
    }
    
    if (params.anatOnly) {
        command << "--anat_only";
    }
    
    if (params.boldOnly) {
        command << "--bold_only";
    }
    
    if (params.resume) {
        command << "--resume";
    }
    
    return command;
}

QStringList DockerPrepRunner::buildFmriPrepCommand(const FmriPrepParams& params)
{
    QStringList command = getDockerCmd();
    
    QString dockerVersion = getDockerVersion();
    
    command << "run" << "--rm";
    command << "-e" << QString("DOCKER_VERSION_8395080871=%1").arg(dockerVersion);
    
    // 挂载目录
    QString absBids = QFileInfo(params.bidsDir).absoluteFilePath();
    QString absOutput = QFileInfo(params.outputDir).absoluteFilePath();
    QString workDir = QDir(absOutput).filePath("work");
    
    // 确保目录存在
    QDir().mkpath(absOutput);
    QDir().mkpath(workDir);
    
    command << "-v" << QString("%1:/data:ro").arg(absBids);
    command << "-v" << QString("%1:/out").arg(absOutput);
    command << "-v" << QString("%1:/work").arg(workDir);
    
    // 挂载许可证文件
    QString absLicense = QFileInfo(params.licenseFile).absoluteFilePath();
    command << "-v" << QString("%1:/opt/freesurfer/license.txt:ro").arg(absLicense);
    
    // 镜像和参数
    command << params.image;
    command << "/data" << "/out" << "participant";
    command << "-w" << "/work";
    
    // 被试参数
    if (!params.subjects.isEmpty()) {
        command << "--participant-label";
        for (const QString& s : params.subjects) {
            QString label = s;
            label.remove("sub-");
            command << label.trimmed();
        }
    }
    
    if (params.skipBidsValidation) {
        command << "--skip-bids-validation";
    }
    
    if (params.anatOnly) {
        command << "--anat-only";
    }
    
    if (params.fsNoReconall) {
        command << "--fs-no-reconall";
    }
    
    if (params.ignoreFieldmaps) {
        command << "--ignore" << "fieldmaps";
    }
    
    if (params.useSynSdc) {
        command << "--use-syn-sdc";
    }
    
    if (!params.outputSpaces.isEmpty()) {
        command << "--output-spaces" << params.outputSpaces;
    }
    
    if (params.nthreads > 0) {
        command << "--nthreads" << QString::number(params.nthreads);
    }
    
    if (params.memMb > 0) {
        command << "--mem-mb" << QString::number(params.memMb);
    }
    
    if (params.lowMem) {
        command << "--low-mem";
    }
    
    return command;
}

// ============================================================
// 异步执行 Docker 命令
// ============================================================

void DockerPrepRunner::startAsyncProcess(const QStringList& command, const QString& logPath, bool isDeepPrep)
{
    if (command.isEmpty()) {
        emit runError("Empty command");
        return;
    }
    
    m_isDeepPrep = isDeepPrep;
    
    qDebug() << "Executing Docker command:" << command.join(" ");
    
    // 创建进程
    m_process = new QProcess(this);
    
    // 打开日志文件
    m_logFile = new QFile(logPath, this);
    m_logFile->open(QIODevice::WriteOnly | QIODevice::Text);
    
    // 连接输出信号 - 实时写入日志并发送信号
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (!m_process) return;
        QByteArray data = m_process->readAllStandardOutput();
        if (m_logFile && m_logFile->isOpen()) {
            m_logFile->write(data);
            m_logFile->flush();
        }
        emit outputLog(QString::fromUtf8(data));
    });
    
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        if (!m_process) return;
        QByteArray data = m_process->readAllStandardError();
        if (m_logFile && m_logFile->isOpen()) {
            m_logFile->write(data);
            m_logFile->flush();
        }
        emit outputLog(QString::fromUtf8(data));
    });
    
    // 连接完成信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        
        // 关闭日志文件
        if (m_logFile) {
            m_logFile->close();
            m_logFile->deleteLater();
            m_logFile = nullptr;
        }
        
        m_isRunning = false;
        
        QString message = exitCode == 0 
            ? (m_isDeepPrep ? "DeepPrep completed successfully" : "fMRIPrep completed successfully")
            : QString("%1 exited with code %2").arg(m_isDeepPrep ? "DeepPrep" : "fMRIPrep").arg(exitCode);
        
        // 发送对应的完成信号
        if (m_isDeepPrep) {
            emit deepPrepFinished(exitCode, message);
        } else {
            emit fmriPrepFinished(exitCode, message);
        }
        
        // 清理进程
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
    });
    
    // 连接错误信号
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        QString errorMsg;
        switch (error) {
            case QProcess::FailedToStart:
                errorMsg = "Failed to start Docker process";
                break;
            case QProcess::Crashed:
                errorMsg = "Docker process crashed";
                break;
            case QProcess::Timedout:
                errorMsg = "Docker process timed out";
                break;
            default:
                errorMsg = m_process ? m_process->errorString() : "Unknown error";
                break;
        }
        
        emit runError(errorMsg);
        
        // 关闭日志文件
        if (m_logFile) {
            m_logFile->close();
            m_logFile->deleteLater();
            m_logFile = nullptr;
        }
        
        m_isRunning = false;
    });
    
    m_isRunning = true;
    
    // 异步启动进程
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start(command.first(), command.mid(1));
}

// ============================================================
// 运行 DeepPrep
// ============================================================

void DockerPrepRunner::runDeepPrep(const DeepPrepParams& params)
{
    if (m_isRunning) {
        emit runError("Another process is already running");
        return;
    }
    
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "DeepPrep Docker Runner";
    qDebug() << QString("=").repeated(60);
    
    // 检查 Docker
    DockerStatus status = checkDocker();
    if (status != DockerReady) {
        QString error = status == DockerNotFound 
            ? "Docker command not found. Is Docker installed?"
            : "Cannot connect to Docker daemon. Is it running?";
        emit runError(error);
        return;
    }
    
    // 检查许可证文件
    if (!QFileInfo::exists(params.licenseFile)) {
        emit runError(QString("FreeSurfer license file not found: %1").arg(params.licenseFile));
        return;
    }
    
    // 验证 BIDS（可选，如果 skipBidsValidation 为 false）
    if (!params.skipBidsValidation) {
        BidsValidationResult validation = validateBidsStructure(params.bidsDir);
        if (!validation.isValid) {
            emit runError(validation.message);
            return;
        }
        qDebug() << validation.message;
    }
    
    // 确定被试列表
    QStringList subjects = determineSubjects(params.bidsDir, params.subjects);
    
    // 构建命令
    DeepPrepParams finalParams = params;
    finalParams.subjects = subjects;
    
    QStringList command = buildDeepPrepCommand(finalParams);
    
    // 日志路径
    QString logPath = QDir(params.outputDir).filePath("deepprep-docker.log");
    
    qDebug() << "\nBIDS Input :" << QFileInfo(params.bidsDir).absoluteFilePath();
    qDebug() << "Output     :" << QFileInfo(params.outputDir).absoluteFilePath();
    qDebug() << "Log File   :" << logPath;
    qDebug() << "Task Type  :" << params.boldTaskType;
    qDebug() << "SDC        :" << params.boldSdc;
    if (!subjects.isEmpty()) {
        qDebug() << "Subjects   :" << subjects.join(", ") << QString("(%1)").arg(subjects.size());
    } else {
        qDebug() << "Subjects   : ALL in BIDS directory";
    }
    
    qDebug() << "\n" << QString("-").repeated(60);
    qDebug() << "Full Docker Command:";
    qDebug() << command.join(" ");
    qDebug() << QString("-").repeated(60) << "\n";
    
    qDebug() << "Starting DeepPrep... (may take hours per subject)";
    
    // 异步启动
    startAsyncProcess(command, logPath, true);
}

// ============================================================
// 运行 fMRIPrep
// ============================================================

void DockerPrepRunner::runFmriPrep(const FmriPrepParams& params)
{
    if (m_isRunning) {
        emit runError("Another process is already running");
        return;
    }
    
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "fMRIPrep Docker Runner";
    qDebug() << QString("=").repeated(60);
    
    // 检查 Docker
    DockerStatus status = checkDocker();
    if (status != DockerReady) {
        QString error = status == DockerNotFound 
            ? "Docker command not found. Is Docker installed?"
            : "Cannot connect to Docker daemon. Is it running?";
        emit runError(error);
        return;
    }
    
    // 检查许可证文件
    if (!QFileInfo::exists(params.licenseFile)) {
        emit runError(QString("FreeSurfer license file not found: %1").arg(params.licenseFile));
        return;
    }
    
    // 验证 BIDS（可选）
    if (!params.skipBidsValidation) {
        BidsValidationResult validation = validateBidsStructure(params.bidsDir);
        if (!validation.isValid) {
            emit runError(validation.message);
            return;
        }
        qDebug() << validation.message;
    }
    
    // 确定被试列表
    QStringList subjects = determineSubjects(params.bidsDir, params.subjects);
    
    // 构建命令
    FmriPrepParams finalParams = params;
    finalParams.subjects = subjects;
    
    QStringList command = buildFmriPrepCommand(finalParams);
    
    // 日志路径
    QString logPath = QDir(params.outputDir).filePath("fmriprep-docker.log");
    QString workDir = QDir(params.outputDir).filePath("work");
    
    qDebug() << "\nBIDS Input :" << QFileInfo(params.bidsDir).absoluteFilePath();
    qDebug() << "Output     :" << QFileInfo(params.outputDir).absoluteFilePath();
    qDebug() << "Work Dir   :" << workDir;
    qDebug() << "Log File   :" << logPath;
    qDebug() << "Anat Only  :" << params.anatOnly;
    qDebug() << "FS Reconall:" << !params.fsNoReconall;
    
    QString sdcMode = params.useSynSdc ? "syn-sdc" : (params.ignoreFieldmaps ? "disabled" : "fieldmaps");
    qDebug() << "SDC        :" << sdcMode;
    
    if (!subjects.isEmpty()) {
        qDebug() << "Subjects   :" << subjects.join(", ") << QString("(%1)").arg(subjects.size());
    } else {
        qDebug() << "Subjects   : ALL in BIDS directory";
    }
    
    qDebug() << "\n" << QString("-").repeated(60);
    qDebug() << "Full Docker Command:";
    qDebug() << command.join(" ");
    qDebug() << QString("-").repeated(60) << "\n";
    
    qDebug() << "Starting fMRIPrep... (may take hours per subject)";
    
    // 异步启动
    startAsyncProcess(command, logPath, false);
}

// ============================================================
// 停止
// ============================================================

void DockerPrepRunner::stop()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // 断开所有信号连接，避免手动停止时触发回调
        disconnect(m_process, nullptr, this, nullptr);
        
        m_process->terminate();
        if (!m_process->waitForFinished(5000)) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        
        // 清理进程对象
        m_process->deleteLater();
        m_process = nullptr;
    }
    
    if (m_logFile) {
        m_logFile->close();
        m_logFile->deleteLater();
        m_logFile = nullptr;
    }
    
    m_isRunning = false;
}

