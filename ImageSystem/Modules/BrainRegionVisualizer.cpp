// BrainRegionVisualizer.cpp
#include "BrainRegionVisualizer.h"
#include "BrainRegionProcessor.h"

#include <vtkPolyDataMapper.h>
#include <vtkImageFlip.h>
#include <vtkMatrix3x3.h>
#include <vtkPNGWriter.h>
#include <vtkWindowToImageFilter.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>
#include <vtkExtractVOI.h>
#include <vtkImagePermute.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageReslice.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSTLReader.h>
#include <vtkPolyData.h>
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

#include <filesystem>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <set>
#include <QDebug>

// ============================================================
// LabelPipeline 类（用于从 NIfTI 生成表面）
// ============================================================

class LabelPipeline
{
public:
    LabelPipeline(vtkImageData* image);
    vtkPolyData* Execute(int label);

private:
    vtkSmartPointer<vtkImageThreshold> threshold;
    vtkSmartPointer<vtkImageDilateErode3D> dilate;
    vtkSmartPointer<vtkImageGaussianSmooth> gaussian;
    vtkSmartPointer<vtkDiscreteMarchingCubes> marching;
    vtkSmartPointer<vtkPolyDataConnectivityFilter> connectivity;
    vtkSmartPointer<vtkCleanPolyData> cleaner;
    vtkSmartPointer<vtkSmoothPolyDataFilter> laplacian;
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother;
    vtkSmartPointer<vtkPolyDataNormals> normals;
};

LabelPipeline::LabelPipeline(vtkImageData* image)
{
    threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(image);
    threshold->ReplaceInOn();
    threshold->SetInValue(1);
    threshold->ReplaceOutOn();
    threshold->SetOutValue(0);

    dilate = vtkSmartPointer<vtkImageDilateErode3D>::New();
    dilate->SetInputConnection(threshold->GetOutputPort());
    dilate->SetDilateValue(1);
    dilate->SetErodeValue(0);
    dilate->SetKernelSize(5, 5, 5);

    gaussian = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    gaussian->SetInputConnection(dilate->GetOutputPort());
    gaussian->SetStandardDeviations(1.0, 1.0, 1.0);
    gaussian->SetRadiusFactors(1.5, 1.5, 1.5);

    marching = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
    marching->SetInputConnection(gaussian->GetOutputPort());
    marching->GenerateValues(1, 1, 1);

    connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    connectivity->SetInputConnection(marching->GetOutputPort());
    connectivity->SetExtractionModeToLargestRegion();

    cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(connectivity->GetOutputPort());

    laplacian = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    laplacian->SetInputConnection(cleaner->GetOutputPort());
    laplacian->SetNumberOfIterations(30);
    laplacian->SetRelaxationFactor(0.15);
    laplacian->FeatureEdgeSmoothingOff();
    laplacian->BoundarySmoothingOff();

    smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smoother->SetInputConnection(laplacian->GetOutputPort());
    smoother->SetNumberOfIterations(40);
    smoother->SetFeatureAngle(120.0);
    smoother->SetPassBand(0.08);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();

    normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputConnection(smoother->GetOutputPort());
    normals->ConsistencyOn();
    normals->SplittingOff();
    normals->AutoOrientNormalsOn();
}

vtkPolyData* LabelPipeline::Execute(int label)
{
    threshold->ThresholdBetween(label, label);
    marching->Update();
    if (!marching->GetOutput() || marching->GetOutput()->GetNumberOfCells() == 0)
        return nullptr;
    
    connectivity->Update();
    if (!connectivity->GetOutput() || connectivity->GetOutput()->GetNumberOfCells() == 0)
        return nullptr;
    
    cleaner->Update();
    laplacian->Update();
    smoother->Update();
    normals->Update();
    
    vtkPolyData* poly = normals->GetOutput();
    if (!poly || poly->GetNumberOfCells() == 0)
        return nullptr;
    
    return poly;
}

// ============================================================
// 构造与析构
// ============================================================

BrainRegionVisualizer::BrainRegionVisualizer()
    : initMode_(InitMode::None)
{
}

BrainRegionVisualizer::BrainRegionVisualizer(const std::string& processedDir, const std::string& rawNiftiPath)
    : initMode_(InitMode::FromProcessedDir)
    , processedDir_(processedDir)
    , rawNiftiPath_(rawNiftiPath)
{
}

BrainRegionVisualizer::BrainRegionVisualizer(const std::string& niftiPath, const std::string& tsvPath, const std::string& rawPath)
    : initMode_(InitMode::FromNifti)
    , niftiPath_(niftiPath)
    , tsvPath_(tsvPath)
    , rawNiftiPath_(rawPath)
{
}

BrainRegionVisualizer::~BrainRegionVisualizer()
{
}

void BrainRegionVisualizer::SetProgressCallback(ProgressCallback cb)
{
    progressCallback_ = std::move(cb);
}

void BrainRegionVisualizer::ReportProgress(int percent, const std::string& message)
{
    if (progressCallback_)
    {
        progressCallback_(percent, message);
    }
}

// ============================================================
// 从已处理目录初始化（推荐，快速）
// ============================================================

