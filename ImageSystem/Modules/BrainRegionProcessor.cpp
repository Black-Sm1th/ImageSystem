#include "BrainRegionProcessor.h"

#include "BrainRegionVisualizer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

#include <vtkSmartPointer.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkImageThreshold.h>
#include <vtkImageDilateErode3D.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkSTLWriter.h>
#include <vtkImageReslice.h>
#include <vtkLookupTable.h>
#include <vtkImageMapToColors.h>
#include <vtkExtractVOI.h>
#include <vtkImagePermute.h>
#include <vtkImageFlip.h>
#include <vtkPNGWriter.h>

#include <set>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <filesystem>

// ============================================================
// 构造与析构
// ============================================================

BrainRegionProcessor::BrainRegionProcessor(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<ProcessingResult>("ProcessingResult");
    qRegisterMetaType<BrainRegionMeta>("BrainRegionMeta");
}

BrainRegionProcessor::~BrainRegionProcessor()
{
    cancel();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }
}

void BrainRegionProcessor::setProgressCallback(ProgressCallback callback)
{
    m_progressCallback = std::move(callback);
}

void BrainRegionProcessor::reportProgress(int percent, const QString& message)
{
    if (m_progressCallback) {
        m_progressCallback(percent, message);
    }
}

// ============================================================
// 同步处理
// ============================================================

ProcessingResult BrainRegionProcessor::process(
    const QString& segNiftiPath,
    const QString& rawNiftiPath,
    const QString& colorTablePath,
    const QString& outputDir)
{
    ProcessingResult result;
    result.outputDir = outputDir;
    m_isProcessing = true;
    m_cancelRequested = false;

    // 1. 创建输出目录结构
    reportProgress(5, "创建输出目录...");
    QDir outDir(outputDir);
    if (!outDir.exists()) {
        if (!outDir.mkpath(".")) {
            result.message = QString("无法创建输出目录: %1").arg(outputDir);
            emit processError(result.message);
            m_isProcessing = false;
            return result;
        }
    }

    QString stlDir = getStlDir(outputDir);
    QDir().mkpath(stlDir);

    // 2. 加载分割图像
    reportProgress(10, "加载分割图像...");
    if (!loadNiftiImage(segNiftiPath, m_segImageData)) {
        result.message = QString("无法加载分割图像: %1").arg(segNiftiPath);
        emit processError(result.message);
        m_isProcessing = false;
        return result;
    }

    // 3. 加载原始图像（可选）
    if (!rawNiftiPath.isEmpty() && QFileInfo::exists(rawNiftiPath)) {
        reportProgress(15, "加载原始图像...");
        loadNiftiImage(rawNiftiPath, m_rawImageData);
    }

    // 4. 加载颜色表
    reportProgress(20, "加载颜色表...");
    if (!loadColorTable(colorTablePath)) {
        result.message = QString("无法加载颜色表: %1").arg(colorTablePath);
        emit processError(result.message);
        m_isProcessing = false;
        return result;
    }

    // 5. 计算脑区统计信息
    reportProgress(25, "计算脑区统计信息...");
    computeRegionStatistics(m_segImageData);

    if (m_cancelRequested) {
        result.message = "处理已取消";
        m_isProcessing = false;
        return result;
    }

    // 6. 生成 STL 文件
    reportProgress(30, "开始生成 STL 文件...");
    if (!generateAllStlFiles(stlDir)) {
        result.message = "生成 STL 文件失败";
        emit processError(result.message);
        m_isProcessing = false;
        return result;
    }

    if (m_cancelRequested) {
        result.message = "处理已取消";
        m_isProcessing = false;
        return result;
    }

    // 7. 保存元数据
    reportProgress(90, "保存元数据...");
    if (!saveMetadataJson(outputDir)) {
        result.message = "保存元数据失败";
        emit processError(result.message);
        m_isProcessing = false;
        return result;
    }

    reportProgress(93, "生成脑分割预览图...");
    if (!generatePreviewImages(segNiftiPath, rawNiftiPath, outputDir)) {
        qWarning() << "生成脑分割预览图失败:" << outputDir;
    }

    // 9. 保存处理信息
    reportProgress(95, "保存处理信息...");
    saveProcessingInfo(outputDir, segNiftiPath, rawNiftiPath);

    // 完成
    reportProgress(100, "处理完成");
    result.success = true;
    result.message = "处理成功";
    result.metadataPath = QDir(outputDir).filePath("brain_regions_metadata.json");
    result.processingInfoPath = QDir(outputDir).filePath("processing_info.json");
    result.regionCount = static_cast<int>(m_regions.size());
    
    // 统计 STL 文件数量
    QDir stlDirectory(stlDir);
    result.stlFileCount = stlDirectory.entryList(QStringList() << "*.stl", QDir::Files).size();

    m_isProcessing = false;
    emit processFinished(result);
    return result;
}

