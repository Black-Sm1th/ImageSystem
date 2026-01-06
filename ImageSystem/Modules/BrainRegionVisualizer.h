#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkImageSlice.h>
#include <vtkTextActor.h>

#include <vtkAutoInit.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkType.h>
#include <vtkImageThreshold.h>
#include <vtkImageDilateErode3D.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPointData.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkLookupTable.h>
#include <vtkImageMapToColors.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageBlend.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkVolume.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkPropPicker.h>
#include <vtkTextProperty.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <functional>
#include <vtkNIFTIImageReader.h>

// 定义结构体和类
struct LabelColor
{
    int R{ 0 };
    int G{ 0 };
    int B{ 0 };
    std::string EnglishName;
    std::string ChineseName;
    char Hemisphere{ 'N' };
    std::string GroupKey;
};

struct LabelStyle
{
    double R{ 0.5 };
    double G{ 0.5 };
    double B{ 0.5 };
    double A{ 0.6 };
    std::string EnglishName;
    std::string ChineseName;
    char Hemisphere{ 'N' };
    std::string GroupKey;
};

struct RegionEntry
{
    int label{ 0 };
    std::string englishName;
    std::string chineseName;
    char hemisphere{ 'N' };
    std::string groupKey;
    double colorR{ 0.0 };
    double colorG{ 0.0 };
    double colorB{ 0.0 };
    double colorA{ 0.0 };   
    double baseOpacity{ 1.0 };
    bool dimmed{ false };
    vtkSmartPointer<vtkActor> actor;
    double voxelCount{ 0.0 };
    double volume{ 0.0 };
    double volumePercent{ 0.0 };
    double asymmetryIndex{ 0.0 };
    int partnerLabel{ -1 };
};

// 前置声明，减少头文件依赖
struct LabelColor;
struct LabelStyle;
struct RegionEntry;
class LabelPipeline;

class BrainRegionVisualizer
{
public:
    using ProgressCallback = std::function<void(int, const std::string&)>;

    // 分割视图显示模式枚举
    enum class SegDisplayMode {
        Overlay = 0,      // 叠加显示（原图+分割，默认）
        OriginalOnly = 1, // 仅原始图像
        SegmentOnly = 2   // 仅分割彩色图
    };

    BrainRegionVisualizer(const std::string& niftiPath, const std::string& tsvPath, const std::string& rawPath = "");
    ~BrainRegionVisualizer(); // 如果需要，可以添加析构函数

    bool Initialize();
    void SetProgressCallback(ProgressCallback cb);

    // 分割视图显示模式控制
    void SetSegDisplayMode(SegDisplayMode mode);
    SegDisplayMode GetSegDisplayMode() const { return segDisplayMode_; }

    // 分割图叠加透明度 (0.0 - 1.0)
    void SetSegOverlayOpacity(double opacity);
    double GetSegOverlayOpacity() const { return segOverlayOpacity_; }

    // 是否有原始图像可用
    bool HasRawImage() const { return rawImageData_ != nullptr; }

    vtkSmartPointer<vtkRenderer> Get3DRenderer() const { return renderer3D_; }
    vtkSmartPointer<vtkImageSlice> GetAxialSlice() const { return axialSlice_; }
    vtkSmartPointer<vtkImageSlice> GetCoronalSlice() const { return coronalSlice_; }
    vtkSmartPointer<vtkImageSlice> GetSagittalSlice() const { return sagittalSlice_; }
    vtkSmartPointer<vtkTextActor> GetLabelTextActor() const { return labelText_; }

    // 导出的三张“融合后中间层”PNG路径（若未生成则为空字符串）
    const std::string& GetAxialMidPngPath() const { return axialMidPngPath_; }
    const std::string& GetCoronalMidPngPath() const { return coronalMidPngPath_; }
    const std::string& GetSagittalMidPngPath() const { return sagittalMidPngPath_; }

    // 动态生成三张“融合后中间层”像素切片 PNG。
    // - outputDir 为空：写入系统临时目录下的 ImageSystem/slice_previews/<timestamp>/
    // - outputDir 非空：写入到该目录（目录不存在会尝试创建）
    // 返回 true 表示三张都成功写出（路径会写入 Get*PngPath）
    bool GenerateMidSlicePNGs(const std::string& outputDir = "");

    // 动态生成一张“仅分割表面”的 3D 截图 PNG（背景透明）。
    // 视角：从上往下（Superior -> Inferior）。
    // - outputDir 为空：写入系统临时目录下的 ImageSystem/slice_previews/<timestamp>/
    // - outputDir 非空：写入到该目录（目录不存在会尝试创建）
    // 返回 true 表示写出成功（路径会写入 GetSeg3DPngPath）
    bool GenerateSegmentation3DPng(const std::string& outputDir = "");
    const std::string& GetSeg3DPngPath() const { return seg3dPngPath_; }