bool BrainRegionVisualizer::InitializeFromProcessedDir(const std::string& processedDir, const std::string& rawNiftiPath)
{
    processedDir_ = processedDir;
    rawNiftiPath_ = rawNiftiPath;
    initMode_ = InitMode::FromProcessedDir;

    ReportProgress(10, "检查处理结果目录...");

    // 检查是否已处理
    if (!BrainRegionProcessor::isAlreadyProcessed(QString::fromStdString(processedDir)))
    {
        ReportProgress(100, "目录未包含有效的处理结果");
        return false;
    }

    // 加载元数据
    ReportProgress(20, "加载脑区元数据...");
    auto metaList = BrainRegionProcessor::loadMetadata(QString::fromStdString(processedDir));
    if (metaList.empty())
    {
        ReportProgress(100, "无法加载元数据");
        return false;
    }

    // 转换为 RegionEntry
    regions_.clear();
    labelIndex_.clear();
    qDebug() << "========== 开始加载脑区元数据 ==========";
    for (const auto& meta : metaList)
    {
        RegionEntry entry;
        entry.label = meta.label;
        entry.englishName = meta.englishName.toStdString();
        entry.chineseName = meta.chineseName.toStdString();
        entry.hemisphere = meta.hemisphere;
        entry.groupKey = meta.groupKey.toStdString();
        entry.colorR = meta.colorR;
        entry.colorG = meta.colorG;
        entry.colorB = meta.colorB;
        entry.colorA = 1.0;  // 强制不透明，忽略 JSON 中的值
        entry.voxelCount = meta.voxelCount;
        entry.volume = meta.volume;
        entry.volumePercent = meta.volumePercent;
        entry.asymmetryIndex = meta.asymmetryIndex;
        entry.partnerLabel = meta.partnerLabel;
        entry.stlFileName = meta.stlFileName.toStdString();
        entry.baseOpacity = 1.0;

        labelIndex_[entry.label] = regions_.size();
        regions_.push_back(entry);
        
        qDebug() << QString("  脑区 [%1] %2: 颜色 RGB(%3, %4, %5), STL=%6")
            .arg(entry.label)
            .arg(meta.chineseName)
            .arg(entry.colorR, 0, 'f', 3)
            .arg(entry.colorG, 0, 'f', 3)
            .arg(entry.colorB, 0, 'f', 3)
            .arg(QString::fromStdString(entry.stlFileName));
    }
    qDebug() << "========== 共加载" << regions_.size() << "个脑区元数据 ==========";

    // 加载原始图像（如果提供）
    if (!rawNiftiPath.empty() && std::filesystem::exists(rawNiftiPath))
    {
        ReportProgress(25, "加载原始图像...");
        qDebug() << "加载原始图像:" << QString::fromStdString(rawNiftiPath);
        auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
        reader->SetFileName(rawNiftiPath.c_str());
        reader->Update();
        rawImageData_ = ReorientToRAS(reader->GetOutput());
        qDebug() << "原始图像加载完成";
    }
    
    // 加载分割图像（如果提供，用于二维切片叠加）
    if (!segNiftiPath_.empty() && std::filesystem::exists(segNiftiPath_))
    {
        ReportProgress(30, "加载分割图像...");
        qDebug() << "加载分割图像:" << QString::fromStdString(segNiftiPath_);
        auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
        reader->SetFileName(segNiftiPath_.c_str());
        reader->Update();
        imageData_ = ReorientToRAS(reader->GetOutput());
        qDebug() << "分割图像加载完成";
    }

    // 加载 STL 文件
    ReportProgress(40, "加载 STL 模型...");
    std::string stlDir = BrainRegionProcessor::getStlDir(QString::fromStdString(processedDir)).toStdString();
    if (!LoadStlFiles(stlDir))
    {
        ReportProgress(100, "加载 STL 文件失败");
        return false;
    }

    // 加载切片预览图路径
    std::string slicesDir = BrainRegionProcessor::getSlicesDir(QString::fromStdString(processedDir)).toStdString();
    std::filesystem::path slicesPath(slicesDir);
    if (std::filesystem::exists(slicesPath / "axial_mid.png"))
        axialMidPngPath_ = (slicesPath / "axial_mid.png").string();
    if (std::filesystem::exists(slicesPath / "coronal_mid.png"))
        coronalMidPngPath_ = (slicesPath / "coronal_mid.png").string();
    if (std::filesystem::exists(slicesPath / "sagittal_mid.png"))
        sagittalMidPngPath_ = (slicesPath / "sagittal_mid.png").string();

    // 构建切片视图（如果有原始图像）
    if (rawImageData_)
    {
        ReportProgress(80, "构建切片视图...");
        BuildSlices();
    }

    // 构建 3D 渲染器
    ReportProgress(90, "构建 3D 视图...");
    Build3DRenderer();

    ReportProgress(100, "初始化完成");
    return true;
}

// ============================================================
// 从 NIfTI 文件初始化（兼容旧逻辑）
// ============================================================

bool BrainRegionVisualizer::Initialize()
{
    if (initMode_ == InitMode::FromProcessedDir)
    {
        return InitializeFromProcessedDir(processedDir_, rawNiftiPath_);
    }

    // 旧的从 NIfTI 初始化逻辑
    ReportProgress(25, "加载分割体数据...");
    if (!LoadImage())
    {
        ReportProgress(100, "加载分割体数据失败");
        return false;
    }

    ReportProgress(30, "加载颜色表...");
    if (!LoadColorData())
    {
        ReportProgress(100, "加载颜色表失败");
        return false;
    }

    ReportProgress(35, "统计脑区数据...");
    ComputeLabelStatistics();

    ReportProgress(40, "构建脑区表面...");
    std::cout << "找到 " << regions_.size() << " 个脑区" << std::endl;

    if (!BuildActors())
    {
        ReportProgress(100, "构建脑区表面失败");
        return false;
    }

    ReportProgress(85, "构建切片视图...");
    BuildSlices();

    ReportProgress(90, "构建3D视图...");
    Build3DRenderer();

    ReportProgress(95, "完成脑区可视化初始化");
    return true;
}

// ============================================================
// STL 文件加载
// ============================================================

