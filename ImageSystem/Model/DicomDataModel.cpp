#include "DicomDataModel.h"

#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkNIFTIImageReader.h>
#include <vtkLookupTable.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkImageMapToColors.h>
#include <vtkType.h>

#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <string>
#include <QString>

#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

namespace
{
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

template<typename T>
void CollectSegLabels(T* ptr, vtkIdType count, std::set<int>& uniqueLabels)
{
    if (!ptr)
    {
        return;
    }
    for (vtkIdType i = 0; i < count; ++i)
    {
        uniqueLabels.insert(static_cast<int>(ptr[i]));
    }
}
}

DicomDataModel::DicomDataModel(QObject* parent)
    : QObject(parent)
{
}

QString DicomDataModel::normalizeLocalPath(const QString& path) const
{
    if (path.isEmpty())
    {
        return {};
    }
    QUrl url(path);
    if (url.isLocalFile())
    {
        return QDir::fromNativeSeparators(url.toLocalFile());
    }
    QString normalized = path;
    if (normalized.startsWith("file:///"))
    {
        normalized = normalized.mid(8);
    }
    return QDir::fromNativeSeparators(normalized);
}

void DicomDataModel::setBrainSegColorTablePath(const QString& path)
{
    QString normalized = normalizeLocalPath(path);
    if (normalized == m_brainSegColorTablePath)
    {
        return;
    }
    m_brainSegColorTablePath = normalized;
    m_brainSegColorTable.clear();
    emit brainSegColorTablePathChanged();
}

bool DicomDataModel::loadDicomDirectory(const QString& path)
{
    QString dirPath = normalizeLocalPath(path);
    if (dirPath.isEmpty())
    {
        return false;
    }

    QFileInfo pathInfo(dirPath);
    if (pathInfo.isFile())
    {
        const QString suffix = pathInfo.completeSuffix().toLower();
        if (suffix == "nii" || suffix == "nii.gz")
        {
           // return loadNiftiVolumeAsPrimary(dirPath);
            return loadBrainSegmentation(dirPath);
        }
    }

    if (!pathInfo.isDir())
    {
        qWarning() << "路径既不是有效的 DICOM 目录也不是 NIfTI 文件:" << dirPath;
        return false;
    }

    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    QByteArray utf8Path = dirPath.toUtf8();
    reader->SetDirectoryName(utf8Path.constData());
    reader->Update();

    vtkImageData* output = reader->GetOutput();
    if (!output || output->GetNumberOfPoints() == 0)
    {
        qWarning() << "Failed to read DICOM data or directory is empty!";
        return false;
    }

    return finalizePrimaryVolumeLoad(output, {});
}

bool DicomDataModel::loadNiftiVolumeAsPrimary(const QString& niftiFilePath)
{
    if (niftiFilePath.isEmpty())
    {
        return false;
    }

    QFileInfo fileInfo(niftiFilePath);
    if (!fileInfo.exists())
    {
        qWarning() << "NIfTI 文件不存在:" << niftiFilePath;
        return false;
    }

    QByteArray utf8Path = niftiFilePath.toUtf8();
    vtkSmartPointer<vtkNIFTIImageReader> reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    reader->SetFileName(utf8Path.constData());
    reader->Update();

    vtkImageData* output = reader->GetOutput();
    if (!output || output->GetNumberOfPoints() == 0)
    {
        qWarning() << "无法读取 NIfTI 文件:" << niftiFilePath;
        return false;
    }

    QString label = QString("NIfTI: %1").arg(fileInfo.fileName());
    return finalizePrimaryVolumeLoad(output, label);
}

