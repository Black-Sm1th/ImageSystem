#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <functional>

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkImageSlice.h>
#include <vtkTextActor.h>
#include <vtkImageData.h>
#include <vtkActor.h>
#include <vtkLookupTable.h>
#include <vtkImageMapToColors.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageBlend.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkVolume.h>

// 前置声明
struct BrainRegionMeta;

/**
 * @brief 脑区显示条目（用于可视化）
 * 继承自 BrainRegionMeta，添加可视化相关属性
 */
struct RegionEntry
{
    int label = 0;
    std::string englishName;
    std::string chineseName;
    char hemisphere = 'N';
    std::string groupKey;
    double colorR = 0.0;
    double colorG = 0.0;
    double colorB = 0.0;
    double colorA = 0.6;
    double baseOpacity = 1.0;
    bool dimmed = false;
    vtkSmartPointer<vtkActor> actor;
    double voxelCount = 0.0;
    double volume = 0.0;
    double volumePercent = 0.0;
    double asymmetryIndex = 0.0;
    int partnerLabel = -1;
    std::string stlFileName;
};

/**
 * @brief 脑区可视化类
 * 
 * 专门用于读取已处理的 STL 文件和元数据，进行 3D 可视化展示。
 * 
 * 支持两种初始化模式：
 * 1. 从已处理目录加载 (InitializeFromProcessedDir) - 推荐，速度快
 * 2. 从原始 NIfTI 文件加载 (InitializeFromNifti) - 兼容旧逻辑，较慢
 * 
 * 功能：
 * - 加载并显示脑区 3D 表面
 * - 提供轴向、冠状、矢状切片视图
 * - 支持脑区显示/隐藏、颜色标注
 * - 支持与表格数据交互
 */
class BrainRegionVisualizer
{
public:
    using ProgressCallback = std::function<void(int, const std::string&)>;

    /**
     * @brief 分割视图显示模式枚举
     */
    enum class SegDisplayMode {
        Overlay = 0,      // 叠加显示（原图+分割，默认）
        OriginalOnly = 1, // 仅原始图像
        SegmentOnly = 2   // 仅分割彩色图
    };

    /**
     * @brief 默认构造函数
     */
    BrainRegionVisualizer();

    /**
     * @brief 从已处理目录构造（推荐）
     * @param processedDir 已处理的输出目录（包含 stl/ 和 brain_regions_metadata.json）
     * @param rawNiftiPath 可选的原始 T1 NIfTI 路径（用于体渲染和切片）
     */
    BrainRegionVisualizer(const std::string& processedDir, const std::string& rawNiftiPath = "");

    /**
     * @brief 兼容旧版构造函数（从 NIfTI 初始化）
     * @param niftiPath 分割 NIfTI 文件路径
     * @param tsvPath 颜色表 TSV 文件路径
     * @param rawPath 原始 T1 NIfTI 文件路径
     * @deprecated 推荐使用 InitializeFromProcessedDir
     */
    BrainRegionVisualizer(const std::string& niftiPath, const std::string& tsvPath, const std::string& rawPath);

    ~BrainRegionVisualizer();

    /**
     * @brief 从已处理目录初始化（推荐，快速）
     * @param processedDir 已处理的输出目录
     * @param rawNiftiPath 可选的原始 T1 NIfTI 路径
     * @return 是否成功
     */
    bool InitializeFromProcessedDir(const std::string& processedDir, const std::string& rawNiftiPath = "");

    /**
     * @brief 从 NIfTI 文件初始化（兼容旧逻辑，较慢）
     * @return 是否成功
     * @deprecated 推荐使用 InitializeFromProcessedDir
     */
    bool Initialize();

    /**
     * @brief 设置进度回调
     */
    void SetProgressCallback(ProgressCallback cb);

    /**
     * @brief 设置分割 NIfTI 文件路径（用于二维切片叠加）
     * @param path 分割 NIfTI 文件路径
     */
    void SetSegmentationNiftiPath(const std::string& path) { segNiftiPath_ = path; }

    // ================== 显示模式控制 ==================

    /**
     * @brief 设置分割视图显示模式
     */
    void SetSegDisplayMode(SegDisplayMode mode);
    SegDisplayMode GetSegDisplayMode() const { return segDisplayMode_; }

    /**
     * @brief 设置分割图叠加透明度 (0.0 - 1.0)
     */
    void SetSegOverlayOpacity(double opacity);
    double GetSegOverlayOpacity() const { return segOverlayOpacity_; }

    /**
     * @brief 是否有原始图像可用
     */
    bool HasRawImage() const { return rawImageData_ != nullptr; }

    // ================== 渲染器和切片获取 ==================

    vtkSmartPointer<vtkRenderer> Get3DRenderer() const { return renderer3D_; }
    vtkSmartPointer<vtkImageSlice> GetAxialSlice() const { return axialSlice_; }
    vtkSmartPointer<vtkImageSlice> GetCoronalSlice() const { return coronalSlice_; }
    vtkSmartPointer<vtkImageSlice> GetSagittalSlice() const { return sagittalSlice_; }
    vtkSmartPointer<vtkTextActor> GetLabelTextActor() const { return labelText_; }

    // ================== 预览图路径 ==================

    const std::string& GetAxialMidPngPath() const { return axialMidPngPath_; }
    const std::string& GetCoronalMidPngPath() const { return coronalMidPngPath_; }
    const std::string& GetSagittalMidPngPath() const { return sagittalMidPngPath_; }
    const std::string& GetSeg3DPngPath() const { return seg3dPngPath_; }