bool BrainRegionVisualizer::LoadStlFiles(const std::string& stlDir)
{
    qDebug() << "========== 开始加载 STL 文件 ==========";
    qDebug() << "STL 目录:" << QString::fromStdString(stlDir);
    
    if (!std::filesystem::exists(stlDir))
    {
        qWarning() << "STL 目录不存在:" << QString::fromStdString(stlDir);
        return false;
    }

    const size_t total = regions_.size();
    size_t loaded = 0;

    for (size_t i = 0; i < total; ++i)
    {
        auto& region = regions_[i];
        if (region.stlFileName.empty())
            continue;

        std::filesystem::path stlPath = std::filesystem::path(stlDir) / region.stlFileName;
        if (!std::filesystem::exists(stlPath))
        {
            qWarning() << "  STL 文件不存在:" << QString::fromStdString(stlPath.string());
            continue;
        }

        auto actor = LoadStlAsActor(stlPath.string(), region);
        if (actor)
        {
            region.actor = actor;
            actorIndex_[actor.GetPointer()] = i;
            ++loaded;
            
            qDebug() << QString("  [%1] 加载成功: %2, 颜色 RGB(%3, %4, %5), 透明度=%6")
                .arg(region.label)
                .arg(QString::fromStdString(region.stlFileName))
                .arg(region.colorR, 0, 'f', 3)
                .arg(region.colorG, 0, 'f', 3)
                .arg(region.colorB, 0, 'f', 3)
                .arg(region.colorA, 0, 'f', 2);
        }

        // 报告进度
        double ratio = static_cast<double>(i + 1) / static_cast<double>(total);
        int percent = 40 + static_cast<int>(ratio * 40);  // 40-80%
        ReportProgress(percent, "加载 STL (" + std::to_string(i + 1) + "/" + std::to_string(total) + ")");
    }

    qDebug() << "========== 成功加载" << loaded << "/" << total << "个 STL 文件 ==========";
    return loaded > 0;
}

vtkSmartPointer<vtkActor> BrainRegionVisualizer::LoadStlAsActor(const std::string& stlPath, const RegionEntry& region)
{
    auto reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(stlPath.c_str());
    reader->Update();

    vtkPolyData* poly = reader->GetOutput();
    if (!poly || poly->GetNumberOfCells() == 0)
    {
        return nullptr;
    }

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(poly);
    mapper->ScalarVisibilityOff();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetInterpolationToPhong();
    actor->GetProperty()->EdgeVisibilityOff();
    actor->GetProperty()->SetSpecular(0.15);
    actor->GetProperty()->SetSpecularPower(15.0);
    actor->GetProperty()->SetColor(region.colorR, region.colorG, region.colorB);
    actor->GetProperty()->SetOpacity(region.colorA);
    actor->SetPickable(true);

    return actor;
}

// ============================================================
// 旧版从 NIfTI 加载方法
// ============================================================

bool BrainRegionVisualizer::LoadImage()
{
    auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    reader->SetFileName(niftiPath_.c_str());
    reader->Update();
    imageData_ = ReorientToRAS(reader->GetOutput());
    
    if (!imageData_)
    {
        std::cerr << "无法读取 NIfTI 文件" << std::endl;
        return false;
    }

    if (!rawNiftiPath_.empty())
    {
        auto rawReader = vtkSmartPointer<vtkNIFTIImageReader>::New();
        rawReader->SetFileName(rawNiftiPath_.c_str());
        rawReader->Update();
        rawImageData_ = ReorientToRAS(rawReader->GetOutput());
        if (!rawImageData_)
        {
            std::cerr << "无法读取原始 NIfTI 文件: " << rawNiftiPath_ << std::endl;
        }
    }

    return true;
}

vtkSmartPointer<vtkImageData> BrainRegionVisualizer::ReorientToRAS(vtkImageData* input)
{
    if (!input)
        return nullptr;

    auto reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetInputData(input);
    reslice->SetOutputDimensionality(3);
    reslice->SetResliceAxesDirectionCosines(1, 0, 0, 0, 0, 1, 0, 1, 0);
    reslice->SetInterpolationModeToCubic();

    double spacing[3];
    input->GetSpacing(spacing);
    reslice->SetOutputSpacing(spacing);

    double origin[3];
    input->GetOrigin(origin);
    reslice->SetOutputOrigin(origin);

    int extent[6];
    input->GetExtent(extent);
    reslice->SetOutputExtent(extent);

    reslice->Update();

    auto aligned = vtkSmartPointer<vtkImageData>::New();
    aligned->DeepCopy(reslice->GetOutput());
    return aligned;
}

bool BrainRegionVisualizer::LoadColorData()
{
    colorTable_ = LoadColorTable(tsvPath_);
    if (colorTable_.empty())
    {
        std::cerr << "颜色表为空，无法上色" << std::endl;
        return false;
    }
    return true;
}

std::unordered_map<int, BrainRegionVisualizer::LabelColor> BrainRegionVisualizer::LoadColorTable(const std::string& filename)
{
    std::unordered_map<int, LabelColor> table;
    std::ifstream file(filename);
    if (!file.is_open())
        return table;

    std::string line;
    std::getline(file, line); // 跳过标题行

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        auto tokens = SplitTSVLine(line);
        if (tokens.size() < 3)
            continue;

        int label = 0;
        try
        {
            label = std::stoi(tokens[0]);
        }
        catch (...)
        {
            continue;
        }

        LabelColor lc;
        lc.EnglishName = tokens.size() > 1 ? tokens[1] : "";
        lc.ChineseName = tokens.size() > 3 ? tokens[3] : lc.EnglishName;
        
        if (tokens.size() > 4 && !tokens[4].empty())
            lc.Hemisphere = static_cast<char>(std::toupper(tokens[4][0]));
        
        if (tokens.size() > 5 && !tokens[5].empty())
            lc.GroupKey = Trim(tokens[5]);
        
        if (lc.GroupKey.empty())
            lc.GroupKey = DeriveGroupKey(lc.EnglishName);

        const std::string& colorStr = tokens[2];
        if (colorStr.size() == 7 && colorStr[0] == '#')
        {
            try
            {
                lc.R = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                lc.G = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                lc.B = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                table[label] = lc;
            }
            catch (...)
            {
                continue;
            }
        }
    }

    return table;
}