// ============================================================
// 异步处理
// ============================================================

void BrainRegionProcessor::processAsync(
    const QString& segNiftiPath,
    const QString& rawNiftiPath,
    const QString& colorTablePath,
    const QString& outputDir)
{
    if (m_isProcessing) {
        emit processError("已有处理任务正在运行");
        return;
    }

    QtConcurrent::run([this, segNiftiPath, rawNiftiPath, colorTablePath, outputDir]() {
        process(segNiftiPath, rawNiftiPath, colorTablePath, outputDir);
    });
}

void BrainRegionProcessor::processBatchAsync(
    const QList<std::tuple<QString, QString, QString>>& subjects,
    const QString& colorTablePath,
    const QString& baseOutputDir)
{
    if (m_isProcessing) {
        emit processError("已有处理任务正在运行");
        return;
    }

    QtConcurrent::run([this, subjects, colorTablePath, baseOutputDir]() {
        m_isProcessing = true;
        m_cancelRequested = false;
        
        int successCount = 0;
        int failCount = 0;
        const int total = subjects.size();

        for (int i = 0; i < total && !m_cancelRequested; ++i) {
            const auto& [segPath, rawPath, subName] = subjects[i];
            emit batchProgress(i, total, subName);

            QString outputDir = QDir(baseOutputDir).filePath(subName);
            ProcessingResult result = process(segPath, rawPath, colorTablePath, outputDir);
            
            if (result.success) {
                ++successCount;
            } else {
                ++failCount;
            }
        }

        m_isProcessing = false;
        emit batchFinished(successCount, failCount);
    });
}

void BrainRegionProcessor::cancel()
{
    m_cancelRequested = true;
}

// ============================================================
// 静态方法
// ============================================================

bool BrainRegionProcessor::isAlreadyProcessed(const QString& outputDir)
{
    QString metadataPath = QDir(outputDir).filePath("brain_regions_metadata.json");
    QString stlDir = getStlDir(outputDir);
    
    return QFileInfo::exists(metadataPath) && 
           QDir(stlDir).entryList(QStringList() << "*.stl", QDir::Files).size() > 0;
}

std::vector<BrainRegionMeta> BrainRegionProcessor::loadMetadata(const QString& outputDir)
{
    std::vector<BrainRegionMeta> regions;
    QString metadataPath = QDir(outputDir).filePath("brain_regions_metadata.json");
    
    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开元数据文件:" << metadataPath;
        return regions;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "元数据文件格式错误";
        return regions;
    }

    QJsonObject root = doc.object();
    QJsonArray regionsArray = root["regions"].toArray();

    for (const QJsonValue& val : regionsArray) {
        QJsonObject obj = val.toObject();
        BrainRegionMeta meta;
        meta.label = obj["label"].toInt();
        meta.englishName = obj["englishName"].toString();
        meta.chineseName = obj["chineseName"].toString();
        meta.hemisphere = obj["hemisphere"].toString().isEmpty() ? 'N' : obj["hemisphere"].toString().at(0).toLatin1();
        meta.groupKey = obj["groupKey"].toString();
        meta.colorR = obj["colorR"].toDouble();
        meta.colorG = obj["colorG"].toDouble();
        meta.colorB = obj["colorB"].toDouble();
        meta.colorA = obj["colorA"].toDouble(0.6);
        meta.voxelCount = obj["voxelCount"].toDouble();
        meta.volume = obj["volume"].toDouble();
        meta.volumePercent = obj["volumePercent"].toDouble();
        meta.asymmetryIndex = obj["asymmetryIndex"].toDouble();
        meta.partnerLabel = obj["partnerLabel"].toInt(-1);
        meta.stlFileName = obj["stlFileName"].toString();
        regions.push_back(meta);
    }

    return regions;
}

