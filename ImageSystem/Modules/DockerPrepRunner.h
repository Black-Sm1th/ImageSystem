#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <functional>

/**
 * @brief DeepPrep 运行参数结构体
 */
struct DeepPrepParams {
    QString bidsDir;                    // BIDS 数据目录
    QString outputDir;                  // 输出目录
    QString licenseFile;                // FreeSurfer 许可证文件
    QString image = "pbfslab/deepprep:25.1.1.cuda129";  // Docker 镜像
    QString boldTaskType = "rest";      // BOLD 任务类型
    bool boldSdc = true;                // 是否启用 SDC
    QStringList subjects;               // 指定被试列表（空则处理全部）
    bool skipBidsValidation = false;    // 跳过 BIDS 验证
    bool anatOnly = false;              // 仅处理解剖图像
    bool boldOnly = false;              // 仅处理功能图像
    QString device = "auto";            // 设备: auto, 0, 1, ..., cpu
    bool resume = false;                // 从上次中断处继续
};
Q_DECLARE_METATYPE(DeepPrepParams)

/**
 * @brief fMRIPrep 运行参数结构体
 */
struct FmriPrepParams {
    QString bidsDir;                    // BIDS 数据目录
    QString outputDir;                  // 输出目录
    QString licenseFile;                // FreeSurfer 许可证文件
    QString image = "nipreps/fmriprep:latest";  // Docker 镜像
    QStringList subjects;               // 指定被试列表（空则处理全部）
    bool skipBidsValidation = false;    // 跳过 BIDS 验证
    bool anatOnly = false;              // 仅处理解剖图像
    bool useSynSdc = false;             // 使用无场图 SyN-based SDC
    bool ignoreFieldmaps = true;        // 忽略场图
    QString outputSpaces = "MNI152NLin2009cAsym:res-2";  // 输出空间
    int nthreads = 0;                   // 最大线程数（0表示不限制）
    int memMb = 0;                      // 最大内存MB（0表示不限制）
    bool lowMem = false;                // 低内存模式
    bool fsNoReconall = true;           // 禁用 FreeSurfer 表面重建
};
Q_DECLARE_METATYPE(FmriPrepParams)

/**
 * @brief BIDS 验证结果结构体
 */
struct BidsValidationResult {
    bool isValid = false;
    QString message;
    int subjectCount = 0;
    QStringList subjects;               // 有效被试列表
};
Q_DECLARE_METATYPE(BidsValidationResult)

/**
 * @brief Docker 预处理运行器
 * 
 * 用于运行 DeepPrep / fMRIPrep / Deface / BAP Docker 容器
 */
class DockerPrepRunner : public QObject
{
    Q_OBJECT

public:
    enum DockerStatus {
        DockerNotFound = -1,
        DockerNotRunning = 0,
        DockerReady = 1
    };
    Q_ENUM(DockerStatus)

    explicit DockerPrepRunner(QObject* parent = nullptr);
    ~DockerPrepRunner();

    // ================== Docker 检查 ==================
    DockerStatus checkDocker();
    bool checkImage(const QString& imageName);
    QString getDockerVersion();

    // ================== BIDS 操作 ==================
    QStringList scanBidsSubjects(const QString& bidsDir);
    BidsValidationResult validateBidsStructure(const QString& bidsDir);
    QStringList readParticipantsTsv(const QString& bidsDir);

    // ================== 运行 Docker ==================
    void runDeepPrep(const DeepPrepParams& params);
    void runFmriPrep(const FmriPrepParams& params);

    // 新增：Deface 和 BAP Docker
    void runDeface(const QString& bidsDir, const QStringList& subjects = {});
    void runBap(const QString& bidsDir, const QStringList& subjects = {});
    void stopDeface();
    void stopBap();

    void stop();
    bool isRunning() const { return m_isRunning; }

    // bap_subjects.txt：相对 BIDS 根 sub-XXX/anat/xxx.nii.gz（deface 用原始；BAP 前改为 *_defaced.nii.gz）
    static bool generateDefaceSubjectsTxt(const QString& bidsDir, const QStringList& subjects = {});
    static bool generateBapSubjectsTxtDefaced(const QString& bidsDir, const QStringList& subjects = {});

signals:
    void deepPrepFinished(int exitCode, const QString& message);
    void fmriPrepFinished(int exitCode, const QString& message);
    void defaceFinished(int exitCode, const QString& message);
    void bapFinished(int exitCode, const QString& message);
    void outputLog(const QString& line);
    void runError(const QString& error);

private:
    QStringList getDockerCmd() const;
    QStringList buildDeepPrepCommand(const DeepPrepParams& params);
    QStringList buildFmriPrepCommand(const FmriPrepParams& params);
    void startAsyncProcess(const QStringList& command, const QString& logPath, bool isDeepPrep);
    void startDetachedDockerProcess(QPointer<QProcess>& proc, const QStringList& command,
                                    const char* taskName,
                                    std::function<void(int, const QString&)> finishedCb);
    QStringList determineSubjects(const QString& bidsDir, const QStringList& specifiedSubjects);
    void configureProcess(QProcess* process) const;

private:
    QPointer<QProcess> m_process;
    QPointer<QProcess> m_defaceProcess;
    QPointer<QProcess> m_bapProcess;
    QFile* m_logFile = nullptr;
    bool m_isRunning = false;
    bool m_isDeepPrep = false;
    QString m_lastProcessOutput;
};