void BrainRegionVisualizer::ComputeLabelStatistics()
{
    vtkDataArray* scalars = imageData_->GetPointData()->GetScalars();
    if (!scalars || scalars->GetNumberOfComponents() != 1)
    {
        throw std::runtime_error("标量数据无效");
    }

    void* dataPtr = scalars->GetVoidPointer(0);
    const vtkIdType tupleCount = scalars->GetNumberOfTuples();
    std::unordered_map<int, vtkIdType> counts;

    switch (scalars->GetDataType())
    {
    case VTK_INT:
        AccumulateCounts(static_cast<int*>(dataPtr), tupleCount, counts);
        break;
    case VTK_UNSIGNED_INT:
        AccumulateCounts(static_cast<unsigned int*>(dataPtr), tupleCount, counts);
        break;
    case VTK_SHORT:
        AccumulateCounts(static_cast<short*>(dataPtr), tupleCount, counts);
        break;
    case VTK_UNSIGNED_SHORT:
        AccumulateCounts(static_cast<unsigned short*>(dataPtr), tupleCount, counts);
        break;
    case VTK_CHAR:
        AccumulateCounts(static_cast<char*>(dataPtr), tupleCount, counts);
        break;
    case VTK_UNSIGNED_CHAR:
        AccumulateCounts(static_cast<unsigned char*>(dataPtr), tupleCount, counts);
        break;
    default:
        throw std::runtime_error("不支持的标量数据类型");
    }

    uniqueLabels_.clear();
    for (const auto& kv : counts)
    {
        uniqueLabels_.insert(kv.first);
    }

    double spacing[3];
    imageData_->GetSpacing(spacing);
    voxelVolume_ = spacing[0] * spacing[1] * spacing[2];
    if (voxelVolume_ <= 0)
        voxelVolume_ = 1.0;

    regions_.clear();
    labelIndex_.clear();
    double totalVolume = 0.0;

    for (int label : uniqueLabels_)
    {
        if (label == 0)
            continue;

        RegionEntry region;
        region.label = label;

        auto it = colorTable_.find(label);
        if (it != colorTable_.end())
        {
            region.englishName = it->second.EnglishName.empty() ? ("Label " + std::to_string(label)) : it->second.EnglishName;
            region.chineseName = it->second.ChineseName.empty() ? region.englishName : it->second.ChineseName;
            region.hemisphere = it->second.Hemisphere;
            region.groupKey = it->second.GroupKey.empty() ? DeriveGroupKey(region.englishName) : it->second.GroupKey;
            region.colorR = it->second.R / 255.0;
            region.colorG = it->second.G / 255.0;
            region.colorB = it->second.B / 255.0;
        }
        else
        {
            region.englishName = "Label " + std::to_string(label);
            region.chineseName = region.englishName;
            region.groupKey = DeriveGroupKey(region.englishName);
        }
        region.colorA = 1.0;  // 默认不透明

        auto countIt = counts.find(label);
        if (countIt != counts.end())
        {
            region.voxelCount = static_cast<double>(countIt->second);
            region.volume = region.voxelCount * voxelVolume_;
            totalVolume += region.volume;
        }

        labelIndex_[label] = regions_.size();
        regions_.push_back(region);
    }

    if (totalVolume <= 0)
        totalVolume = 1.0;

    for (auto& region : regions_)
    {
        region.volumePercent = (region.volume / totalVolume) * 100.0;
    }

    // 计算不对称性指数
    struct PairVolumes
    {
        int leftIndex = -1;
        double leftVolume = 0.0;
        int rightIndex = -1;
        double rightVolume = 0.0;
    };

    std::unordered_map<std::string, PairVolumes> pairMap;
    for (size_t i = 0; i < regions_.size(); ++i)
    {
        auto& region = regions_[i];
        std::string key = region.groupKey.empty() ? region.englishName : region.groupKey;
        auto& pair = pairMap[key];
        if (region.hemisphere == 'L')
        {
            pair.leftIndex = static_cast<int>(i);
            pair.leftVolume = region.volume;
        }
        else if (region.hemisphere == 'R')
        {
            pair.rightIndex = static_cast<int>(i);
            pair.rightVolume = region.volume;
        }
    }

    for (const auto& kv : pairMap)
    {
        auto pair = kv.second;
        if (pair.leftIndex != -1 && pair.rightIndex != -1)
        {
            double denom = pair.leftVolume + pair.rightVolume;
            if (denom > 0)
            {
                double asym = 200.0 * std::abs(pair.leftVolume - pair.rightVolume) / denom;
                regions_[pair.leftIndex].asymmetryIndex = asym;
                regions_[pair.rightIndex].asymmetryIndex = asym;
                regions_[pair.leftIndex].partnerLabel = regions_[pair.rightIndex].label;
                regions_[pair.rightIndex].partnerLabel = regions_[pair.leftIndex].label;
            }
        }
    }
}

bool BrainRegionVisualizer::BuildActors()
{
    if (regions_.empty())
    {
        ReportProgress(70, "没有可用的脑区");
        return true;
    }

    auto pipeline = std::make_unique<LabelPipeline>(imageData_);
    const size_t total = regions_.size();

    constexpr double progressStart = 40.0;
    constexpr double progressEnd = 75.0;
    const double progressRange = progressEnd - progressStart;

    for (size_t i = 0; i < total; ++i)
    {
        auto& region = regions_[i];
        auto actor = CreateLabelActor(region.label, region.colorR, region.colorG, region.colorB, region.colorA);
        if (actor)
        {
            region.actor = actor;
            region.baseOpacity = 1.0;
            actorIndex_[actor.GetPointer()] = i;
        }

        double ratio = static_cast<double>(i + 1) / static_cast<double>(total);
        int percent = static_cast<int>(progressStart + ratio * progressRange);
        ReportProgress(percent, "构建脑区表面 (" + std::to_string(i + 1) + "/" + std::to_string(total) + ")");
    }

    return true;
}

