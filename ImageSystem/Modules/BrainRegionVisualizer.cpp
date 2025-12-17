// BrainRegionVisualizer.cpp
#include "BrainRegionVisualizer.h"
#include <vtkPolyDataMapper.h> 
#include "vtkImageFlip.h"

class LabelPipeline
{
public:
    LabelPipeline(vtkImageData* image);
    vtkPolyData* Execute(int label);

private:
    vtkSmartPointer<vtkImageThreshold> threshold;
    vtkSmartPointer<vtkImageDilateErode3D> dilate;
    vtkSmartPointer<vtkImageDilateErode3D> erode1;
    vtkSmartPointer<vtkImageDilateErode3D> erode2;
    vtkSmartPointer<vtkImageGaussianSmooth> gaussian;
    vtkSmartPointer<vtkDiscreteMarchingCubes> marching;
    vtkSmartPointer<vtkPolyDataConnectivityFilter> connectivity;
    vtkSmartPointer<vtkCleanPolyData> cleaner;
    vtkSmartPointer<vtkSmoothPolyDataFilter> laplacian;
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother;
    vtkSmartPointer<vtkPolyDataNormals> normals;
};

// LabelPipeline 的实现
LabelPipeline::LabelPipeline(vtkImageData* image)
{
    // 阈值出指定标签的二值体素
    threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(image);
    threshold->ReplaceInOn();
    threshold->SetInValue(1);
    threshold->ReplaceOutOn();
    threshold->SetOutValue(0);

    // 先做一次膨胀，连接零散体素
    dilate = vtkSmartPointer<vtkImageDilateErode3D>::New();
    dilate->SetInputConnection(threshold->GetOutputPort());
    dilate->SetDilateValue(1);
    dilate->SetErodeValue(0);
    dilate->SetKernelSize(5, 5, 5);

    // 两次腐蚀，去掉毛刺/孤立点
    //erode1 = vtkSmartPointer<vtkImageDilateErode3D>::New();
    //erode1->SetInputConnection(dilate->GetOutputPort());
    //erode1->SetDilateValue(1);
    //erode1->SetErodeValue(0);
    //erode1->SetKernelSize(1, 1, 1);

    //erode2 = vtkSmartPointer<vtkImageDilateErode3D>::New();
    //erode2->SetInputConnection(erode1->GetOutputPort());
    //erode2->SetDilateValue(1);
    //erode2->SetErodeValue(0);
    //erode2->SetKernelSize(1, 1, 1);

    // 轻度高斯平滑，柔化体素边界
    gaussian = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    gaussian->SetInputConnection(dilate->GetOutputPort());
    gaussian->SetStandardDeviations(1.0, 1.0, 1.0);
    gaussian->SetRadiusFactors(1.5, 1.5, 1.5);

    // Marching Cubes 提取表面
    marching = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
    marching->SetInputConnection(gaussian->GetOutputPort());
    marching->GenerateValues(1, 1, 1);

    // 仅保留最大连通区域（可选，默认全保留）
    connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    connectivity->SetInputConnection(marching->GetOutputPort());
    connectivity->SetExtractionModeToLargestRegion();

    // 清理重复点
    cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(connectivity->GetOutputPort());

    // Laplacian 平滑
    laplacian = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    laplacian->SetInputConnection(cleaner->GetOutputPort());
    laplacian->SetNumberOfIterations(30);
    laplacian->SetRelaxationFactor(0.15);
    laplacian->FeatureEdgeSmoothingOff();
    laplacian->BoundarySmoothingOff();

    // Windowed Sinc 平滑，减少台阶
    smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smoother->SetInputConnection(laplacian->GetOutputPort());
    smoother->SetNumberOfIterations(40);
    smoother->SetFeatureAngle(120.0);
    smoother->SetPassBand(0.08);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();

    // 重新计算法线用于光滑着色
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
    {
        return nullptr;
    }
    connectivity->Update();
    if (!connectivity->GetOutput() || connectivity->GetOutput()->GetNumberOfCells() == 0)
    {
        return nullptr;
    }
    cleaner->Update();
    if (!cleaner->GetOutput() || cleaner->GetOutput()->GetNumberOfCells() == 0)
    {
        return nullptr;
    }
    laplacian->Update();
    if (!laplacian->GetOutput() || laplacian->GetOutput()->GetNumberOfCells() == 0)
    {
        return nullptr;
    }
    smoother->Update();
    if (!smoother->GetOutput() || smoother->GetOutput()->GetNumberOfCells() == 0)
    {
        return nullptr;
    }
    normals->Update();
    vtkPolyData* poly = normals->GetOutput();
    if (!poly || poly->GetNumberOfCells() == 0)
    {
        return nullptr;
    }
    return poly;
}

