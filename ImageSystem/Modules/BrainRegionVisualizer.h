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
#include <vtkSmartPointer.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkLookupTable.h>
#include <vtkImageMapToColors.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageSlice.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolumeProperty.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkPropPicker.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <map>
#include <memory>
#include <cmath>
#include <stdexcept>
#include <vtkNIFTIImageReader.h>
#include <vtkProperty.h>

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
    BrainRegionVisualizer(const std::string& niftiPath, const std::string& tsvPath);
    ~BrainRegionVisualizer(); // 如果需要，可以添加析构函数

    bool Initialize();

    vtkSmartPointer<vtkRenderer> Get3DRenderer() const { return renderer3D_; }
    vtkSmartPointer<vtkImageSlice> GetAxialSlice() const { return axialSlice_; }
    vtkSmartPointer<vtkImageSlice> GetCoronalSlice() const { return coronalSlice_; }
    vtkSmartPointer<vtkImageSlice> GetSagittalSlice() const { return sagittalSlice_; }
    vtkSmartPointer<vtkTextActor> GetLabelTextActor() const { return labelText_; }

    std::vector<RegionEntry>& Regions() { return regions_; }
    std::unordered_map<vtkActor*, size_t>& ActorIndex() { return actorIndex_; }

    bool SetActorOpacity(int label, double opacity);
    bool SetActorVisible(int label, bool visible);

private:
    bool LoadImage();
    bool LoadColorData();
    void ComputeLabelStatistics();
    bool BuildActors();
    void BuildSlices();
    void Build3DRenderer();

    std::unordered_map<int, LabelColor> LoadColorTable(const std::string& filename);
    std::map<int, LabelStyle> BuildLabelStyles(const std::set<int>& labels,
        const std::unordered_map<int, LabelColor>& colorTable);
    std::string DeriveGroupKey(const std::string& englishName) const;
    vtkSmartPointer<vtkActor> CreateLabelActor(int label, const LabelStyle& style);

    // 其他私有成员函数和变量
    std::string niftiPath_;
    std::string tsvPath_;

    vtkSmartPointer<vtkNIFTIImageReader> reader_;
    vtkSmartPointer<vtkImageData> imageData_;
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
    vtkSmartPointer<vtkImageSliceMapper> axialMapper_;
    vtkSmartPointer<vtkImageSliceMapper> coronalMapper_;
    vtkSmartPointer<vtkImageSliceMapper> sagittalMapper_;
    vtkSmartPointer<vtkImageSlice> axialSlice_;
    vtkSmartPointer<vtkImageSlice> coronalSlice_;
    vtkSmartPointer<vtkImageSlice> sagittalSlice_;

    vtkSmartPointer<vtkSmartVolumeMapper> volumeMapper_;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty_;
    vtkSmartPointer<vtkVolume> volume_;

    vtkSmartPointer<vtkRenderer> renderer3D_;
    vtkSmartPointer<vtkTextActor> labelText_;

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
