#pragma once
#include <QObject.h>
#include <QDebug.h>
#include "Modules/CommonFunc.h"
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkDICOMImageReader.h>
#include "Modules/BrainRegionVisualizer.h"
#include "Modules/BrainMetrics.h"
#include "Model/BrainSegmentationTableModel.h"
#include <string>
#include <memory>

class DicomDataModel : public QObject
{
    SINGLETON_CLASS(DicomDataModel)
        Q_OBJECT

        QUICK_PROPERTY(bool, showOriginal)

        Q_PROPERTY(int axialSlice READ axialSlice WRITE setAxialSlice NOTIFY axialSliceChanged)
        Q_PROPERTY(int sagittalSlice READ sagittalSlice WRITE setSagittalSlice NOTIFY sagittalSliceChanged)
        Q_PROPERTY(int coronalSlice READ coronalSlice WRITE setCoronalSlice NOTIFY coronalSliceChanged)
        Q_PROPERTY(int maxAxialSlice READ maxAxialSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxSagittalSlice READ maxSagittalSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxCoronalSlice READ maxCoronalSlice NOTIFY dataLoaded)
        Q_PROPERTY(int dimX READ dimX NOTIFY dimensionsChanged)
        Q_PROPERTY(int dimY READ dimY NOTIFY dimensionsChanged)
        Q_PROPERTY(int dimZ READ dimZ NOTIFY dimensionsChanged)
        Q_PROPERTY(double spacingX READ spacingX NOTIFY dimensionsChanged)
        Q_PROPERTY(double spacingY READ spacingY NOTIFY dimensionsChanged)
        Q_PROPERTY(double spacingZ READ spacingZ NOTIFY dimensionsChanged)
        // SegData独立的切片属性
        Q_PROPERTY(int segAxialSlice READ segAxialSlice WRITE setSegAxialSlice NOTIFY segAxialSliceChanged)
        Q_PROPERTY(int segSagittalSlice READ segSagittalSlice WRITE setSegSagittalSlice NOTIFY segSagittalSliceChanged)
        Q_PROPERTY(int segCoronalSlice READ segCoronalSlice WRITE setSegCoronalSlice NOTIFY segCoronalSliceChanged)
        Q_PROPERTY(int maxSegAxialSlice READ maxSegAxialSlice NOTIFY segDataLoaded)
        Q_PROPERTY(int maxSegSagittalSlice READ maxSegSagittalSlice NOTIFY segDataLoaded)
        Q_PROPERTY(int maxSegCoronalSlice READ maxSegCoronalSlice NOTIFY segDataLoaded)
        Q_PROPERTY(bool isSegDataMode READ isSegDataMode NOTIFY segDataModeChanged)
        // 分割视图显示模式: 0=叠加, 1=仅原图, 2=仅分割
        Q_PROPERTY(int segDisplayMode READ segDisplayMode WRITE setSegDisplayMode NOTIFY segDisplayModeChanged)
        // 分割图叠加透明度 (0.0 - 1.0)
        Q_PROPERTY(double segOverlayOpacity READ segOverlayOpacity WRITE setSegOverlayOpacity NOTIFY segOverlayOpacityChanged)
        // 是否有原始图像可用
        Q_PROPERTY(bool hasRawImage READ hasRawImage NOTIFY segDataLoaded)
        Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
        Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowLevelChanged)
        // 当前交互模式（键盘/工具栏切换用），与 ViewController/InteractionState.h 的 ToolMode 数值保持一致
        Q_PROPERTY(int toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
        // QML 十字线开关（Key_8 toggle）
        Q_PROPERTY(bool crosshairEnabled READ crosshairEnabled WRITE setCrosshairEnabled NOTIFY crosshairEnabledChanged)
        Q_PROPERTY(QString dicomInfo READ dicomInfo NOTIFY dataLoaded)
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataLoaded)
        
