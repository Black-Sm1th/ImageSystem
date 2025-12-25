#include "DicomDataModel.h"
#include <vtkImageSliceMapper.h>
#include <QFile>
#include <QProcess>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <QtConcurrent/QtConcurrent>

// ========== Getter ==========
int DicomDataModel::axialSlice() const { return m_axialSlice; }
int DicomDataModel::sagittalSlice() const { return m_sagittalSlice; }
int DicomDataModel::coronalSlice() const { return m_coronalSlice; }
int DicomDataModel::maxAxialSlice() const { return m_dims[2] - 1; }
int DicomDataModel::maxSagittalSlice() const { return m_dims[0] - 1; }
int DicomDataModel::maxCoronalSlice() const { return m_dims[1] - 1; }
int DicomDataModel::dimX() const { return m_isSegDataMode ? m_segDims[0] : m_dims[0]; }
int DicomDataModel::dimY() const { return m_isSegDataMode ? m_segDims[1] : m_dims[1]; }
int DicomDataModel::dimZ() const { return m_isSegDataMode ? m_segDims[2] : m_dims[2]; }

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

    // 切换到普通数据模式
    setSegDataMode(false);
    
    emit dataLoaded();
    // 设置默认切片为中间位置
    setAxialSlice(m_dims[2] / 2);
    setSagittalSlice(m_dims[0] / 2);
    setCoronalSlice(m_dims[1] / 2);
    return true;
}