vtkSmartPointer<vtkActor> BrainRegionVisualizer::CreateLabelActor(int label, double r, double g, double b, double a)
{
    auto pipeline = std::make_unique<LabelPipeline>(imageData_);
    vtkPolyData* poly = pipeline->Execute(label);
    if (!poly)
        return nullptr;

    auto polyCopy = vtkSmartPointer<vtkPolyData>::New();
    polyCopy->DeepCopy(poly);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyCopy);
    mapper->ScalarVisibilityOff();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetInterpolationToPhong();
    actor->GetProperty()->EdgeVisibilityOff();
    actor->GetProperty()->SetSpecular(0.15);
    actor->GetProperty()->SetSpecularPower(15.0);
    actor->GetProperty()->SetColor(r, g, b);
    actor->GetProperty()->SetOpacity(a);
    actor->SetPickable(true);

    return actor;
}

// ============================================================
// 切片视图构建
// ============================================================

void BrainRegionVisualizer::BuildSlices()
{
    vtkImageData* sliceSource = rawImageData_ ? rawImageData_.GetPointer() : imageData_.GetPointer();
    if (!sliceSource)
        return;

    // 如果有分割图像，构建颜色映射
    if (imageData_)
    {
        int maxLabel = 0;
        for (const auto& region : regions_)
        {
            if (region.label > maxLabel)
                maxLabel = region.label;
        }

        lut_ = vtkSmartPointer<vtkLookupTable>::New();
        lut_->SetRange(0, maxLabel);
        lut_->SetNumberOfTableValues(maxLabel + 1);
        lut_->Build();
        lut_->SetTableValue(0, 0, 0, 0, 0);

        for (const auto& region : regions_)
        {
            double alpha = (region.label == 0) ? 0.0 : region.colorA;
            lut_->SetTableValue(region.label, region.colorR, region.colorG, region.colorB, alpha);
        }

        colorMap_ = vtkSmartPointer<vtkImageMapToColors>::New();
        colorMap_->SetInputData(imageData_);
        colorMap_->SetLookupTable(lut_);
        colorMap_->PassAlphaToOutputOn();
        colorMap_->Update();
    }

    vtkAlgorithmOutput* sliceInput = nullptr;

    // 如有原始图，构建灰阶底图并与标签叠加
    if (rawImageData_)
    {
        double range[2] = {0.0, 255.0};
        rawImageData_->GetScalarRange(range);

        grayLut_ = vtkSmartPointer<vtkLookupTable>::New();
        grayLut_->SetNumberOfTableValues(256);
        grayLut_->SetTableRange(range[0], range[1]);
        grayLut_->Build();
        for (int i = 0; i < 256; ++i)
        {
            double v = static_cast<double>(i) / 255.0;
            grayLut_->SetTableValue(i, v, v, v, 1.0);
        }

        grayMap_ = vtkSmartPointer<vtkImageMapToColors>::New();
        grayMap_->SetInputData(rawImageData_);
        grayMap_->SetLookupTable(grayLut_);
        grayMap_->PassAlphaToOutputOn();
        grayMap_->Update();

        if (colorMap_)
        {
            blendImage_ = vtkSmartPointer<vtkImageBlend>::New();
            blendImage_->AddInputConnection(grayMap_->GetOutputPort());
            blendImage_->SetOpacity(0, 1.0);
            blendImage_->AddInputConnection(colorMap_->GetOutputPort());
            blendImage_->SetOpacity(1, segOverlayOpacity_);
            blendImage_->Update();
            sliceInput = blendImage_->GetOutputPort();
        }
        else
        {
            sliceInput = grayMap_->GetOutputPort();
        }
    }
    else if (colorMap_)
    {
        sliceInput = colorMap_->GetOutputPort();
    }

    if (!sliceInput)
        return;

    axialMapper_ = vtkSmartPointer<vtkImageSliceMapper>::New();
    axialMapper_->SetInputConnection(sliceInput);
    axialMapper_->SetOrientationToZ();
    axialMapper_->SetSliceNumber((axialMapper_->GetSliceNumberMinValue() + axialMapper_->GetSliceNumberMaxValue()) / 2);

    coronalMapper_ = vtkSmartPointer<vtkImageSliceMapper>::New();
    coronalMapper_->SetInputConnection(sliceInput);
    coronalMapper_->SetOrientationToY();
    coronalMapper_->SetSliceNumber((coronalMapper_->GetSliceNumberMinValue() + coronalMapper_->GetSliceNumberMaxValue()) / 2);

    sagittalMapper_ = vtkSmartPointer<vtkImageSliceMapper>::New();
    sagittalMapper_->SetInputConnection(sliceInput);
    sagittalMapper_->SetOrientationToX();
    sagittalMapper_->SetSliceNumber((sagittalMapper_->GetSliceNumberMinValue() + sagittalMapper_->GetSliceNumberMaxValue()) / 2);

    axialSlice_ = vtkSmartPointer<vtkImageSlice>::New();
    axialSlice_->SetMapper(axialMapper_);
    coronalSlice_ = vtkSmartPointer<vtkImageSlice>::New();
    coronalSlice_->SetMapper(coronalMapper_);
    sagittalSlice_ = vtkSmartPointer<vtkImageSlice>::New();
    sagittalSlice_->SetMapper(sagittalMapper_);
}

// ============================================================
// 3D 渲染器构建
// ============================================================