public:
    // 普通数据的切片
    int axialSlice() const;
    int sagittalSlice() const;
    int coronalSlice() const;
    int maxAxialSlice() const;
    int maxSagittalSlice() const;
    int maxCoronalSlice() const;
    int dimX() const;
    int dimY() const;
    int dimZ() const;
    double spacingX() const;
    double spacingY() const;
    double spacingZ() const;
    
    // SegData的切片
    int segAxialSlice() const;
    int segSagittalSlice() const;
    int segCoronalSlice() const;
    int maxSegAxialSlice() const;
    int maxSegSagittalSlice() const;
    int maxSegCoronalSlice() const;
    
    bool isSegDataMode() const;
    
    // 分割视图显示模式相关
    int segDisplayMode() const;
    double segOverlayOpacity() const;
    bool hasRawImage() const;
    
    double windowWidth() const;
    double windowLevel() const;
    int toolMode() const;
    bool crosshairEnabled() const;
    QString dicomInfo() const;
    bool hasData() const;
    vtkSmartPointer<vtkImageData> getImageData();
    vtkSmartPointer<vtkImageSlice> getSegImageData(int index);
    vtkSmartPointer<vtkRenderer> getSeg3DRenderer();
    
    BrainSegmentationTableModel* getSegmentationTableModel();

    void setAxialSlice(int slice);
    void setSagittalSlice(int slice);
    void setCoronalSlice(int slice);
    
    void setSegAxialSlice(int slice);
    void setSegSagittalSlice(int slice);
    void setSegCoronalSlice(int slice);

    void setWindowWidth(double width);
    void setWindowLevel(double level);
    Q_INVOKABLE void setToolMode(int mode);
    Q_INVOKABLE void setCrosshairEnabled(bool enabled);

    // Key_7: reset 所有交互状态（WW/WL、相机平移/缩放、测量等）
    Q_INVOKABLE void resetAllInteractions();
    Q_INVOKABLE bool loadDicomDirectory(const QString& path);

    Q_INVOKABLE void loadSegBrainDirectory(const QString& path);
    
    // 设置脑区可见性
    Q_INVOKABLE void setRegionVisible(int row, bool visible);
    
    Q_INVOKABLE void setSegDataMode(bool enabled);
    
    // 分割视图显示模式设置
    Q_INVOKABLE void setSegDisplayMode(int mode);
    Q_INVOKABLE void setSegOverlayOpacity(double opacity);

    void generateSegDataPNGs(const QString& path, QString& axialMidPngPath, QString& coronalMidPngPath, QString& sagittalMidPngPath, QString& seg3dPngPath);
signals:
    void axialSliceChanged(int slice);
    void sagittalSliceChanged(int slice);
    void coronalSliceChanged(int slice);
    void segAxialSliceChanged(int slice);
    void segSagittalSliceChanged(int slice);
    void segCoronalSliceChanged(int slice);
    void windowWidthChanged(double width);
    void windowLevelChanged(double level);
    void toolModeChanged(int mode);
    void crosshairEnabledChanged(bool enabled);
    void dataLoaded();
    void segDataLoaded();
    void segDataModeChanged();
    void segDisplayModeChanged();
    void segOverlayOpacityChanged();
    void segLoadingStarted();
    void segLoadingProgress(int percent, const QString& message);
    void segLoadingFinished(bool success, const QString& message);
    void segRefreshRenderer();
    void segSliceRefreshRequested();  // 仅刷新2D切片视图
    void interactionResetRequested();
    void dimensionsChanged();

private:
    void finalizeSegDataLoad(std::unique_ptr<BrainRegionVisualizer> region);

    vtkSmartPointer<vtkImageData> m_imageData;
    std::unique_ptr<BrainRegionVisualizer> m_region;
    std::unique_ptr<BrainRegionVisualizer> m_pendingRegion;
    BrainSegmentationTableModel* m_segmentationTableModel;
    int m_dims[3] = {1, 1, 1};
    int m_segDims[3] = {1, 1, 1};
    double m_spacing[3] = {1.0, 1.0, 1.0};
    double m_segSpacing[3] = {1.0, 1.0, 1.0};
    int m_axialSlice = 0;
    int m_sagittalSlice = 0;
    int m_coronalSlice = 0;
    int m_segAxialSlice = 0;
    int m_segSagittalSlice = 0;
    int m_segCoronalSlice = 0;
    bool m_isSegDataMode = false;
    int m_segDisplayMode = 0;       // 0=叠加, 1=仅原图, 2=仅分割
    double m_segOverlayOpacity = 0.6;
    double m_windowWidth = 2000;
    double m_windowLevel = 0;
    int m_toolMode = 0; // 默认 WindowLevel
    bool m_crosshairEnabled = false;
    QString m_dicomInfo;
    bool m_segLoadingInProgress = false;
    QString m_statsDir;
    std::shared_ptr<BrainMetrics> m_brainMetrics;
    int m_metricsLoadSerial = 0;
};