QString BrainRegionProcessor::getStlDir(const QString& outputDir)
{
    return QDir(outputDir).filePath("stl");
}

// ============================================================
// 内部方法实现
// ============================================================

bool BrainRegionProcessor::loadNiftiImage(const QString& path, vtkImageData*& imageData)
{
    auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    reader->SetFileName(path.toStdString().c_str());
    reader->Update();

    vtkImageData* output = reader->GetOutput();
    if (!output) {
        qWarning() << "无法读取 NIfTI 文件:" << path;
        return false;
    }

    // 重定向到 RAS 坐标系
    imageData = reorientToRAS(output);
    return imageData != nullptr;
}

vtkImageData* BrainRegionProcessor::reorientToRAS(vtkImageData* input)
{
    if (!input) {
        return nullptr;
    }

    auto reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetInputData(input);
    reslice->SetOutputDimensionality(3);
    reslice->SetResliceAxesDirectionCosines(
        1, 0, 0,
        0, 0, 1,
        0, 1, 0);
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
    aligned->Register(nullptr);  // 防止被释放
    
    return aligned;
}

bool BrainRegionProcessor::loadColorTable(const QString& tsvPath)
{
    m_colorTable.clear();

    QFile file(tsvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开颜色表文件:" << tsvPath;
        return false;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");  // 设置 UTF-8 编码，解决中文乱码问题
    in.readLine(); // 跳过表头: index	name	color	chinese_name	side

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList tokens = line.split('\t');
        if (tokens.size() < 3) continue;

        bool ok;
        int label = tokens[0].trimmed().toInt(&ok);
        if (!ok) continue;

        LabelColor lc;
        
        // 解析英文名（去掉引号）
        QString englishName = tokens.size() > 1 ? tokens[1].trimmed() : "";
        // 去掉首尾的双引号
        englishName.remove(QChar('"'));
        lc.englishName = englishName;
        
        // 解析中文名
        QString chineseName = tokens.size() > 3 ? tokens[3].trimmed() : "";
        chineseName.remove(QChar('"'));  // 去掉可能存在的引号
        lc.chineseName = chineseName.isEmpty() ? lc.englishName : chineseName;
        
        // 解析半球 (side 列)
        if (tokens.size() > 4 && !tokens[4].trimmed().isEmpty()) {
            lc.hemisphere = tokens[4].trimmed().toUpper().at(0).toLatin1();
        }
        
        // 根据英文名推导 groupKey
        lc.groupKey = deriveGroupKey(lc.englishName);

        // 解析颜色 #RRGGBB
        QString colorStr = tokens[2].trimmed();
        if (colorStr.size() == 7 && colorStr[0] == '#') {
            lc.R = colorStr.mid(1, 2).toInt(nullptr, 16);
            lc.G = colorStr.mid(3, 2).toInt(nullptr, 16);
            lc.B = colorStr.mid(5, 2).toInt(nullptr, 16);
        } else {
            // 默认颜色（灰色）
            lc.R = 128;
            lc.G = 128;
            lc.B = 128;
        }

        m_colorTable[label] = lc;
    }

    file.close();
    qDebug() << "已加载颜色表，共" << m_colorTable.size() << "个条目";
    return !m_colorTable.empty();
}

