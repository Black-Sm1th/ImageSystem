#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <functional>
#include <vector>
#include <map>
#include <memory>

// VTK 前置声明
class vtkImageData;
class vtkPolyData;
class BrainRegionVisualizer;

/**
 * @brief 脑区元数据结构体
 * 包含单个脑区的统计和标识信息
 */
struct BrainRegionMeta {
    int label = 0;                      // 标签值
    QString englishName;                // 英文名称
    QString chineseName;                // 中文名称
    char hemisphere = 'N';              // 半球: 'L', 'R', 'N'
    QString groupKey;                   // 分组键
    double colorR = 0.5;                // 颜色 R (0-1)
    double colorG = 0.5;                // 颜色 G (0-1)
    double colorB = 0.5;                // 颜色 B (0-1)
    double colorA = 0.6;                // 透明度 (0-1)
    double voxelCount = 0.0;            // 体素数量
    double volume = 0.0;                // 体积 (mm³)
    double volumePercent = 0.0;         // 体积占比 (%)
    double asymmetryIndex = 0.0;        // 不对称性指数 (%)
    int partnerLabel = -1;              // 配对脑区标签
    QString stlFileName;                // STL 文件名
};

/**
 * @brief 处理结果结构体
 */
struct ProcessingResult {
    bool success = false;
    QString message;
    QString outputDir;
    QString metadataPath;               // brain_regions_metadata.json
    QString processingInfoPath;         // processing_info.json
    int regionCount = 0;
    int stlFileCount = 0;
};

/**
 * @brief 脑区数据处理类
 * 
 * 负责从 fMRIPrep/DeepPrep 输出中提取脑区分割数据，
 * 生成 STL 网格文件和统计元数据。
 * 
 * 输出目录结构:
 * output_dir/
 * ├── stl/
 * │   ├── region_001_left_hippocampus.stl
 * │   └── ...
 * ├── slices/
 * │   ├── axial_mid.png
 * │   ├── coronal_mid.png
 * │   └── sagittal_mid.png
 * ├── brain_regions_metadata.json
 * └── processing_info.json
 */
class BrainRegionProcessor : public QObject
{
    Q_OBJECT

public:
    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    explicit BrainRegionProcessor(QObject* parent = nullptr);
    ~BrainRegionProcessor();

    /**
     * @brief 设置进度回调
     * @param callback 回调函数
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief 同步处理单个被试的分割数据
     * @param segNiftiPath 分割 NIfTI 文件路径 (e.g., aparc+aseg.nii.gz)
     * @param rawNiftiPath 原始 T1 NIfTI 文件路径 (可选，用于切片预览)
     * @param colorTablePath 颜色表 TSV 文件路径
     * @param outputDir 输出目录
     * @return 处理结果
     */
    ProcessingResult process(
        const QString& segNiftiPath,
        const QString& rawNiftiPath,
        const QString& colorTablePath,
        const QString& outputDir
    );

    /**
     * @brief 异步处理单个被试的分割数据
     * 结果通过 processFinished 信号返回
     */
    void processAsync(
        const QString& segNiftiPath,
        const QString& rawNiftiPath,
        const QString& colorTablePath,
        const QString& outputDir
    );

    /**
     * @brief 批量异步处理多个被试
     * @param subjects 被试列表 [(segPath, rawPath, outputSubDir), ...]
     * @param colorTablePath 共享的颜色表路径
     * @param baseOutputDir 基础输出目录
     */
    void processBatchAsync(
        const QList<std::tuple<QString, QString, QString>>& subjects,
        const QString& colorTablePath,
        const QString& baseOutputDir
    );

    /**
     * @brief 检查输出目录是否已包含处理结果
     * @param outputDir 输出目录
     * @return 是否已处理
     */
    static bool isAlreadyProcessed(const QString& outputDir);

    /**
     * @brief 读取已处理的元数据
     * @param outputDir 输出目录
     * @return 脑区元数据列表，失败返回空列表
     */
    static std::vector<BrainRegionMeta> loadMetadata(const QString& outputDir);

    /**
     * @brief 获取 STL 文件目录路径
     */
    static QString getStlDir(const QString& outputDir);

    /**
     * @brief 取消当前处理
     */
    void cancel();

    /**
     * @brief 是否正在处理
     */
    bool isProcessing() const { return m_isProcessing; }

signals:
    /**
     * @brief 单个被试处理完成
     * @param result 处理结果
     */
    void processFinished(const ProcessingResult& result);

    /**
     * @brief 批量处理进度
     * @param current 当前已完成数量
     * @param total 总数量
     * @param currentSubject 当前被试名称
     */
    void batchProgress(int current, int total, const QString& currentSubject);

    /**
     * @brief 批量处理全部完成
     * @param successCount 成功数量
     * @param failCount 失败数量
     */
    void batchFinished(int successCount, int failCount);

    /**
     * @brief 处理错误
     * @param error 错误信息
     */
    void processError(const QString& error);

private:
    // ================== 内部处理方法 ==================
    
    /**
     * @brief 加载 NIfTI 图像
     */
    bool loadNiftiImage(const QString& path, vtkImageData*& imageData);

    /**
     * @brief 加载颜色表
     */
    bool loadColorTable(const QString& tsvPath);

    /**
     * @brief 计算脑区统计信息
     */
    void computeRegionStatistics(vtkImageData* imageData);

    /**
     * @brief 生成单个脑区的 STL 文件
     * @param label 标签值
     * @param outputPath STL 输出路径
     * @return 是否成功
     */
    bool generateStlFile(int label, const QString& outputPath);

    /**
     * @brief 生成所有脑区的 STL 文件
     */
    bool generateAllStlFiles(const QString& stlDir);

    /**
     * @brief 保存元数据 JSON
     */
    bool saveMetadataJson(const QString& outputDir);

    /**
     * @brief 保存处理信息 JSON
     */
    bool saveProcessingInfo(const QString& outputDir, const QString& segPath, const QString& rawPath);

    /**
     * @brief 生成脑分割预览图（3 个切面 + 1 张 3D 图）
     */
    bool generatePreviewImages(const QString& segPath, const QString& rawPath, const QString& outputDir);

    /**
     * @brief 清理文件名中的非法字符
     */
    QString sanitizeFileName(const QString& name) const;

    /**
     * @brief 从英文名推导分组键
     */
    QString deriveGroupKey(const QString& englishName) const;

    /**
     * @brief 重定向图像到 RAS 坐标系
     */
    vtkImageData* reorientToRAS(vtkImageData* input);

    /**
     * @brief 报告进度
     */
    void reportProgress(int percent, const QString& message);

private:
    // ================== 成员变量 ==================
    
    ProgressCallback m_progressCallback;
    bool m_isProcessing = false;
    bool m_cancelRequested = false;

    // 图像数据
    vtkImageData* m_segImageData = nullptr;
    vtkImageData* m_rawImageData = nullptr;
    
    // 颜色表结构
    struct LabelColor {
        int R = 0, G = 0, B = 0;
        QString englishName;
        QString chineseName;
        char hemisphere = 'N';
        QString groupKey;
    };
    std::map<int, LabelColor> m_colorTable;

    // 脑区元数据
    std::vector<BrainRegionMeta> m_regions;
    std::map<int, size_t> m_labelIndex;  // label -> regions_ 索引
    
    // 体素体积
    double m_voxelVolume = 1.0;

    // 工作线程
    QThread* m_workerThread = nullptr;
};

// 声明元类型以便在信号槽中使用
Q_DECLARE_METATYPE(ProcessingResult)
Q_DECLARE_METATYPE(BrainRegionMeta)

