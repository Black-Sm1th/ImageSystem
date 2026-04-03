#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QThread>
#include <atomic>

/**
 * @brief MRI 序列信息结构体
 */
struct MriSeriesInfo {
    QString patientId;          // 患者 ID (0010,0020)
    QString patientName;        // 患者姓名 (0010,0010)
    QString patientSex;         // 患者性别 (0010,0040)
    QString patientBirthDate;   // 出生日期 (0010,0030)
    QString studyDate;          // 检查日期 (0008,0020)
    QString studyInstanceUid;   // Study UID (0020,000D)
    QString seriesInstanceUid;  // Series UID (0020,000E)
    QString seriesDescription;  // 序列描述 (0008,103E)
    QString modality;           // 模态 (0008,0060)
    
    // Physical Parameters for Identification
    QString scanningSequence;   // (0018,0020) e.g., EP, GR, IR
    QString sequenceName;       // (0018,0024) vendor sequence name, e.g. *tfl3d1, *epfid2d1
    QString protocolName;       // (0018,1030) protocol name, often more standardized
    QString imageType;          // (0008,0008) e.g., ORIGINAL\PRIMARY\M\ND\NORM
    QString mrAcquisitionType;  // (0018,0023) "2D" or "3D"
    double repetitionTime = 0;  // TR (0018,0080) in ms
    double echoTime = 0;        // TE (0018,0081) in ms
    int numberOfFrames = 0;     // (0028,0008) for enhanced/multi-frame DICOM

    int numberOfImages = 0;     // 图像张数（文件数 or numberOfFrames）
    QString folderPath;         // 文件夹路径
    QString sourceFilePath;     // 实际用于识别的源文件
    bool isNifti = false;       // 是否为 NIfTI 文件

    // 序列类型
    enum Type { Unknown, T1W, BOLD };
    Type type = Unknown;
};
Q_DECLARE_METATYPE(MriSeriesInfo)

/**
 * @brief 配对结果结构体
 */
struct MriPairResult {
    QString patientId;
    QString patientName;
    QString patientSex;
    QString patientBirthDate;
    QString studyDate;
    QString t1SeriesUid;
    QString boldSeriesUid;

    // T1W 序列信息
    QString t1Path;
    QString t1SeriesDesc;
    int t1ImageCount = 0;

    // BOLD 序列信息
    QString boldPath;
    QString boldSeriesDesc;
    int boldImageCount = 0;

    // BIDS 转换后的受训者ID（如 sub-20260120001）
    QString subjectId;

    // 扫描模式标志
    int scanMode = 0;  // 0: 配对模式(T1W+BOLD), 1: 脑龄模式(仅T1W)

    // 预测脑龄
    double predictedBrainAge = -1.0;  // -1 表示未预测

    bool isComplete() const {
        if (scanMode == 1) {
            // 脑龄模式：只需要 T1W
            return !t1Path.isEmpty();
        }
        // 配对模式：需要 T1W 和 BOLD
        return !t1Path.isEmpty() && !boldPath.isEmpty();
    }
    QString primarySeriesUid() const { return !t1SeriesUid.isEmpty() ? t1SeriesUid : boldSeriesUid; }
};
Q_DECLARE_METATYPE(MriPairResult)
Q_DECLARE_METATYPE(QList<MriPairResult>)

/**
 * @brief 扫描进度信息
 */
struct ScanProgress {
    int totalFolders = 0;       // 总文件夹数
    int scannedFolders = 0;     // 已扫描文件夹数
    int foundT1Count = 0;       // 找到的 T1W 数量
    int foundBoldCount = 0;     // 找到的 BOLD 数量
    int pairedCount = 0;        // 配对成功数量
    QString currentFolder;      // 当前正在扫描的文件夹
    float percentage() const { return totalFolders > 0 ? (float)scannedFolders / totalFolders : 0; }
};
Q_DECLARE_METATYPE(ScanProgress)