void DicomDataModel::loadSegBrainDirectory(const QString& path)
{
    if (path.isEmpty()) {
        emit segLoadingFinished(false, QStringLiteral("分割路径为空"));
        return;
    }

    if (m_segLoadingInProgress) {
        return;
    }

    QString dirPath = path;
    if (dirPath.startsWith("file:///")) {
        dirPath = dirPath.mid(8);
    }
    
    // 首先尝试fMRIPrep格式的路径
    QString mriDirPath = dirPath + "/sourcedata/freesurfer/sub-01/mri";
    m_statsDir = dirPath + "/sourcedata/freesurfer/sub-01/stats";
    QString mgzPath = mriDirPath + "/aparc+aseg.mgz";
    QString niiPath = mriDirPath + "/aparc+aseg.nii.gz";
    QString origMgzPath = mriDirPath + "/T1.mgz";
    QString origNiiPathGz = mriDirPath + "/T1.nii.gz";
    QString origNiiPath = mriDirPath + "/T1.nii";
    
    // 如果fMRIPrep格式不存在，尝试DeepPrep格式
    if (!QFile::exists(mgzPath) && !QFile::exists(niiPath)) {
        qDebug() << QStringLiteral("未找到fMRIPrep格式的分割文件，尝试DeepPrep格式...");
        mriDirPath = dirPath + "/Recon/fsaverage/mri";
        mgzPath = mriDirPath + "/aparc+aseg.mgz";
        niiPath = mriDirPath + "/aparc+aseg.nii.gz";
        m_statsDir = dirPath + "/Recon/fsaverage/stats";
        
        if (QFile::exists(mgzPath) || QFile::exists(niiPath)) {
            qDebug() << QStringLiteral("检测到DeepPrep格式的分割文件!");
        }
    }

    // ===== 初始化/加载 BrainMetrics（只读 stats，不影响 RegionEntry/QML 列表）=====
    // 注意：异步加载，避免阻塞 UI；用 serial 防止快速重复加载导致旧结果覆盖新结果。
    const QString statsDirToLoad = m_statsDir;
    const int serial = ++m_metricsLoadSerial;
    QtConcurrent::run([this, statsDirToLoad, serial]() {
        auto metrics = std::make_shared<BrainMetrics>(statsDirToLoad.toStdString());
        const bool ok = metrics->Load();
        QMetaObject::invokeMethod(this, [this, ok, metrics, statsDirToLoad, serial]() {
            if (serial != m_metricsLoadSerial) {
                return; // 已有更新的 load 请求
            }
            if (ok) {
                m_brainMetrics = metrics;
                qDebug() << QStringLiteral("BrainMetrics 加载成功: ") << statsDirToLoad;
            } else {
                m_brainMetrics.reset();
                qWarning() << QStringLiteral("BrainMetrics 加载失败: ") << statsDirToLoad;
            }
        }, Qt::QueuedConnection);
    });


    // 通知开始
    emit segLoadingStarted();

    // 检查nii文件是否存在，如果不存在则从mgz转换
    if (!QFile::exists(niiPath)) {
        qDebug() << QStringLiteral("nii文件不存在，尝试从mgz转换: ") << niiPath;

        // 检查mgz文件是否存在
        if (!QFile::exists(mgzPath)) {
            qWarning() << QStringLiteral("mgz文件也不存在，无法转换: ") << mgzPath;
            emit segLoadingFinished(false, QStringLiteral("未找到分割文件: %1").arg(mgzPath));
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
            emit segLoadingFinished(false, QStringLiteral("mgz2nii转换超时"));
            return;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            qWarning() << QStringLiteral("mgz2nii转换失败，退出代码: ") << process.exitCode();
            qWarning() << QStringLiteral("标准输出: ") << process.readAllStandardOutput();
            qWarning() << QStringLiteral("错误输出: ") << process.readAllStandardError();
            emit segLoadingFinished(false, QStringLiteral("mgz2nii转换失败"));
            return;
        }

        qDebug() << QStringLiteral("mgz2nii转换成功: ") << niiPath;
    } else {
        qDebug() << QStringLiteral("nii文件已存在，直接使用: ") << niiPath;
    }

    // 检查原始orig nii是否存在，不存在则从orig mgz转换（如有）
    QString origNiiToUse;
    if (QFile::exists(origNiiPathGz)) {
        origNiiToUse = origNiiPathGz;
        qDebug() << QStringLiteral("检测到orig nii.gz: ") << origNiiPathGz;
    } else if (QFile::exists(origNiiPath)) {
        origNiiToUse = origNiiPath;
        qDebug() << QStringLiteral("检测到orig nii: ") << origNiiPath;
    } else {
        qDebug() << QStringLiteral("原始orig nii不存在，尝试从mgz转换: ") << origNiiPath;

        if (QFile::exists(origMgzPath)) {
            QProcess process;
            process.setProgram("Scripts/mgz2nii.exe");
            process.setArguments({origMgzPath, origNiiPath});

            qDebug() << QStringLiteral("开始转换orig mgz到nii: ") << origMgzPath << " -> " << origNiiPath;
            process.start();

            if (!process.waitForFinished(60000)) {
                qWarning() << QStringLiteral("orig mgz2nii转换超时");
            } else if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                qWarning() << QStringLiteral("orig mgz2nii转换失败，退出代码: ") << process.exitCode();
                qWarning() << QStringLiteral("orig 标准输出: ") << process.readAllStandardOutput();
                qWarning() << QStringLiteral("orig 错误输出: ") << process.readAllStandardError();
            } else {
                qDebug() << QStringLiteral("orig mgz2nii转换成功: ") << origNiiPath;
                origNiiToUse = origNiiPath;
            }
        } else {
            qWarning() << QStringLiteral("orig mgz文件不存在，无法转换: ") << origMgzPath;
        }
    }

    if (origNiiToUse.isEmpty()) {
        // 兜底：如果转换失败但产生了gz文件也尝试
        if (QFile::exists(origNiiPathGz)) {
            origNiiToUse = origNiiPathGz;
        } else if (QFile::exists(origNiiPath)) {
            origNiiToUse = origNiiPath;
        }
    }

    m_segLoadingInProgress = true;

    const QString colorTablePath = QStringLiteral("Scripts/tsv/desc-aseg_dseg_with_chinese.tsv");
    m_pendingRegion = std::make_unique<BrainRegionVisualizer>(niiPath.toStdString(), colorTablePath.toStdString(), origNiiToUse.toStdString());
    m_pendingRegion->SetProgressCallback([this](int percent, const std::string& message) {
        QString text = QString::fromStdString(message);
        QMetaObject::invokeMethod(this, [this, percent, text]() {
            emit segLoadingProgress(percent, text);
        }, Qt::QueuedConnection);
    });

    BrainRegionVisualizer* regionRaw = m_pendingRegion.get();
    QtConcurrent::run([this, regionRaw]() {
        bool ok = regionRaw->Initialize();
        QMetaObject::invokeMethod(this, [this, ok]() {
            if (!ok) {
                m_pendingRegion.reset();
                m_segLoadingInProgress = false;
                emit segLoadingFinished(false, QStringLiteral("分割数据加载失败"));
                return;
            }
            finalizeSegDataLoad(std::move(m_pendingRegion));
            m_segLoadingInProgress = false;
            emit segLoadingFinished(true, QString());
        }, Qt::QueuedConnection);
    });
}

