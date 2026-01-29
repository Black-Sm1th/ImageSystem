#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

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
 * 用于运行 DeepPrep 和 fMRIPrep Docker 容器
 * 支持：
 * - 检查 Docker 环境
 * - 验证 BIDS 数据结构
 * - 读取 participants.tsv
 * - 异步运行 Docker 容器
 */
class DockerPrepRunner : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Docker 状态枚举
     */
    enum DockerStatus {
        DockerNotFound = -1,    // Docker 未安装
        DockerNotRunning = 0,   // Docker 守护进程未运行
        DockerReady = 1         // Docker 就绪
    };
    Q_ENUM(DockerStatus)

    explicit DockerPrepRunner(QObject* parent = nullptr);
    ~DockerPrepRunner();

    // ================== Docker 检查 ==================
    
    /**
     * @brief 检查 Docker 是否可用
     * @return Docker 状态
     */
    DockerStatus checkDocker();

    /**
     * @brief 检查镜像是否存在
     * @param imageName 镜像名称
     * @return 是否存在
     */
    bool checkImage(const QString& imageName);

    /**
     * @brief 获取 Docker 版本
     * @return 版本字符串，失败返回空
     */
    QString getDockerVersion();

    // ================== BIDS 操作 ==================
    
    /**
     * @brief 扫描 BIDS 目录中的被试
     * @param bidsDir BIDS 目录路径
     * @return 被试 ID 列表（带 sub- 前缀）
     */
    QStringList scanBidsSubjects(const QString& bidsDir);

    /**
     * @brief 验证 BIDS 数据结构
     * @param bidsDir BIDS 目录路径
     * @return 验证结果
     */
    BidsValidationResult validateBidsStructure(const QString& bidsDir);

    /**
     * @brief 从 participants.tsv 读取被试列表
     * @param bidsDir BIDS 目录路径
     * @return 被试 ID 列表（带 sub- 前缀），失败返回空列表
     */
    QStringList readParticipantsTsv(const QString& bidsDir);

    // ================== 运行 Docker ==================
    
    /**
     * @brief 异步运行 DeepPrep Docker
     * @param params 运行参数
     * 
     * 结果通过 deepPrepFinished 信号返回
     */
    void runDeepPrep(const DeepPrepParams& params);

    /**
     * @brief 异步运行 fMRIPrep Docker
     * @param params 运行参数
     * 
     * 结果通过 fmriPrepFinished 信号返回
     */
    void runFmriPrep(const FmriPrepParams& params);

    /**
     * @brief 停止当前运行的 Docker 容器
     */
    void stop();

    /**
     * @brief 是否正在运行
     */
    bool isRunning() const { return m_isRunning; }

signals:
    /**
     * @brief DeepPrep 完成信号
     * @param exitCode 退出码（0表示成功）
     * @param message 完成消息
     */
    void deepPrepFinished(int exitCode, const QString& message);

    /**
     * @brief fMRIPrep 完成信号
     * @param exitCode 退出码（0表示成功）
     * @param message 完成消息
     */
    void fmriPrepFinished(int exitCode, const QString& message);

    /**
     * @brief 输出日志信号
     * @param line 日志行
     */
    void outputLog(const QString& line);

    /**
     * @brief 运行出错信号
     * @param error 错误信息
     */
    void runError(const QString& error);

private:
    /**
     * @brief 获取 Docker 命令（Windows 不需要 sudo）
     * @return 命令列表
     */
    QStringList getDockerCmd() const;

    /**
     * @brief 构建 DeepPrep Docker 命令
     * @param params 运行参数
     * @return 完整命令列表
     */
    QStringList buildDeepPrepCommand(const DeepPrepParams& params);

    /**
     * @brief 构建 fMRIPrep Docker 命令
     * @param params 运行参数
     * @return 完整命令列表
     */
    QStringList buildFmriPrepCommand(const FmriPrepParams& params);

    /**
     * @brief 启动异步 Docker 进程
     * @param command 命令列表
     * @param logPath 日志文件路径
     * @param isDeepPrep 是否为 DeepPrep（用于区分完成信号）
     */
    void startAsyncProcess(const QStringList& command, const QString& logPath, bool isDeepPrep);

    /**
     * @brief 确定要处理的被试列表
     * @param bidsDir BIDS 目录
     * @param specifiedSubjects 命令行指定的被试
     * @return 最终被试列表
     */
    QStringList determineSubjects(const QString& bidsDir, const QStringList& specifiedSubjects);

private:
    QPointer<QProcess> m_process;
    QFile* m_logFile = nullptr;
    bool m_isRunning = false;
    bool m_isDeepPrep = false;  // 标识当前运行的是 DeepPrep 还是 fMRIPrep
};