void BrainRegionVisualizer::Build3DRenderer()
{
    renderer3D_ = vtkSmartPointer<vtkRenderer>::New();
    renderer3D_->SetViewport(0.0, 0.0, 1.0, 1.0);
    renderer3D_->SetBackground(0.1, 0.1, 0.1);

    // 原始体渲染（若存在原始图像）
    if (rawImageData_)
    {
        auto volColor = vtkSmartPointer<vtkColorTransferFunction>::New();
        volColor->AddRGBPoint(-3024, 0.0, 0.0, 0.0);
        volColor->AddRGBPoint(-77, 0.54902, 0.25098, 0.14902);
        volColor->AddRGBPoint(94, 0.882353, 0.603922, 0.290196);
        volColor->AddRGBPoint(179, 1.0, 0.937033, 0.954531);
        volColor->AddRGBPoint(260, 0.615686, 0.0, 0.0);
        volColor->AddRGBPoint(3071, 0.827451, 0.658824, 1.0);

        auto volOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
        volOpacity->AddPoint(-3024, 0.0);
        volOpacity->AddPoint(-77, 0.0);
        volOpacity->AddPoint(94, 0.2);
        volOpacity->AddPoint(179, 0.25);
        volOpacity->AddPoint(260, 0.29);
        volOpacity->AddPoint(3071, 0.31);

        auto gradientFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
        gradientFunc->AddPoint(0, 0.0);
        gradientFunc->AddPoint(90, 0.5);
        gradientFunc->AddPoint(100, 1.0);

        volumeProperty_ = vtkSmartPointer<vtkVolumeProperty>::New();
        volumeProperty_->SetColor(volColor);
        volumeProperty_->SetScalarOpacity(volOpacity);
        volumeProperty_->SetGradientOpacity(gradientFunc);
        volumeProperty_->SetInterpolationTypeToLinear();
        volumeProperty_->ShadeOn();
        volumeProperty_->SetAmbient(0.4);
        volumeProperty_->SetDiffuse(0.6);
        volumeProperty_->SetSpecular(0.2);

        volumeMapper_ = vtkSmartPointer<vtkSmartVolumeMapper>::New();
        volumeMapper_->SetInputData(rawImageData_);
        volumeMapper_->SetBlendModeToComposite();
        volumeMapper_->SetRequestedRenderModeToGPU();

        volume_ = vtkSmartPointer<vtkVolume>::New();
        volume_->SetMapper(volumeMapper_);
        volume_->SetProperty(volumeProperty_);
        renderer3D_->AddVolume(volume_);
    }

    // 添加脑区 actors
    for (auto& region : regions_)
    {
        if (region.actor)
        {
            renderer3D_->AddActor(region.actor);
        }
    }

    // 标签文本
    labelText_ = vtkSmartPointer<vtkTextActor>::New();
    labelText_->SetInput("");
    labelText_->SetVisibility(0);
    labelText_->SetPosition(20, 40);
    labelText_->GetTextProperty()->SetFontSize(18);
    labelText_->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
    labelText_->GetTextProperty()->SetBold(true);
    renderer3D_->AddActor2D(labelText_);
}

// ============================================================
// 显示模式控制
// ============================================================

void BrainRegionVisualizer::SetSegDisplayMode(SegDisplayMode mode)
{
    if (segDisplayMode_ == mode)
        return;
    segDisplayMode_ = mode;
    UpdateSliceInput();
    Update3DDisplayMode();
}

void BrainRegionVisualizer::SetSegOverlayOpacity(double opacity)
{
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    segOverlayOpacity_ = opacity;

    if (blendImage_)
    {
        blendImage_->SetOpacity(1, segOverlayOpacity_);
        blendImage_->Update();
    }
}

void BrainRegionVisualizer::UpdateSliceInput()
{
    vtkAlgorithmOutput* sliceInput = nullptr;

    switch (segDisplayMode_)
    {
    case SegDisplayMode::OriginalOnly:
        sliceInput = grayMap_ ? grayMap_->GetOutputPort() : (colorMap_ ? colorMap_->GetOutputPort() : nullptr);
        break;
    case SegDisplayMode::SegmentOnly:
        sliceInput = colorMap_ ? colorMap_->GetOutputPort() : nullptr;
        break;
    case SegDisplayMode::Overlay:
    default:
        sliceInput = blendImage_ ? blendImage_->GetOutputPort() : (colorMap_ ? colorMap_->GetOutputPort() : nullptr);
        break;
    }

    if (sliceInput)
    {
        if (axialMapper_) axialMapper_->SetInputConnection(sliceInput);
        if (coronalMapper_) coronalMapper_->SetInputConnection(sliceInput);
        if (sagittalMapper_) sagittalMapper_->SetInputConnection(sliceInput);
    }
}

void BrainRegionVisualizer::Update3DDisplayMode()
{
    switch (segDisplayMode_)
    {
    case SegDisplayMode::OriginalOnly:
        if (volume_) volume_->SetVisibility(1);
        for (auto& region : regions_)
            if (region.actor) region.actor->SetVisibility(0);
        break;
    case SegDisplayMode::SegmentOnly:
        if (volume_) volume_->SetVisibility(0);
        for (auto& region : regions_)
            if (region.actor) region.actor->SetVisibility(region.baseOpacity > 0 ? 1 : 0);
        break;
    case SegDisplayMode::Overlay:
    default:
        if (volume_) volume_->SetVisibility(1);
        for (auto& region : regions_)
            if (region.actor) region.actor->SetVisibility(region.baseOpacity > 0 ? 1 : 0);
        break;
    }
}

// ============================================================
// Actor 控制
// ============================================================

bool BrainRegionVisualizer::SetActorOpacity(int label, double opacity)
{
    auto it = labelIndex_.find(label);
    if (it == labelIndex_.end())
        return false;
    
    auto& region = regions_[it->second];
    if (!region.actor)
        return false;
    
    region.baseOpacity = opacity;
    region.actor->GetProperty()->SetOpacity(opacity);
    return true;
}