bool DicomDataModel::finalizePrimaryVolumeLoad(vtkImageData* imageData, const QString& sourceLabel)
{
    if (!imageData)
    {
        return false;
    }

    m_imageData = imageData;
    m_primaryImageData = m_imageData;
    m_imageData->GetDimensions(m_dims);
    resetBrainSegmentationState(true);

    m_windowWidth = 2000;
    m_windowLevel = 0;

    double* spacing = m_imageData->GetSpacing();
    double* origin = m_imageData->GetOrigin();

    QString info = QString("Dimensions: %1 x %2 x %3\n"
        "Spacing: %4 x %5 x %6 mm\n"
        "Origin: (%7, %8, %9)")
        .arg(m_dims[0]).arg(m_dims[1]).arg(m_dims[2])
        .arg(spacing[0]).arg(spacing[1]).arg(spacing[2])
        .arg(origin[0]).arg(origin[1]).arg(origin[2]);

    QString formattedInfo = sourceLabel.isEmpty()
        ? info
        : QString("%1\n%2").arg(sourceLabel, info);
    m_primaryDicomInfo = formattedInfo;
    m_dicomInfo = formattedInfo;

    resetSlicesToCenter();
    emit dataLoaded();
    return true;
}

bool DicomDataModel::parseBrainSegColorTable()
{
    if (m_brainSegColorTablePath.isEmpty())
    {
        qWarning() << "请先设置 TSV 颜色表路径";
        return false;
    }

    QByteArray utf8Path = m_brainSegColorTablePath.toUtf8();
    std::ifstream file(utf8Path.constData());
    if (!file.is_open())
    {
        qWarning() << "无法打开 TSV 文件:" << m_brainSegColorTablePath;
        return false;
    }

    std::unordered_map<int, BrainLabelColor> parsed;
    std::string line;
    std::getline(file, line);

    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        auto tokens = SplitTSVLine(line);
        if (tokens.size() < 3)
        {
            qWarning() << "TSV 行格式错误，跳过:" << QString::fromStdString(line);
            continue;
        }

        int label = 0;
        try
        {
            label = std::stoi(tokens[0]);
        }
        catch (const std::exception&)
        {
            qWarning() << "无法解析标签索引:" << QString::fromStdString(tokens[0]);
            continue;
        }

        const std::string& colorStr = tokens[2];
        if (colorStr.size() != 7 || colorStr[0] != '#')
        {
            qWarning() << "颜色格式错误:" << QString::fromStdString(colorStr);
            continue;
        }

        try
        {
            int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
            int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
            int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
            parsed[label] = { r, g, b };
        }
        catch (const std::exception&)
        {
            qWarning() << "无法解析颜色值:" << QString::fromStdString(colorStr);
            continue;
        }
    }

    if (parsed.empty())
    {
        qWarning() << "颜色表解析结果为空";
        return false;
    }

    m_brainSegColorTable = std::move(parsed);
    return true;
}

