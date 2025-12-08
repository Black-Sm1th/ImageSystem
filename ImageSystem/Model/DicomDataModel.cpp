#include "DicomDataModel.h"
#include <vtkImageSliceMapper.h>
#include <QFile>
#include <QProcess>

// ========== Getter ==========
int DicomDataModel::axialSlice() const { return m_axialSlice; }
int DicomDataModel::sagittalSlice() const { return m_sagittalSlice; }
int DicomDataModel::coronalSlice() const { return m_coronalSlice; }
int DicomDataModel::maxAxialSlice() const { return m_dims[2] - 1; }
int DicomDataModel::maxSagittalSlice() const { return m_dims[0] - 1; }
int DicomDataModel::maxCoronalSlice() const { return m_dims[1] - 1; }

int DicomDataModel::segAxialSlice() const { return m_segAxialSlice; }
int DicomDataModel::segSagittalSlice() const { return m_segSagittalSlice; }
int DicomDataModel::segCoronalSlice() const { return m_segCoronalSlice; }
int DicomDataModel::maxSegAxialSlice() const { return m_segDims[2] - 1; }
int DicomDataModel::maxSegSagittalSlice() const { return m_segDims[0] - 1; }
int DicomDataModel::maxSegCoronalSlice() const { return m_segDims[1] - 1; }

bool DicomDataModel::isSegDataMode() const { return m_isSegDataMode; }

double DicomDataModel::windowWidth() const { return m_windowWidth; }
double DicomDataModel::windowLevel() const { return m_windowLevel; }
QString DicomDataModel::dicomInfo() const { return m_dicomInfo; }
bool DicomDataModel::hasData() const { return m_imageData != nullptr; }

vtkSmartPointer<vtkImageData> DicomDataModel::getImageData() { return m_imageData; }

vtkSmartPointer<vtkImageSlice> DicomDataModel::getSegImageData(int index)
{
    if (!m_region) {
        return nullptr;
    }
    if (index == 0) {
        return m_region->GetAxialSlice();
    }
    if (index == 1) {
        return m_region->GetCoronalSlice();
    }
    if (index == 2) {
        return m_region->GetSagittalSlice();
    }
    return nullptr;
}

vtkSmartPointer<vtkRenderer> DicomDataModel::getSeg3DRenderer()
{
    return m_region ? m_region->Get3DRenderer() : nullptr;
}

BrainSegmentationTableModel* DicomDataModel::getSegmentationTableModel()
{
    return m_segmentationTableModel;
}

// ========== Setter ==========
void DicomDataModel::setAxialSlice(int slice)
{
    if (slice != m_axialSlice && slice >= 0 && slice < m_dims[2]) {
        m_axialSlice = slice;
        emit axialSliceChanged(slice);
    }
}

void DicomDataModel::setSagittalSlice(int slice)
{
    if (slice != m_sagittalSlice && slice >= 0 && slice < m_dims[0]) {
        m_sagittalSlice = slice;
        emit sagittalSliceChanged(slice);
    }
}

void DicomDataModel::setCoronalSlice(int slice)
{
    if (slice != m_coronalSlice && slice >= 0 && slice < m_dims[1]) {
        m_coronalSlice = slice;
        emit coronalSliceChanged(slice);
    }
}

void DicomDataModel::setSegAxialSlice(int slice)
{
    if (slice != m_segAxialSlice && slice >= 0 && slice < m_segDims[2]) {
        m_segAxialSlice = slice;
        emit segAxialSliceChanged(slice);
    }
}

void DicomDataModel::setSegSagittalSlice(int slice)
{
    if (slice != m_segSagittalSlice && slice >= 0 && slice < m_segDims[0]) {
        m_segSagittalSlice = slice;
        emit segSagittalSliceChanged(slice);
    }
}

void DicomDataModel::setSegCoronalSlice(int slice)
{
    if (slice != m_segCoronalSlice && slice >= 0 && slice < m_segDims[1]) {
        m_segCoronalSlice = slice;
        emit segCoronalSliceChanged(slice);
    }
}

void DicomDataModel::setWindowWidth(double width)
{
    if (width != m_windowWidth && width > 0) {
        m_windowWidth = width;
        emit windowWidthChanged(width);
    }
}

void DicomDataModel::setWindowLevel(double level)
{
    if (level != m_windowLevel) {
        m_windowLevel = level;
        emit windowLevelChanged(level);
    }
}

void DicomDataModel::setSegDataMode(bool enabled)
{
    if (m_isSegDataMode != enabled) {
        m_isSegDataMode = enabled;
        emit segDataModeChanged();
    }
}

DicomDataModel::DicomDataModel(QObject* parent)
    : QObject(parent) {
    m_segmentationTableModel = new BrainSegmentationTableModel(this);
}