    /**
     * @brief 动态生成三张"融合后中间层"像素切片 PNG
     */
    bool GenerateMidSlicePNGs(const std::string& outputDir = "");

    /**
     * @brief 动态生成一张"仅分割表面"的 3D 截图 PNG
     */
    bool GenerateSegmentation3DPng(const std::string& outputDir = "");

    // ================== 脑区数据访问 ==================

    std::vector<RegionEntry>& Regions() { return regions_; }
    const std::vector<RegionEntry>& Regions() const { return regions_; }
    std::unordered_map<vtkActor*, size_t>& ActorIndex() { return actorIndex_; }

    // ================== Actor 控制 ==================

    /**
     * @brief 设置指定脑区的透明度
     */
    bool SetActorOpacity(int label, double opacity);

    /**
     * @brief 设置指定脑区的可见性
     */
    bool SetActorVisible(int label, bool visible);

    /**
     * @brief 高亮指定脑区
     */
    bool HighlightRegion(int label, bool highlight);

    /**
     * @brief 重置所有脑区为默认状态
     */
    void ResetAllRegions();

private:
    // ================== 内部方法 ==================

    // 从 STL 文件加载
    bool LoadStlFiles(const std::string& stlDir);
    vtkSmartPointer<vtkActor> LoadStlAsActor(const std::string& stlPath, const RegionEntry& region);

    // 从 NIfTI 处理（兼容旧逻辑）
    bool LoadImage();
    bool LoadColorData();
    void ComputeLabelStatistics();
    bool BuildActors();
    vtkSmartPointer<vtkActor> CreateLabelActor(int label, double r, double g, double b, double a);

    // 通用方法
    void BuildSlices();
    void Build3DRenderer();
    void UpdateSliceInput();
    void Update3DDisplayMode();

    // 图像处理
    vtkSmartPointer<vtkImageData> ReorientToRAS(vtkImageData* input);
    bool ExportMidSlicePNGs(vtkImageData* sliceInputData, const std::string& outputDir);
    bool ExportSegmentation3DPng(const std::string& outputDir);

    // 颜色表
    struct LabelColor {
        int R = 0, G = 0, B = 0;
        std::string EnglishName;
        std::string ChineseName;
        char Hemisphere = 'N';
        std::string GroupKey;
    };
    std::unordered_map<int, LabelColor> LoadColorTable(const std::string& filename);

    // 工具方法
    std::string DeriveGroupKey(const std::string& englishName) const;
    void ReportProgress(int percent, const std::string& message);

    template<typename T>
    void AccumulateCounts(T* ptr, vtkIdType count, std::unordered_map<int, vtkIdType>& counts)
    {
        for (vtkIdType i = 0; i < count; ++i)
        {
            counts[static_cast<int>(ptr[i])] += 1;
        }
    }

private:
    // ================== 成员变量 ==================

    // 初始化模式
    enum class InitMode {
        None,
        FromProcessedDir,
        FromNifti
    };
    InitMode initMode_ = InitMode::None;

    // 路径
    std::string processedDir_;
    std::string niftiPath_;
    std::string tsvPath_;
    std::string rawNiftiPath_;
    std::string segNiftiPath_;  // 分割 NIfTI 路径（用于二维切片叠加）

    // 图像数据
    vtkSmartPointer<vtkImageData> imageData_;
    vtkSmartPointer<vtkImageData> rawImageData_;

    // 颜色表
    std::unordered_map<int, LabelColor> colorTable_;
    std::set<int> uniqueLabels_;

    // 脑区数据
    std::vector<RegionEntry> regions_;
    std::unordered_map<vtkActor*, size_t> actorIndex_;
    std::unordered_map<int, size_t> labelIndex_;

    // 体素体积
    double voxelVolume_ = 1.0;

    // 显示模式
    SegDisplayMode segDisplayMode_ = SegDisplayMode::Overlay;
    double segOverlayOpacity_ = 0.6;

    // 颜色映射
    vtkSmartPointer<vtkLookupTable> lut_;
    vtkSmartPointer<vtkImageMapToColors> colorMap_;
    vtkSmartPointer<vtkLookupTable> grayLut_;
    vtkSmartPointer<vtkImageMapToColors> grayMap_;
    vtkSmartPointer<vtkImageBlend> blendImage_;

    // 切片
    vtkSmartPointer<vtkImageSliceMapper> axialMapper_;
    vtkSmartPointer<vtkImageSliceMapper> coronalMapper_;
    vtkSmartPointer<vtkImageSliceMapper> sagittalMapper_;
    vtkSmartPointer<vtkImageSlice> axialSlice_;
    vtkSmartPointer<vtkImageSlice> coronalSlice_;
    vtkSmartPointer<vtkImageSlice> sagittalSlice_;

    // 预览图路径
    std::string axialMidPngPath_;
    std::string coronalMidPngPath_;
    std::string sagittalMidPngPath_;
    std::string seg3dPngPath_;

    // 3D 渲染
    vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper_;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty_;
    vtkSmartPointer<vtkVolume> volume_;
    vtkSmartPointer<vtkRenderer> renderer3D_;
    vtkSmartPointer<vtkTextActor> labelText_;

    // 进度回调
    ProgressCallback progressCallback_;

    // 禁止拷贝
    BrainRegionVisualizer(const BrainRegionVisualizer&) = delete;
    BrainRegionVisualizer& operator=(const BrainRegionVisualizer&) = delete;
};

// 工具函数
std::string Trim(const std::string& s);
std::vector<std::string> SplitTSVLine(const std::string& line);