// BrainRegionVisualizer 的实现
BrainRegionVisualizer::BrainRegionVisualizer(const std::string& niftiPath, const std::string& tsvPath, const std::string& rawPath)
    : niftiPath_(niftiPath), tsvPath_(tsvPath), rawNiftiPath_(rawPath)
{
}

BrainRegionVisualizer::~BrainRegionVisualizer()
{
    // 清理资源（如果需要）
}

bool BrainRegionVisualizer::Initialize()
{
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
    pipeline_ = std::make_unique<LabelPipeline>(imageData_);
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

bool BrainRegionVisualizer::LoadImage()
{
    reader_ = vtkSmartPointer<vtkNIFTIImageReader>::New();
    reader_->SetFileName(niftiPath_.c_str());
    reader_->Update();
    imageData_ = reader_->GetOutput();
    if (!imageData_)
    {
        std::cerr << "无法读取 NIfTI 文件" << std::endl;
        return false;
    }

    if (!rawNiftiPath_.empty())
    {
        rawReader_ = vtkSmartPointer<vtkNIFTIImageReader>::New();
        rawReader_->SetFileName(rawNiftiPath_.c_str());
        rawReader_->Update();
        rawImageData_ = rawReader_->GetOutput();
        if (!rawImageData_)
        {
            std::cerr << "无法读取原始 NIfTI 文件: " << rawNiftiPath_ << std::endl;
        }
    }
    return true;
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

void BrainRegionVisualizer::ComputeLabelStatistics()
{
    vtkDataArray* scalars = imageData_->GetPointData()->GetScalars();
    if (!scalars || scalars->GetNumberOfComponents() != 1)
    {
        throw std::runtime_error("标量数据无效（缺失或多通道）");
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

    labelStyles_ = BuildLabelStyles(uniqueLabels_, colorTable_);

    double spacing[3]{ 1.0, 1.0, 1.0 };
    imageData_->GetSpacing(spacing);
    voxelVolume_ = spacing[0] * spacing[1] * spacing[2];
    if (voxelVolume_ <= 0)
    {
        voxelVolume_ = 1.0;
    }

    regions_.clear();
    labelIndex_.clear();

    for (const auto& entry : labelStyles_)
    {
        RegionEntry region;
        region.label = entry.first;
        region.englishName = entry.second.EnglishName.empty() ? ("Label " + std::to_string(entry.first)) : entry.second.EnglishName;
        region.chineseName = entry.second.ChineseName.empty() ? region.englishName : entry.second.ChineseName;
        region.hemisphere = entry.second.Hemisphere;
        region.groupKey = entry.second.GroupKey.empty() ? DeriveGroupKey(region.englishName) : entry.second.GroupKey;
        region.colorR = entry.second.R;
        region.colorG = entry.second.G;
        region.colorB = entry.second.B;
        region.colorA = entry.second.A;

        regions_.emplace_back(region);
        labelIndex_[region.label] = regions_.size() - 1;
    }
}

bool BrainRegionVisualizer::BuildActors()
{
    if (!pipeline_)
    {
        pipeline_ = std::make_unique<LabelPipeline>(imageData_);
    }
    const size_t actorCount = regions_.size();
    if (actorCount == 0)
    {
        ReportProgress(70, "没有可用的脑区用于构建表面");
        return true;
    }

    constexpr double progressStart = 40.0;
    constexpr double progressEnd = 75.0;
    const double progressRange = progressEnd - progressStart;
    double lastReported = progressStart;

    for (size_t i = 0; i < actorCount; ++i)
    {
        auto& region = regions_[i];
        auto styleIt = labelStyles_.find(region.label);
        if (styleIt == labelStyles_.end())
        {
            continue;
        }

        auto actor = CreateLabelActor(region.label, styleIt->second);
        if (!actor)
        {
            continue;
        }

        region.actor = actor;
        region.baseOpacity = 1.0;
        actorIndex_[actor.GetPointer()] = i;

        double ratio = static_cast<double>(i + 1) / static_cast<double>(actorCount);
        double currentPercent = progressStart + ratio * progressRange;
        if ((currentPercent - lastReported) >= 1.0 || i + 1 == actorCount)
        {
            std::ostringstream oss;
            oss << "构建脑区表面 (" << (i + 1) << "/" << actorCount << ")";
            ReportProgress(static_cast<int>(currentPercent), oss.str());
            lastReported = currentPercent;
        }
    }
    return true;
}

void BrainRegionVisualizer::BuildSlices()
{
    // 标签 LUT / 颜色映射
    colorTF_ = vtkSmartPointer<vtkColorTransferFunction>::New();
    opacityTF_ = vtkSmartPointer<vtkPiecewiseFunction>::New();
    for (const auto& entry : labelStyles_)
    {
        colorTF_->AddRGBPoint(entry.first, entry.second.R, entry.second.G, entry.second.B);
        opacityTF_->AddPoint(entry.first, entry.second.A);
    }

    int maxLabel = labelStyles_.empty() ? 0 : labelStyles_.rbegin()->first;
    lut_ = vtkSmartPointer<vtkLookupTable>::New();
    lut_->SetRange(0, maxLabel);
    lut_->SetNumberOfTableValues(maxLabel + 1);
    lut_->Build();
    lut_->SetTableValue(0, 0, 0, 0, 0);
    for (const auto& entry : labelStyles_)
    {
        double alpha = (entry.first == 0) ? 0.0 : entry.second.A;
        lut_->SetTableValue(entry.first, entry.second.R, entry.second.G, entry.second.B, alpha);
    }

    colorMap_ = vtkSmartPointer<vtkImageMapToColors>::New();
    colorMap_->SetInputData(imageData_);
    colorMap_->SetLookupTable(lut_);
    colorMap_->PassAlphaToOutputOn();
    colorMap_->Update();

    vtkAlgorithmOutput* sliceInput = colorMap_->GetOutputPort();

    // 如有原始图，构建灰阶底图并与标签叠加；否则保持标签单层显示
    if (rawImageData_)
    {
        double range[2]{ 0.0, 255.0 };
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

        blendImage_ = vtkSmartPointer<vtkImageBlend>::New();
        blendImage_->AddInputConnection(grayMap_->GetOutputPort());   // 原始灰阶
        blendImage_->SetOpacity(0, 1.0);
        blendImage_->AddInputConnection(colorMap_->GetOutputPort());  // 标签彩色
        blendImage_->SetOpacity(1, 0.6); // 标签透明度
        blendImage_->Update();

        sliceInput = blendImage_->GetOutputPort();
    }

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

void BrainRegionVisualizer::Build3DRenderer()
{

    renderer3D_ = vtkSmartPointer<vtkRenderer>::New();
    // 3D视图单独使用一个渲染窗口，因此使用全屏视口
    renderer3D_->SetViewport(0.0, 0.0, 1.0, 1.0);
    renderer3D_->SetBackground(0.1, 0.1, 0.1);

    // 原始体渲染（若存在原始图像，否则使用标签图作为体渲染）
    vtkImageData* volImage = rawImageData_;
    if (volImage)
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

        vtkSmartPointer<vtkPiecewiseFunction> gradientFunc =
            vtkSmartPointer<vtkPiecewiseFunction>::New();
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
        volumeMapper_->SetInputData(volImage);
        volumeMapper_->SetBlendModeToComposite();
        volumeMapper_->SetRequestedRenderModeToGPU();

        volume_ = vtkSmartPointer<vtkVolume>::New();
        volume_->SetMapper(volumeMapper_);
        volume_->SetProperty(volumeProperty_);
        renderer3D_->AddVolume(volume_);
    }

    double surfaceOpacity = volume_ ? 0.6 : 1.0;
    for (auto& region : regions_)
    {
        if (region.actor)
        {
            renderer3D_->AddActor(region.actor);
        }
    }

    labelText_ = vtkSmartPointer<vtkTextActor>::New();
    labelText_->SetInput("");
    labelText_->SetVisibility(0);
    labelText_->SetPosition(20, 40);
    labelText_->GetTextProperty()->SetFontSize(18);
    labelText_->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
    labelText_->GetTextProperty()->SetBold(true);
    renderer3D_->AddActor2D(labelText_);
}

bool BrainRegionVisualizer::SetActorOpacity(int label, double opacity)
{
    auto it = labelIndex_.find(label);
    if (it == labelIndex_.end())
    {
        return false;
    }
    auto& region = regions_[it->second];
    if (!region.actor)
    {
        return false;
    }
    region.baseOpacity = opacity;
    region.actor->GetProperty()->SetOpacity(opacity);
    return true;
}

bool BrainRegionVisualizer::SetActorVisible(int label, bool visible)
{
    auto it = labelIndex_.find(label);
    if (it == labelIndex_.end())
    {
        return false;
    }
    auto& region = regions_[it->second];
    if (!region.actor)
    {
        return false;
    }
    region.actor->SetVisibility(visible ? 1 : 0);
    return true;
}

std::string Trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t\r\n\"");
    const auto last = s.find_last_not_of(" \t\r\n\"");
    if (first == std::string::npos || last == std::string::npos)
    {
        return "";
    }
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

std::string BrainRegionVisualizer::DeriveGroupKey(const std::string& englishName) const
{
    std::string trimmed = Trim(englishName);
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto stripPrefix = [&](const std::string& prefix) -> bool
        {
            if (lower.rfind(prefix, 0) == 0 && trimmed.size() > prefix.size())
            {
                trimmed = trimmed.substr(prefix.size());
                return true;
            }
            return false;
        };

    stripPrefix("left-") || stripPrefix("left_") || stripPrefix("ctx-lh-") || stripPrefix("wm-lh-") || stripPrefix("lh.") || stripPrefix("wm_lh");
    stripPrefix("right-") || stripPrefix("right_") || stripPrefix("ctx-rh-") || stripPrefix("wm-rh-") || stripPrefix("rh.") || stripPrefix("wm_rh");

    trimmed.erase(0, trimmed.find_first_not_of("-_. "));
    if (trimmed.empty())
    {
        return Trim(englishName);
    }
    return trimmed;
}

std::unordered_map<int, LabelColor> BrainRegionVisualizer::LoadColorTable(const std::string& filename)
{
    std::unordered_map<int, LabelColor> table;

    std::ifstream file(filename);
    if (!file.is_open())
    {
        //std::cerr << "无法打开 TSV 文件: " << filename << std::endl;
        return table;
    }

    std::string line;
    std::getline(file, line); // 跳过标题行

    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        auto tokens = SplitTSVLine(line);
        if (tokens.size() < 3)
        {
            //std::cerr << "TSV 行格式错误，跳过: " << line << std::endl;
            continue;
        }

        int label = 0;
        try
        {
            label = std::stoi(tokens[0]);
        }
        catch (const std::exception&)
        {
            //std::cerr << "无法解析标签索引: " << tokens[0] << ", 跳过该行." << std::endl;
            continue;
        }

        const std::string englishName = tokens.size() > 1 ? tokens[1] : "";
        const std::string chineseName = tokens.size() > 3 ? tokens[3] : englishName;
        char hemisphere = 'N';
        if (tokens.size() > 4 && !tokens[4].empty())
        {
            hemisphere = static_cast<char>(std::toupper(tokens[4][0]));
        }
        std::string groupKey;
        if (tokens.size() > 5 && !tokens[5].empty())
        {
            groupKey = Trim(tokens[5]);
        }
        if (groupKey.empty())
        {
            groupKey = DeriveGroupKey(englishName);
        }

        const std::string& colorStr = tokens[2];
        if (colorStr.size() != 7 || colorStr[0] != '#')
        {
            //std::cerr << "颜色格式错误: " << colorStr << ", 跳过该行." << std::endl;
            continue;
        }

        try
        {
            int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
            int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
            int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
            table[label] = { r, g, b, Trim(englishName), Trim(chineseName), hemisphere, groupKey };
        }
        catch (const std::exception&)
        {
            //std::cerr << "无法解析颜色: " << colorStr << ", 跳过该行." << std::endl;
            continue;
        }
    }

    //std::cout << "TSV 文件加载完成，共 " << table.size() << " 个标签颜色." << std::endl;
    return table;
}

template<typename T>
void CollectLabels(T* ptr, vtkIdType count, std::set<int>& uniqueLabels)
{
    for (vtkIdType i = 0; i < count; ++i)
    {
        uniqueLabels.insert(static_cast<int>(ptr[i]));
    }
}

std::map<int, LabelStyle> BrainRegionVisualizer::BuildLabelStyles(
    const std::set<int>& labels,
    const std::unordered_map<int, LabelColor>& colorTable)
{
    std::map<int, LabelStyle> styles;

    for (int label : labels)
    {
        LabelStyle style;
        auto it = colorTable.find(label);
        if (it != colorTable.end())
        {
            style.R = it->second.R / 255.0;
            style.G = it->second.G / 255.0;
            style.B = it->second.B / 255.0;
            style.A = (label == 0) ? 0.0 : /*0.7*/1.0;
            style.EnglishName = it->second.EnglishName;
            style.ChineseName = it->second.ChineseName;
            style.Hemisphere = it->second.Hemisphere;
            style.GroupKey = it->second.GroupKey;
        }
        else if (label == 0)
        {
            style.A = 0.0;
        }
        else
        {
            style.EnglishName = "Label " + std::to_string(label);
            style.ChineseName = style.EnglishName;
        }
        if (style.ChineseName.empty())
        {
            style.ChineseName = style.EnglishName;
        }
        if (style.GroupKey.empty())
        {
            style.GroupKey = DeriveGroupKey(style.EnglishName);
        }
        styles[label] = style;
    }

    return styles;
}

vtkSmartPointer<vtkActor> BrainRegionVisualizer::CreateLabelActor(int label, const LabelStyle& style)
{
    if (!pipeline_)
    {
        pipeline_ = std::make_unique<LabelPipeline>(imageData_);
    }
    vtkPolyData* poly = pipeline_->Execute(label);
    if (!poly)
    {
        return nullptr;
    }

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
    actor->GetProperty()->SetColor(style.R, style.G, style.B);
    actor->GetProperty()->SetOpacity(style.A); // 使用 A 作为透明度
    actor->SetPickable(true);

    return actor;
}

void BrainRegionVisualizer::SetProgressCallback(ProgressCallback cb)
{
    progressCallback_ = std::move(cb);
}

void BrainRegionVisualizer::ReportProgress(int percent, const std::string& message)
{
    if (progressCallback_)
    {
        std::cout << percent << std::endl;
        progressCallback_(percent, message);
    }
}