void BrainRegionProcessor::computeRegionStatistics(vtkImageData* imageData)
{
    m_regions.clear();
    m_labelIndex.clear();

    vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
    if (!scalars || scalars->GetNumberOfComponents() != 1) {
        qWarning() << "标量数据无效";
        return;
    }

    // 统计每个标签的体素数量
    std::unordered_map<int, vtkIdType> counts;
    const vtkIdType tupleCount = scalars->GetNumberOfTuples();
    
    for (vtkIdType i = 0; i < tupleCount; ++i) {
        int label = static_cast<int>(scalars->GetTuple1(i));
        counts[label]++;
    }

    // 获取唯一标签集合
    std::set<int> uniqueLabels;
    for (const auto& kv : counts) {
        if (kv.first != 0) {  // 排除背景
            uniqueLabels.insert(kv.first);
        }
    }

    // 计算体素体积
    double spacing[3];
    imageData->GetSpacing(spacing);
    m_voxelVolume = std::abs(spacing[0] * spacing[1] * spacing[2]);
    if (m_voxelVolume <= 0) {
        m_voxelVolume = 1.0;
    }

    // 计算总体积
    double totalVolume = 0.0;
    for (const auto& kv : counts) {
        if (kv.first != 0) {
            totalVolume += kv.second * m_voxelVolume;
        }
    }
    if (totalVolume <= 0) {
        totalVolume = 1.0;
    }

    // 构建脑区元数据
    for (int label : uniqueLabels) {
        BrainRegionMeta meta;
        meta.label = label;

        auto it = m_colorTable.find(label);
        if (it != m_colorTable.end()) {
            meta.englishName = it->second.englishName;
            meta.chineseName = it->second.chineseName.isEmpty() ? it->second.englishName : it->second.chineseName;
            meta.hemisphere = it->second.hemisphere;
            meta.groupKey = it->second.groupKey;
            meta.colorR = it->second.R / 255.0;
            meta.colorG = it->second.G / 255.0;
            meta.colorB = it->second.B / 255.0;
        } else {
            meta.englishName = QString("Label %1").arg(label);
            meta.chineseName = meta.englishName;
            meta.groupKey = deriveGroupKey(meta.englishName);
        }
        meta.colorA = 1.0;  // 默认不透明

        auto countIt = counts.find(label);
        if (countIt != counts.end()) {
            meta.voxelCount = static_cast<double>(countIt->second);
            meta.volume = meta.voxelCount * m_voxelVolume;
            meta.volumePercent = (meta.volume / totalVolume) * 100.0;
        }

        m_labelIndex[label] = m_regions.size();
        m_regions.push_back(meta);
    }

    // 计算不对称性指数
    struct PairVolumes {
        int leftIndex = -1;
        double leftVolume = 0.0;
        int rightIndex = -1;
        double rightVolume = 0.0;
    };

    std::unordered_map<std::string, PairVolumes> pairMap;
    for (size_t i = 0; i < m_regions.size(); ++i) {
        auto& region = m_regions[i];
        std::string key = region.groupKey.toStdString();
        if (key.empty()) key = region.englishName.toStdString();
        
        auto& pair = pairMap[key];
        if (region.hemisphere == 'L') {
            pair.leftIndex = static_cast<int>(i);
            pair.leftVolume = region.volume;
        } else if (region.hemisphere == 'R') {
            pair.rightIndex = static_cast<int>(i);
            pair.rightVolume = region.volume;
        }
    }

    for (const auto& kv : pairMap) {
        auto pair = kv.second;
        if (pair.leftIndex != -1 && pair.rightIndex != -1) {
            double denom = pair.leftVolume + pair.rightVolume;
            if (denom > 0) {
                double asym = 200.0 * std::abs(pair.leftVolume - pair.rightVolume) / denom;
                m_regions[pair.leftIndex].asymmetryIndex = asym;
                m_regions[pair.rightIndex].asymmetryIndex = asym;
                m_regions[pair.leftIndex].partnerLabel = m_regions[pair.rightIndex].label;
                m_regions[pair.rightIndex].partnerLabel = m_regions[pair.leftIndex].label;
            }
        }
    }

    qDebug() << "计算完成，共" << m_regions.size() << "个脑区";
}