bool DicomDataModel::buildBrainSegStyles(vtkImageData* imageData)
{
    if (!imageData)
    {
        qWarning() << "缺少脑分割数据";
        return false;
    }

    vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
    if (!scalars || scalars->GetNumberOfComponents() != 1)
    {
        qWarning() << "标量数据无效";
        return false;
    }

    std::set<int> uniqueLabels;
    void* dataPtr = scalars->GetVoidPointer(0);
    const vtkIdType tupleCount = scalars->GetNumberOfTuples();

    switch (scalars->GetDataType())
    {
    case VTK_CHAR: CollectSegLabels(static_cast<char*>(dataPtr), tupleCount, uniqueLabels); break;
    case VTK_UNSIGNED_CHAR: CollectSegLabels(static_cast<unsigned char*>(dataPtr), tupleCount, uniqueLabels); break;
    case VTK_SHORT: CollectSegLabels(static_cast<short*>(dataPtr), tupleCount, uniqueLabels); break;
    case VTK_UNSIGNED_SHORT: CollectSegLabels(static_cast<unsigned short*>(dataPtr), tupleCount, uniqueLabels); break;
    case VTK_INT: CollectSegLabels(static_cast<int*>(dataPtr), tupleCount, uniqueLabels); break;
    case VTK_UNSIGNED_INT: CollectSegLabels(static_cast<unsigned int*>(dataPtr), tupleCount, uniqueLabels); break;
    default:
        qWarning() << "不支持的标量类型";
        return false;
    }

    if (uniqueLabels.empty())
    {
        qWarning() << "未找到标签值";
        return false;
    }

    m_brainSegStyles.clear();
    for (int label : uniqueLabels)
    {
        BrainLabelStyle style;
        auto it = m_brainSegColorTable.find(label);
        if (it != m_brainSegColorTable.end())
        {
            style.R = it->second.R / 255.0;
            style.G = it->second.G / 255.0;
            style.B = it->second.B / 255.0;
            style.A = (label == 0) ? 0.0 : 0.7;
        }
        else if (label == 0)
        {
            style.A = 0.0;
        }
        m_brainSegStyles[label] = style;
    }

    for (const auto& entry : m_brainSegColorTable)
    {
        if (m_brainSegStyles.count(entry.first))
        {
            continue;
        }
        BrainLabelStyle style;
        style.R = entry.second.R / 255.0;
        style.G = entry.second.G / 255.0;
        style.B = entry.second.B / 255.0;
        style.A = (entry.first == 0) ? 0.0 : 0.7;
        m_brainSegStyles[entry.first] = style;
    }

    if (m_brainSegStyles.empty())
    {
        return false;
    }

    m_brainSegLookupTable = vtkSmartPointer<vtkLookupTable>::New();
    m_brainSegColorTransfer = vtkSmartPointer<vtkColorTransferFunction>::New();
    m_brainSegOpacityTransfer = vtkSmartPointer<vtkPiecewiseFunction>::New();

    const int maxLabel = m_brainSegStyles.rbegin()->first;
    m_brainSegLookupTable->SetRange(0, maxLabel);
    m_brainSegLookupTable->SetNumberOfTableValues(maxLabel + 1);
    m_brainSegLookupTable->Build();

    for (int i = 0; i <= maxLabel; ++i)
    {
        m_brainSegLookupTable->SetTableValue(i, 0, 0, 0, 0);
    }

    m_brainSegColorTransfer->RemoveAllPoints();
    m_brainSegOpacityTransfer->RemoveAllPoints();

    for (const auto& entry : m_brainSegStyles)
    {
        m_brainSegColorTransfer->AddRGBPoint(entry.first, entry.second.R, entry.second.G, entry.second.B);
        m_brainSegOpacityTransfer->AddPoint(entry.first, entry.second.A);
        m_brainSegLookupTable->SetTableValue(entry.first,
            entry.second.R,
            entry.second.G,
            entry.second.B,
            entry.second.A);
    }

    return true;
}

bool DicomDataModel::loadBrainSegmentation(const QString& niftiFilePath)
{
    QString segPath = normalizeLocalPath(niftiFilePath);
    if (segPath.isEmpty())
    {
        qWarning() << "请提供 NIfTI 文件路径";
        return false;
    }

    if (!QFileInfo::exists(segPath))
    {
        qWarning() << "NIfTI 文件不存在:" << segPath;
        return false;
    }

    if (!parseBrainSegColorTable())
    {
        return false;
    }
    QByteArray utf8Path = segPath.toUtf8();
    vtkSmartPointer<vtkNIFTIImageReader> reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
    reader->SetFileName(utf8Path.constData());
    reader->Update();

    vtkImageData* output = reader->GetOutput();
    if (!output || output->GetNumberOfPoints() == 0)
    {
        qWarning() << "无法读取 NIfTI 文件";
        return false;
    }

    m_brainSegRawData = output;
    if (!buildBrainSegStyles(m_brainSegRawData))
    {
        m_brainSegRawData = nullptr;
        return false;
    }

    m_brainSegColorMapper = vtkSmartPointer<vtkImageMapToColors>::New();
    m_brainSegColorMapper->SetInputData(m_brainSegRawData);
    m_brainSegColorMapper->SetLookupTable(m_brainSegLookupTable);
    m_brainSegColorMapper->PassAlphaToOutputOn();
    m_brainSegColorMapper->Update();

    m_brainSegColorizedData = vtkSmartPointer<vtkImageData>::New();
    m_brainSegColorizedData->ShallowCopy(m_brainSegColorMapper->GetOutput());

    const bool hadPrimaryVolume = m_primaryImageData != nullptr;
    if (!hadPrimaryVolume)
    {
        m_primaryImageData = m_brainSegColorizedData;
        m_imageData = m_brainSegColorizedData;
        m_imageData->GetDimensions(m_dims);
    }
    else
    {
        m_imageData = m_primaryImageData;
    }

    QString segLabel = QString("Brain Segmentation: %1").arg(QFileInfo(segPath).fileName());
    if (hadPrimaryVolume && !m_primaryDicomInfo.isEmpty())
    {
        m_dicomInfo = segLabel + "\n" + m_primaryDicomInfo;
    }
    else
    {
        m_primaryDicomInfo = segLabel;
        m_dicomInfo = segLabel;
    }
    m_brainSegActive = true;

    resetSlicesToCenter();
    emit dataLoaded();
    emit brainSegmentationChanged();
    return true;
}