bool DicomDataModel::loadDicomDirectory(const QString& path) {
    if (path.isEmpty()) return false;

    QString dirPath = path;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }

    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetDirectoryName(dirPath.toStdString().c_str());
    reader->Update();

    if (reader->GetOutput()->GetNumberOfPoints() == 0) {
        qDebug() << "Failed to read DICOM data or directory is empty!";
        return false;
    }

    m_imageData = reader->GetOutput();
    m_imageData->GetDimensions(m_dims);

    // 设置默认窗宽窗位
    m_windowWidth = 2000;
    m_windowLevel = 0;

    // 获取DICOM信息
    double* spacing = m_imageData->GetSpacing();
    double* origin = m_imageData->GetOrigin();

    m_dicomInfo = QString("Dimensions: %1 x %2 x %3\n"
        "Spacing: %4 x %5 x %6 mm\n"
        "Origin: (%7, %8, %9)")
        .arg(m_dims[0]).arg(m_dims[1]).arg(m_dims[2])
        .arg(spacing[0]).arg(spacing[1]).arg(spacing[2])
        .arg(origin[0]).arg(origin[1]).arg(origin[2]);

    emit dataLoaded();
    // 设置默认切片为中间位置
    setAxialSlice(m_dims[2] / 2);
    setSagittalSlice(m_dims[0] / 2);
    setCoronalSlice(m_dims[1] / 2);
    return true;
    return true;
    
}

void DicomDataModel::loadSegBrainDirectory(const QString& path)
{
    if (path.isEmpty()) return;

    QString dirPath = path;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }
    QString mriDirPath = dirPath + "/sourcedata/freesurfer/sub-01/mri";

    QString mgzPath = mriDirPath + "/aparc+aseg.mgz";
    QString niiPath = mriDirPath + "/aparc+aseg.nii.gz";
    
    // 检查nii文件是否存在，如果不存在则从mgz转换
    if (!QFile::exists(niiPath)) {
        qDebug() << QStringLiteral("nii文件不存在，尝试从mgz转换: ") << niiPath;
        
        // 检查mgz文件是否存在
        if (!QFile::exists(mgzPath)) {
            qWarning() << QStringLiteral("mgz文件也不存在，无法转换: ") << mgzPath;
            return;
        }
        
        // 调用转换脚本
        QProcess process;
        process.setProgram("Scripts/mgz2nii.exe");
        process.setArguments({mgzPath, niiPath});
        
        qDebug() << QStringLiteral("开始转换mgz到nii: ") << mgzPath << " -> " << niiPath;
        process.start();
        
        if (!process.waitForFinished(60000)) { // 等待最多60秒
            qWarning() << QStringLiteral("mgz2nii转换超时");
            return;
        }
        
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            qWarning() << QStringLiteral("mgz2nii转换失败，退出代码: ") << process.exitCode();
            qWarning() << QStringLiteral("标准输出: ") << process.readAllStandardOutput();
            qWarning() << QStringLiteral("错误输出: ") << process.readAllStandardError();
            return;
        }
        
        qDebug() << QStringLiteral("mgz2nii转换成功: ") << niiPath;
    } else {
        qDebug() << QStringLiteral("nii文件已存在，直接使用: ") << niiPath;
    }
    
    // 继续正常加载流程
    m_region = std::make_unique<BrainRegionVisualizer>(niiPath.toStdString(), "Scripts/tsv/desc-aseg_dseg_with_chinese.tsv");
    m_region->Initialize();
    
    // 获取SegData的维度信息
    vtkSmartPointer<vtkImageSlice> axialSlice = m_region->GetAxialSlice();
    if (axialSlice && axialSlice->GetMapper()) {
        vtkImageSliceMapper* mapper = vtkImageSliceMapper::SafeDownCast(axialSlice->GetMapper());
        if (mapper && mapper->GetInput()) {
            mapper->GetInput()->GetDimensions(m_segDims);
            qDebug() << "SegData dimensions:" << m_segDims[0] << "x" << m_segDims[1] << "x" << m_segDims[2];
        }
    }
    
    // 加载表格数据
    QVector<SegmentationRegion> regions;
    auto& regionEntries = m_region->Regions();
    for (const auto& entry : regionEntries) {
        SegmentationRegion region;
        region.chineseName = QString::fromStdString(entry.chineseName);
        region.hemisphere = QString(QChar(entry.hemisphere));
        region.volume = entry.volume;
        region.volumePercent = entry.volumePercent;
        region.label = entry.label;
        regions.append(region);
    }
    m_segmentationTableModel->loadRegions(regions);
    
    m_windowWidth = 80;
    m_windowLevel = 40;
    
    // 设置SegData的默认切片为中间位置
    setSegAxialSlice(m_segDims[2] / 2);
    setSegSagittalSlice(m_segDims[0] / 2);
    setSegCoronalSlice(m_segDims[1] / 2);
    
    emit segDataLoaded();
}