bool BrainRegionProcessor::generateStlFile(int label, const QString& outputPath)
{
    if (!m_segImageData) {
        return false;
    }

    // 阈值提取指定标签
    auto threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(m_segImageData);
    threshold->ThresholdBetween(label, label);
    threshold->ReplaceInOn();
    threshold->SetInValue(1);
    threshold->ReplaceOutOn();
    threshold->SetOutValue(0);

    // 膨胀连接零散体素
    auto dilate = vtkSmartPointer<vtkImageDilateErode3D>::New();
    dilate->SetInputConnection(threshold->GetOutputPort());
    dilate->SetDilateValue(1);
    dilate->SetErodeValue(0);
    dilate->SetKernelSize(5, 5, 5);

    // 高斯平滑
    auto gaussian = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    gaussian->SetInputConnection(dilate->GetOutputPort());
    gaussian->SetStandardDeviations(1.0, 1.0, 1.0);
    gaussian->SetRadiusFactors(1.5, 1.5, 1.5);

    // Marching Cubes 提取表面
    auto marching = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
    marching->SetInputConnection(gaussian->GetOutputPort());
    marching->GenerateValues(1, 1, 1);
    marching->Update();

    if (!marching->GetOutput() || marching->GetOutput()->GetNumberOfCells() == 0) {
        return false;
    }

    // 保留最大连通区域
    auto connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    connectivity->SetInputConnection(marching->GetOutputPort());
    connectivity->SetExtractionModeToLargestRegion();
    connectivity->Update();

    if (!connectivity->GetOutput() || connectivity->GetOutput()->GetNumberOfCells() == 0) {
        return false;
    }

    // 清理重复点
    auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputConnection(connectivity->GetOutputPort());

    // Laplacian 平滑
    auto laplacian = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    laplacian->SetInputConnection(cleaner->GetOutputPort());
    laplacian->SetNumberOfIterations(30);
    laplacian->SetRelaxationFactor(0.15);
    laplacian->FeatureEdgeSmoothingOff();
    laplacian->BoundarySmoothingOff();

    // Windowed Sinc 平滑
    auto smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smoother->SetInputConnection(laplacian->GetOutputPort());
    smoother->SetNumberOfIterations(40);
    smoother->SetFeatureAngle(120.0);
    smoother->SetPassBand(0.08);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();

    // 重新计算法线
    auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputConnection(smoother->GetOutputPort());
    normals->ConsistencyOn();
    normals->SplittingOff();
    normals->AutoOrientNormalsOn();
    normals->Update();

    vtkPolyData* poly = normals->GetOutput();
    if (!poly || poly->GetNumberOfCells() == 0) {
        return false;
    }

    // 写入 STL 文件
    auto writer = vtkSmartPointer<vtkSTLWriter>::New();
    writer->SetFileName(outputPath.toStdString().c_str());
    writer->SetInputData(poly);
    writer->SetFileTypeToBinary();
    writer->Write();

    return QFileInfo::exists(outputPath);
}

bool BrainRegionProcessor::generateAllStlFiles(const QString& stlDir)
{
    const size_t total = m_regions.size();
    if (total == 0) {
        return true;
    }

    constexpr double progressStart = 30.0;
    constexpr double progressEnd = 85.0;
    const double progressRange = progressEnd - progressStart;

    int successCount = 0;
    for (size_t i = 0; i < total && !m_cancelRequested; ++i) {
        auto& region = m_regions[i];
        
        // 生成文件名: region_XXX_name.stl
        QString safeName = sanitizeFileName(region.englishName);
        QString fileName = QString("region_%1_%2.stl")
            .arg(region.label, 3, 10, QChar('0'))
            .arg(safeName);
        QString filePath = QDir(stlDir).filePath(fileName);

        if (generateStlFile(region.label, filePath)) {
            region.stlFileName = fileName;
            ++successCount;
        }

        // 报告进度
        double ratio = static_cast<double>(i + 1) / static_cast<double>(total);
        int percent = static_cast<int>(progressStart + ratio * progressRange);
        reportProgress(percent, QString("生成 STL (%1/%2): %3")
            .arg(i + 1).arg(total).arg(region.chineseName));
    }

    qDebug() << "STL 生成完成:" << successCount << "/" << total;
    return successCount > 0;
}