void DicomDataModel::resetBrainSegmentationState(bool emitSignals)
{
    if (!m_brainSegActive)
    {
        return;
    }

    m_brainSegActive = false;
    m_brainSegRawData = nullptr;
    m_brainSegLookupTable = nullptr;
    m_brainSegColorMapper = nullptr;
    m_brainSegColorTransfer = nullptr;
    m_brainSegOpacityTransfer = nullptr;
    m_brainSegColorizedData = nullptr;
    m_brainSegStyles.clear();

    if (emitSignals)
    {
        emit brainSegmentationChanged();
    }
}

void DicomDataModel::clearBrainSegmentation()
{
    if (!m_brainSegActive)
    {
        return;
    }

    resetBrainSegmentationState(true);

    if (m_primaryImageData)
    {
        m_imageData = m_primaryImageData;
        m_imageData->GetDimensions(m_dims);
        resetSlicesToCenter();
        emit dataLoaded();
    }
}

void DicomDataModel::resetSlicesToCenter()
{
    const int axialCenter = (m_dims[2] > 0) ? m_dims[2] / 2 : 0;
    const int sagittalCenter = (m_dims[0] > 0) ? m_dims[0] / 2 : 0;
    const int coronalCenter = (m_dims[1] > 0) ? m_dims[1] / 2 : 0;

    forceSetAxialSlice(axialCenter);
    forceSetSagittalSlice(sagittalCenter);
    forceSetCoronalSlice(coronalCenter);
}

void DicomDataModel::forceSetAxialSlice(int slice)
{
    if (m_dims[2] <= 0)
    {
        m_axialSlice = 0;
        emit axialSliceChanged(m_axialSlice);
        return;
    }
    int clamped = std::clamp(slice, 0, m_dims[2] - 1);
    if (m_axialSlice == clamped)
    {
        m_axialSlice = -1;
    }
    setAxialSlice(clamped);
}

void DicomDataModel::forceSetSagittalSlice(int slice)
{
    if (m_dims[0] <= 0)
    {
        m_sagittalSlice = 0;
        emit sagittalSliceChanged(m_sagittalSlice);
        return;
    }
    int clamped = std::clamp(slice, 0, m_dims[0] - 1);
    if (m_sagittalSlice == clamped)
    {
        m_sagittalSlice = -1;
    }
    setSagittalSlice(clamped);
}

void DicomDataModel::forceSetCoronalSlice(int slice)
{
    if (m_dims[1] <= 0)
    {
        m_coronalSlice = 0;
        emit coronalSliceChanged(m_coronalSlice);
        return;
    }
    int clamped = std::clamp(slice, 0, m_dims[1] - 1);
    if (m_coronalSlice == clamped)
    {
        m_coronalSlice = -1;
    }
    setCoronalSlice(clamped);
}