    std::vector<RegionEntry>& Regions() { return regions_; }
    std::unordered_map<vtkActor*, size_t>& ActorIndex() { return actorIndex_; }

    bool SetActorOpacity(int label, double opacity);
    bool SetActorVisible(int label, bool visible);

private:
    // 根据显示模式更新切片输入源
    void UpdateSliceInput();
    // 根据显示模式更新3D视图
    void Update3DDisplayMode();

    // 显示模式相关
    SegDisplayMode segDisplayMode_ = SegDisplayMode::Overlay;
    double segOverlayOpacity_ = 0.6;

    vtkSmartPointer<vtkImageData> ReorientToRAS(vtkImageData* input);
    bool LoadImage();
    bool LoadColorData();
    void ComputeLabelStatistics();
    bool BuildActors();
    void BuildSlices();
    void Build3DRenderer();
    bool ExportMidSlicePNGs(vtkImageData* sliceInputData, const std::string& outputDir);
    bool ExportSegmentation3DPng(const std::string& outputDir);

    std::unordered_map<int, LabelColor> LoadColorTable(const std::string& filename);
    std::map<int, LabelStyle> BuildLabelStyles(const std::set<int>& labels,
        const std::unordered_map<int, LabelColor>& colorTable);
    std::string DeriveGroupKey(const std::string& englishName) const;
    vtkSmartPointer<vtkActor> CreateLabelActor(int label, const LabelStyle& style);

    // 其他私有成员函数和变量
    std::string niftiPath_;
    std::string tsvPath_;
    std::string rawNiftiPath_;

    vtkSmartPointer<vtkNIFTIImageReader> reader_;
    vtkSmartPointer<vtkNIFTIImageReader> rawReader_;
    vtkSmartPointer<vtkImageData> imageData_;
    vtkSmartPointer<vtkImageData> rawImageData_;
    std::unordered_map<int, LabelColor> colorTable_;
    std::set<int> uniqueLabels_;
    std::map<int, LabelStyle> labelStyles_;

    std::vector<RegionEntry> regions_;
    std::unordered_map<vtkActor*, size_t> actorIndex_;
    std::unordered_map<int, size_t> labelIndex_;

    std::unique_ptr<LabelPipeline> pipeline_;
    double voxelVolume_{ 1.0 };

    vtkSmartPointer<vtkColorTransferFunction> colorTF_;
    vtkSmartPointer<vtkPiecewiseFunction> opacityTF_;
    vtkSmartPointer<vtkLookupTable> lut_;
    vtkSmartPointer<vtkImageMapToColors> colorMap_;
    vtkSmartPointer<vtkLookupTable> grayLut_;
    vtkSmartPointer<vtkImageMapToColors> grayMap_;
    vtkSmartPointer<vtkImageBlend> blendImage_;
    vtkSmartPointer<vtkImageSliceMapper> axialMapper_;
    vtkSmartPointer<vtkImageSliceMapper> coronalMapper_;
    vtkSmartPointer<vtkImageSliceMapper> sagittalMapper_;
    vtkSmartPointer<vtkImageSlice> axialSlice_;
    vtkSmartPointer<vtkImageSlice> coronalSlice_;
    vtkSmartPointer<vtkImageSlice> sagittalSlice_;

    std::string axialMidPngPath_;
    std::string coronalMidPngPath_;
    std::string sagittalMidPngPath_;
    std::string seg3dPngPath_;

    vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper_;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty_;
    vtkSmartPointer<vtkVolume> volume_;

    vtkSmartPointer<vtkRenderer> renderer3D_;
    vtkSmartPointer<vtkTextActor> labelText_;
    ProgressCallback progressCallback_;

    void ReportProgress(int percent, const std::string& message);

    template<typename T>
    void AccumulateCounts(T* ptr, vtkIdType count, std::unordered_map<int, vtkIdType>& counts)
    {
        for (vtkIdType i = 0; i < count; ++i)
        {
            counts[static_cast<int>(ptr[i])] += 1;
        }
    }

    // 禁止拷贝和赋值
    BrainRegionVisualizer(const BrainRegionVisualizer&) = delete;
    BrainRegionVisualizer& operator=(const BrainRegionVisualizer&) = delete;
};

std::string Trim(const std::string& s);
std::vector<std::string> SplitTSVLine(const std::string& line);