bool BrainRegionVisualizer::SetActorVisible(int label, bool visible)
{
    auto it = labelIndex_.find(label);
    if (it == labelIndex_.end())
        return false;
    
    auto& region = regions_[it->second];
    
    if (region.actor)
        region.actor->SetVisibility(visible ? 1 : 0);
    
    // 更新 2D 切片视图的 LUT 透明度
    if (lut_ && label > 0)
    {
        double rgba[4];
        lut_->GetTableValue(label, rgba);
        rgba[3] = visible ? region.colorA : 0.0;
        lut_->SetTableValue(label, rgba[0], rgba[1], rgba[2], rgba[3]);
        lut_->Modified();
        
        if (colorMap_) colorMap_->Update();
        if (blendImage_) blendImage_->Update();
    }
    
    return true;
}

bool BrainRegionVisualizer::HighlightRegion(int label, bool highlight)
{
    auto it = labelIndex_.find(label);
    if (it == labelIndex_.end())
        return false;
    
    auto& region = regions_[it->second];
    if (!region.actor)
        return false;

    if (highlight)
    {
        region.actor->GetProperty()->SetOpacity(1.0);
        region.actor->GetProperty()->SetAmbient(0.5);
        // 使其他脑区变暗
        for (auto& other : regions_)
        {
            if (other.label != label && other.actor)
            {
                other.actor->GetProperty()->SetOpacity(0.2);
                other.dimmed = true;
            }
        }
    }
    else
    {
        region.actor->GetProperty()->SetOpacity(region.baseOpacity);
        region.actor->GetProperty()->SetAmbient(0.4);
        // 恢复其他脑区
        for (auto& other : regions_)
        {
            if (other.dimmed && other.actor)
            {
                other.actor->GetProperty()->SetOpacity(other.baseOpacity);
                other.dimmed = false;
            }
        }
    }
    
    return true;
}

void BrainRegionVisualizer::ResetAllRegions()
{
    for (auto& region : regions_)
    {
        if (region.actor)
        {
            region.actor->GetProperty()->SetOpacity(region.colorA);
            region.actor->GetProperty()->SetAmbient(0.4);
            region.actor->SetVisibility(1);
            region.baseOpacity = region.colorA;
            region.dimmed = false;
        }
    }
}

// ============================================================
// 预览图生成
// ============================================================

bool BrainRegionVisualizer::GenerateMidSlicePNGs(const std::string& outputDir)
{
    vtkImageData* source = imageData_ ? imageData_.GetPointer() : rawImageData_.GetPointer();
    if (!source)
        return false;

    // 构建导出用的颜色映射
    int maxLabel = 0;
    for (const auto& region : regions_)
    {
        if (region.label > maxLabel)
            maxLabel = region.label;
    }

    auto exportLut = vtkSmartPointer<vtkLookupTable>::New();
    exportLut->SetRange(0, maxLabel);
    exportLut->SetNumberOfTableValues(maxLabel + 1);
    exportLut->Build();
    exportLut->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
    
    for (const auto& region : regions_)
    {
        if (region.label > 0)
            exportLut->SetTableValue(region.label, region.colorR, region.colorG, region.colorB, 1.0);
    }

    auto exportMap = vtkSmartPointer<vtkImageMapToColors>::New();
    exportMap->SetInputData(source);
    exportMap->SetLookupTable(exportLut);
    exportMap->PassAlphaToOutputOn();
    exportMap->Update();

    return ExportMidSlicePNGs(exportMap->GetOutput(), outputDir);
}

bool BrainRegionVisualizer::ExportMidSlicePNGs(vtkImageData* sliceInputData, const std::string& outputDir)
{
    axialMidPngPath_.clear();
    coronalMidPngPath_.clear();
    sagittalMidPngPath_.clear();

    if (!sliceInputData)
        return false;

    int extent[6];
    sliceInputData->GetExtent(extent);
    const int midX = (extent[0] + extent[1]) / 2;
    const int midY = (extent[2] + extent[3]) / 2;
    const int midZ = (extent[4] + extent[5]) / 2;

    std::filesystem::path outDir;
    try
    {
        if (!outputDir.empty())
            outDir = std::filesystem::path(outputDir);
        else
        {
            auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            outDir = std::filesystem::temp_directory_path() / "ImageSystem" / "slice_previews" / std::to_string(ts);
        }
        std::filesystem::create_directories(outDir);
    }
    catch (...)
    {
        return false;
    }

    auto write2D = [&](const std::filesystem::path& outFile, vtkAlgorithmOutput* port) -> bool {
        auto writer = vtkSmartPointer<vtkPNGWriter>::New();
        writer->SetFileName(outFile.string().c_str());
        writer->SetInputConnection(port);
        writer->Write();
        return std::filesystem::exists(outFile);
    };

    // Axial
    auto axialVOI = vtkSmartPointer<vtkExtractVOI>::New();
    axialVOI->SetInputData(sliceInputData);
    axialVOI->SetVOI(extent[0], extent[1], extent[2], extent[3], midZ, midZ);
    auto axialPerm = vtkSmartPointer<vtkImagePermute>::New();
    axialPerm->SetInputConnection(axialVOI->GetOutputPort());
    axialPerm->SetFilteredAxes(0, 1, 2);
    auto axialFlip = vtkSmartPointer<vtkImageFlip>::New();
    axialFlip->SetInputConnection(axialPerm->GetOutputPort());
    axialFlip->SetFilteredAxis(1);
    axialFlip->FlipAboutOriginOff();
    axialFlip->Update();

    // Coronal
    auto corVOI = vtkSmartPointer<vtkExtractVOI>::New();
    corVOI->SetInputData(sliceInputData);
    corVOI->SetVOI(extent[0], extent[1], midY, midY, extent[4], extent[5]);
    auto corPerm = vtkSmartPointer<vtkImagePermute>::New();
    corPerm->SetInputConnection(corVOI->GetOutputPort());
    corPerm->SetFilteredAxes(0, 2, 1);
    auto corFlip = vtkSmartPointer<vtkImageFlip>::New();
    corFlip->SetInputConnection(corPerm->GetOutputPort());
    corFlip->SetFilteredAxis(1);
    corFlip->FlipAboutOriginOff();
    corFlip->Update();

    // Sagittal
    auto sagVOI = vtkSmartPointer<vtkExtractVOI>::New();
    sagVOI->SetInputData(sliceInputData);
    sagVOI->SetVOI(midX, midX, extent[2], extent[3], extent[4], extent[5]);
    auto sagPerm = vtkSmartPointer<vtkImagePermute>::New();
    sagPerm->SetInputConnection(sagVOI->GetOutputPort());
    sagPerm->SetFilteredAxes(1, 2, 0);
    auto sagFlip = vtkSmartPointer<vtkImageFlip>::New();
    sagFlip->SetInputConnection(sagPerm->GetOutputPort());
    sagFlip->SetFilteredAxis(1);
    sagFlip->FlipAboutOriginOff();
    sagFlip->Update();

    bool ok = true;
    std::filesystem::path axialFile = outDir / "axial_mid.png";
    std::filesystem::path corFile = outDir / "coronal_mid.png";
    std::filesystem::path sagFile = outDir / "sagittal_mid.png";

    ok &= write2D(axialFile, axialFlip->GetOutputPort());
    ok &= write2D(corFile, corFlip->GetOutputPort());
    ok &= write2D(sagFile, sagFlip->GetOutputPort());

    if (ok)
    {
        axialMidPngPath_ = axialFile.string();
        coronalMidPngPath_ = corFile.string();
        sagittalMidPngPath_ = sagFile.string();
    }

    return ok;
}