void DicomDataModel::finalizeSegDataLoad(std::unique_ptr<BrainRegionVisualizer> region)
{
    if (!region) {
        return;
    }

    m_region = std::move(region);

    vtkSmartPointer<vtkImageSlice> axialSlice = m_region->GetAxialSlice();
    if (axialSlice && axialSlice->GetMapper()) {
        vtkImageSliceMapper* mapper = vtkImageSliceMapper::SafeDownCast(axialSlice->GetMapper());
        if (mapper && mapper->GetInput()) {
            mapper->GetInput()->GetDimensions(m_segDims);
            qDebug() << "SegData dimensions:" << m_segDims[0] << "x" << m_segDims[1] << "x" << m_segDims[2];
        }
    }

    QVector<SegmentationRegion> regions;
    auto& regionEntries = m_region->Regions();

    // 按左右配对把 actor 放在一起（L 在前、R 在后），无配对的照常追加
    std::unordered_map<int, size_t> labelToIndex;
    labelToIndex.reserve(regionEntries.size());
    for (size_t i = 0; i < regionEntries.size(); ++i) {
        labelToIndex[regionEntries[i].label] = i;
    }
    std::unordered_set<int> processed;
    processed.reserve(regionEntries.size());

    auto appendEntry = [&](const RegionEntry& entry) {
        SegmentationRegion region;
        region.chineseName = QString::fromStdString(entry.chineseName);
        region.hemisphere = QString(QChar(entry.hemisphere));
        // entry.volume 当前为 mm^3，如需 cm^3 显示则除以 1000
        region.volume = entry.volume / 1000.0;
        region.volumePercent = entry.volumePercent;
        region.label = entry.label;
        region.colorR = entry.colorR;
        region.colorG = entry.colorG;
        region.colorB = entry.colorB;
        region.colorA = entry.colorA;
        region.partnerLabel = entry.partnerLabel;
        region.asymmetryIndex = entry.asymmetryIndex;
        region.visible = true;
        regions.append(region);
        processed.insert(entry.label);
    };

    for (const auto& entry : regionEntries) {
        if (entry.label == 0) continue;
        if (processed.count(entry.label)) continue;

        int partner = entry.partnerLabel;
        auto pit = labelToIndex.find(partner);
        bool hasPartner = (partner != -1 && pit != labelToIndex.end());

        if (!hasPartner) {
            appendEntry(entry);
            continue;
        }

        const auto& partnerEntry = regionEntries[pit->second];
        if (processed.count(partnerEntry.label)) {
            appendEntry(entry);
            continue;
        }

        // 确保 L 在前
        if (entry.hemisphere == 'R' && partnerEntry.hemisphere == 'L') {
            appendEntry(partnerEntry);
            appendEntry(entry);
        } else {
            appendEntry(entry);
            appendEntry(partnerEntry);
        }
    }

    m_segmentationTableModel->loadRegions(regions);

    // 先切换到SegData模式，再设置切片
    // 这样可以避免切片信号触发时模式还未切换导致的问题
    setSegDataMode(true);


    emit segDataLoaded();
    // 设置SegData的默认切片为中间位置
    setSegAxialSlice(m_segDims[2] / 2);
    setSegSagittalSlice(m_segDims[0] / 2);
    setSegCoronalSlice(m_segDims[1] / 2);
}

void DicomDataModel::setRegionVisible(int row, bool visible)
{
    if (!m_region) {
        return;
    }
    
    // 更新表格模型的visible状态
    m_segmentationTableModel->setRegionVisible(row, visible);
    
    // 获取该行的label
    auto& regionEntries = m_region->Regions();
    if (row < 0 || row >= static_cast<int>(regionEntries.size())) {
        return;
    }
    
    // 从表格模型获取对应行的label
    QModelIndex idx = m_segmentationTableModel->index(row, 0);
    int label = m_segmentationTableModel->data(idx, BrainSegmentationTableModel::LabelRole).toInt();
    
    // 调用BrainRegionVisualizer设置可见性
    m_region->SetActorVisible(label, visible);

    emit segRefreshRenderer();
}
