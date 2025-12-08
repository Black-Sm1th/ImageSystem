#include "DicomDataModel.h"

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
    m_windowWidth = 80;
    m_windowLevel = 40;

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
    dirPath += "/sourcedata/freesurfer/sub-01/mri/aparc+aseg.nii.gz";
    qDebug() << dirPath;
    m_region = new BrainRegionVisualizer(dirPath.toStdString(), "Scripts/tsv/desc-aseg_dseg_with_chinese.tsv");
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
    
    m_windowWidth = 0;
    m_windowLevel = 0;
    
    // 设置SegData的默认切片为中间位置
    setSegAxialSlice(m_segDims[2] / 2);
    setSegSagittalSlice(m_segDims[0] / 2);
    setSegCoronalSlice(m_segDims[1] / 2);
    
    emit segDataLoaded();
}