bool BrainRegionVisualizer::GenerateSegmentation3DPng(const std::string& outputDir)
{
    return ExportSegmentation3DPng(outputDir);
}

bool BrainRegionVisualizer::ExportSegmentation3DPng(const std::string& outputDir)
{
    seg3dPngPath_.clear();

    bool hasAnyActor = false;
    for (const auto& r : regions_)
    {
        if (r.actor)
        {
            hasAnyActor = true;
            break;
        }
    }
    if (!hasAnyActor)
        return false;

    std::filesystem::path outDir;
    try
    {
        if (!outputDir.empty())
            outDir = std::filesystem::path(outputDir);
        else
        {
            auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            outDir = std::filesystem::temp_directory_path() / "ImageSystem" / "slice_previews" / std::to_string(ts);
        }
        std::filesystem::create_directories(outDir);
    }
    catch (...)
    {
        return false;
    }

    std::filesystem::path outFile = outDir / "seg3d_superior.png";

    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.0, 0.0, 0.0);
#if VTK_MAJOR_VERSION >= 8
    renderer->SetBackgroundAlpha(0.0);
#endif

    for (const auto& region : regions_)
    {
        if (region.actor)
            renderer->AddActor(region.actor);
    }

    auto window = vtkSmartPointer<vtkRenderWindow>::New();
    window->OffScreenRenderingOn();
    window->SetSize(1024, 1024);
    window->SetMultiSamples(0);
    window->SetAlphaBitPlanes(1);
    window->AddRenderer(renderer);

    renderer->ResetCamera();
    double bounds[6];
    renderer->ComputeVisiblePropBounds(bounds);
    const double cx = 0.5 * (bounds[0] + bounds[1]);
    const double cy = 0.5 * (bounds[2] + bounds[3]);
    const double cz = 0.5 * (bounds[4] + bounds[5]);
    const double maxDim = std::max({bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]});
    const double dist = (maxDim <= 0.0) ? 500.0 : (2.0 * maxDim);

    vtkCamera* cam = renderer->GetActiveCamera();
    if (cam)
    {
        cam->SetFocalPoint(cx, cy, cz);
        cam->SetPosition(cx, cy + dist, cz);
        cam->SetViewUp(0, 0, -1);
    }
    renderer->ResetCameraClippingRange();

    window->Render();

    auto w2i = vtkSmartPointer<vtkWindowToImageFilter>::New();
    w2i->SetInput(window);
    w2i->SetInputBufferTypeToRGBA();
    w2i->ReadFrontBufferOff();
    w2i->Update();

    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(outFile.string().c_str());
    writer->SetInputConnection(w2i->GetOutputPort());
    writer->Write();

    if (std::filesystem::exists(outFile))
    {
        seg3dPngPath_ = outFile.string();
        return true;
    }
    return false;
}

// ============================================================
// 工具方法
// ============================================================

std::string BrainRegionVisualizer::DeriveGroupKey(const std::string& englishName) const
{
    std::string trimmed = Trim(englishName);
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto stripPrefix = [&](const std::string& prefix) -> bool {
        if (lower.rfind(prefix, 0) == 0 && trimmed.size() > prefix.size())
        {
            trimmed = trimmed.substr(prefix.size());
            lower = lower.substr(prefix.size());
            return true;
        }
        return false;
    };

    stripPrefix("left-") || stripPrefix("left_") || stripPrefix("ctx-lh-") || 
    stripPrefix("wm-lh-") || stripPrefix("lh.") || stripPrefix("wm_lh");
    stripPrefix("right-") || stripPrefix("right_") || stripPrefix("ctx-rh-") || 
    stripPrefix("wm-rh-") || stripPrefix("rh.") || stripPrefix("wm_rh");

    size_t start = 0;
    while (start < trimmed.size() && (trimmed[start] == '-' || trimmed[start] == '_' || 
           trimmed[start] == '.' || trimmed[start] == ' '))
    {
        ++start;
    }
    trimmed = trimmed.substr(start);

    if (trimmed.empty())
        return Trim(englishName);
    return trimmed;
}

// ============================================================
// 全局工具函数
// ============================================================

std::string Trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r\n\"");
    const auto last = s.find_last_not_of(" \t\r\n\"");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return s.substr(first, last - first + 1);
}

std::vector<std::string> SplitTSVLine(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '\t'))
    {
        out.emplace_back(Trim(field));
    }
    return out;
}