bool BrainRegionProcessor::saveMetadataJson(const QString& outputDir)
{
    QString filePath = QDir(outputDir).filePath("brain_regions_metadata.json");
    
    QJsonObject root;
    root["version"] = "1.0";
    root["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["regionCount"] = static_cast<int>(m_regions.size());
    root["voxelVolume"] = m_voxelVolume;

    QJsonArray regionsArray;
    for (const auto& region : m_regions) {
        QJsonObject obj;
        obj["label"] = region.label;
        obj["englishName"] = region.englishName;
        obj["chineseName"] = region.chineseName;
        obj["hemisphere"] = QString(QChar(region.hemisphere));
        obj["groupKey"] = region.groupKey;
        obj["colorR"] = region.colorR;
        obj["colorG"] = region.colorG;
        obj["colorB"] = region.colorB;
        obj["colorA"] = region.colorA;
        obj["voxelCount"] = region.voxelCount;
        obj["volume"] = region.volume;
        obj["volumePercent"] = region.volumePercent;
        obj["asymmetryIndex"] = region.asymmetryIndex;
        obj["partnerLabel"] = region.partnerLabel;
        obj["stlFileName"] = region.stlFileName;
        regionsArray.append(obj);
    }
    root["regions"] = regionsArray;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法写入元数据文件:" << filePath;
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool BrainRegionProcessor::generatePreviewImages(const QString& segPath, const QString& rawPath, const QString& outputDir)
{
    const QString slicesDir = QDir(outputDir).filePath("slices");
    if (!QDir().mkpath(slicesDir)) {
        qWarning() << "无法创建脑分割预览图目录:" << slicesDir;
        return false;
    }

    BrainRegionVisualizer visualizer;
    visualizer.SetSegmentationNiftiPath(segPath.toStdString());
    if (!visualizer.InitializeFromProcessedDir(outputDir.toStdString(), rawPath.toStdString())) {
        qWarning() << "BrainRegionVisualizer 初始化失败:" << outputDir;
        return false;
    }

    const bool slicesOk = visualizer.GenerateMidSlicePNGs(slicesDir.toStdString());
    const bool seg3dOk = visualizer.GenerateSegmentation3DPng(slicesDir.toStdString());
    return slicesOk && seg3dOk;
}

bool BrainRegionProcessor::saveProcessingInfo(const QString& outputDir, const QString& segPath, const QString& rawPath)
{
    QString filePath = QDir(outputDir).filePath("processing_info.json");
    
    QJsonObject root;
    root["version"] = "1.0";
    root["processedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["segmentationFile"] = segPath;
    root["rawFile"] = rawPath;
    root["stlDirectory"] = getStlDir(outputDir);
    root["metadataFile"] = QDir(outputDir).filePath("brain_regions_metadata.json");
    root["slicesDirectory"] = QDir(outputDir).filePath("slices");
    root["axialMidImage"] = QDir(outputDir).filePath("slices/axial_mid.png");
    root["coronalMidImage"] = QDir(outputDir).filePath("slices/coronal_mid.png");
    root["sagittalMidImage"] = QDir(outputDir).filePath("slices/sagittal_mid.png");
    root["seg3dImage"] = QDir(outputDir).filePath("slices/seg3d_superior.png");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法写入处理信息文件:" << filePath;
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

QString BrainRegionProcessor::sanitizeFileName(const QString& name) const
{
    QString result = name.toLower();
    result.replace(QRegularExpression("[^a-z0-9_-]"), "_");
    result.replace(QRegularExpression("_{2,}"), "_");
    result.remove(QRegularExpression("^[_-]+|[_-]+$"));
    if (result.isEmpty()) {
        result = "unnamed";
    }
    return result;
}

QString BrainRegionProcessor::deriveGroupKey(const QString& englishName) const
{
    QString trimmed = englishName.trimmed();
    QString lower = trimmed.toLower();

    auto stripPrefix = [&](const QString& prefix) -> bool {
        if (lower.startsWith(prefix) && trimmed.size() > prefix.size()) {
            trimmed = trimmed.mid(prefix.size());
            lower = lower.mid(prefix.size());
            return true;
        }
        return false;
    };

    stripPrefix("left-") || stripPrefix("left_") || stripPrefix("ctx-lh-") || 
    stripPrefix("wm-lh-") || stripPrefix("lh.") || stripPrefix("wm_lh");
    stripPrefix("right-") || stripPrefix("right_") || stripPrefix("ctx-rh-") || 
    stripPrefix("wm-rh-") || stripPrefix("rh.") || stripPrefix("wm_rh");

    // 移除开头的特殊字符
    int start = 0;
    while (start < trimmed.size() && (trimmed[start] == '-' || trimmed[start] == '_' || 
           trimmed[start] == '.' || trimmed[start] == ' ')) {
        ++start;
    }
    trimmed = trimmed.mid(start);

    if (trimmed.isEmpty()) {
        return englishName.trimmed();
    }
    return trimmed;
}