/**
 * @brief 批量 MRI 扫描器
 *
 * 功能：
 * 1. 扫描指定文件夹下三层目录结构
 * 2. 通过 DICOM Tag 识别 T1W 和 BOLD 序列
 * 3. 自动配对同一患者的 T1W 和 BOLD（配对模式）或仅查找 T1W（脑龄模式）
 * 4. 提供实时扫描进度
 */
class BatchMriScanner : public QObject
{
    Q_OBJECT

public:
    // 扫描模式枚举
    enum ScanMode {
        PairingMode = 0,  // 配对模式：查找 T1W + BOLD
        BrainAgeMode = 1  // 脑龄模式：仅查找 T1W
    };

    explicit BatchMriScanner(QObject* parent = nullptr);
    ~BatchMriScanner();

    /**
     * @brief 开始扫描（异步，在后台线程执行）
     * @param rootPath 根目录路径
     * @param maxDepth 最大扫描深度（默认3层）
     * @param mode 扫描模式（0: 配对模式, 1: 脑龄模式）
     */
    Q_INVOKABLE void startScan(const QString& rootPath, int maxDepth = 3, int mode = 0);

    /**
     * @brief 同步扫描（阻塞，适合命令行测试）
     * @param rootPath 根目录路径
     * @param maxDepth 最大扫描深度
     * @param mode 扫描模式（0: 配对模式, 1: 脑龄模式）
     * @return 配对结果列表
     */
    QList<MriPairResult> scanSync(const QString& rootPath, int maxDepth = 3, int mode = 0);

    /**
     * @brief 停止扫描
     */
    Q_INVOKABLE void stopScan();

    /**
     * @brief 获取当前进度
     */
    ScanProgress currentProgress() const { return m_progress; }

    /**
     * @brief 获取最近一次扫描结果
     */
    QList<MriPairResult> results() const { return m_results; }

    /**
     * @brief 是否正在扫描
     */
    bool isScanning() const { return m_isScanning; }

signals:
    /**
     * @brief 进度更新信号
     * @param progress 当前进度信息
     */
    void progressUpdated(const ScanProgress& progress);

    /**
     * @brief 扫描完成信号
     * @param results 配对结果列表
     */
    void scanFinished(const QList<MriPairResult>& results);

    /**
     * @brief 发现新序列信号
     * @param info 序列信息
     */
    void seriesFound(const MriSeriesInfo& info);

    /**
     * @brief 扫描出错信号
     * @param error 错误信息
     */
    void scanError(const QString& error);

    /**
     * @brief 扫描日志信号
     * @param message 可直接展示给用户的扫描日志
     */
    void scanLog(const QString& message);

private:
    /**
     * @brief 读取单个目录的 DICOM 信息
     * @param dirPath 目录路径
     * @return 序列信息
     */
    MriSeriesInfo readDicomInfo(const QString& dirPath);
    MriSeriesInfo readNiftiInfo(const QString& dirPath);

    /**
     * @brief 识别序列类型（T1W 或 BOLD）
     * @param info 序列信息
     * @return 识别后的类型
     */
    MriSeriesInfo::Type identifySeriesType(const MriSeriesInfo& info);

    /**
     * @brief 收集所有待扫描的目录（用于计算总数）
     * @param rootPath 根目录
     * @param maxDepth 最大深度
     * @return 目录列表
     */
    QStringList collectDicomFolders(const QString& rootPath, int maxDepth);

    /**
     * @brief 执行实际扫描逻辑
     * @param rootPath 根目录
     * @param maxDepth 最大深度
     * @param mode 扫描模式
     */
    void performScan(const QString& rootPath, int maxDepth, int mode);

private:
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_isScanning{false};
    ScanProgress m_progress;
    QList<MriPairResult> m_results;
    QThread* m_workerThread = nullptr;
    int m_scanMode = 0;  // 当前扫描模式
};

