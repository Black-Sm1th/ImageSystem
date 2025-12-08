#pragma once
#include <QObject.h>
#include <QDebug.h>
#include "Modules/CommonFunc.h"
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkDICOMImageReader.h>
#include "Modules/BrainRegionVisualizer.h"
#include "Model/BrainSegmentationTableModel.h"

class DicomDataModel : public QObject
{
    SINGLETON_CLASS(DicomDataModel)
    Q_OBJECT
        Q_PROPERTY(int axialSlice READ axialSlice WRITE setAxialSlice NOTIFY axialSliceChanged)
        Q_PROPERTY(int sagittalSlice READ sagittalSlice WRITE setSagittalSlice NOTIFY sagittalSliceChanged)
        Q_PROPERTY(int coronalSlice READ coronalSlice WRITE setCoronalSlice NOTIFY coronalSliceChanged)
        Q_PROPERTY(int maxAxialSlice READ maxAxialSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxSagittalSlice READ maxSagittalSlice NOTIFY dataLoaded)
        Q_PROPERTY(int maxCoronalSlice READ maxCoronalSlice NOTIFY dataLoaded)
        // SegData独立的切片属性
        Q_PROPERTY(int segAxialSlice READ segAxialSlice WRITE setSegAxialSlice NOTIFY segAxialSliceChanged)
        Q_PROPERTY(int segSagittalSlice READ segSagittalSlice WRITE setSegSagittalSlice NOTIFY segSagittalSliceChanged)
        Q_PROPERTY(int segCoronalSlice READ segCoronalSlice WRITE setSegCoronalSlice NOTIFY segCoronalSliceChanged)
        Q_PROPERTY(int maxSegAxialSlice READ maxSegAxialSlice NOTIFY segDataLoaded)
        Q_PROPERTY(int maxSegSagittalSlice READ maxSegSagittalSlice NOTIFY segDataLoaded)
        Q_PROPERTY(int maxSegCoronalSlice READ maxSegCoronalSlice NOTIFY segDataLoaded)
        Q_PROPERTY(bool isSegDataMode READ isSegDataMode NOTIFY segDataModeChanged)
        Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
        Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowLevelChanged)
        Q_PROPERTY(QString dicomInfo READ dicomInfo NOTIFY dataLoaded)
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataLoaded)

public:
    // 普通数据的切片
    int axialSlice() const { return m_axialSlice; }
    int sagittalSlice() const { return m_sagittalSlice; }
    int coronalSlice() const { return m_coronalSlice; }
    int maxAxialSlice() const { return m_dims[2] - 1; }
    int maxSagittalSlice() const { return m_dims[0] - 1; }
    int maxCoronalSlice() const { return m_dims[1] - 1; }
    
    // SegData的切片
    int segAxialSlice() const { return m_segAxialSlice; }
    int segSagittalSlice() const { return m_segSagittalSlice; }
    int segCoronalSlice() const { return m_segCoronalSlice; }
    int maxSegAxialSlice() const { return m_segDims[2] - 1; }
    int maxSegSagittalSlice() const { return m_segDims[0] - 1; }
    int maxSegCoronalSlice() const { return m_segDims[1] - 1; }
    
    bool isSegDataMode() const { return m_isSegDataMode; }
    
    double windowWidth() const { return m_windowWidth; }
    double windowLevel() const { return m_windowLevel; }
    QString dicomInfo() const { return m_dicomInfo; }
    bool hasData() const { return m_imageData != nullptr; }
    vtkSmartPointer<vtkImageData> getImageData() { return m_imageData; }
    vtkSmartPointer<vtkImageSlice> getSegImageData(int index) {
        if (index == 0) {
            return m_region->GetAxialSlice();
        }if (index == 1) {
            return m_region->GetCoronalSlice();
        }if (index == 2) {
            return m_region->GetSagittalSlice();
        }
    }
    vtkSmartPointer<vtkRenderer> getSeg3DRenderer() {return m_region->Get3DRenderer();}
    
    BrainSegmentationTableModel* getSegmentationTableModel() { return m_segmentationTableModel; }

    void setAxialSlice(int slice) {
        if (slice != m_axialSlice && slice >= 0 && slice < m_dims[2]) {
            m_axialSlice = slice;
            emit axialSliceChanged(slice);
        }
    }

    void setSagittalSlice(int slice) {
        if (slice != m_sagittalSlice && slice >= 0 && slice < m_dims[0]) {
            m_sagittalSlice = slice;
            emit sagittalSliceChanged(slice);
        }
    }

    void setCoronalSlice(int slice) {
        if (slice != m_coronalSlice && slice >= 0 && slice < m_dims[1]) {
            m_coronalSlice = slice;
            emit coronalSliceChanged(slice);
        }
    }
    
    void setSegAxialSlice(int slice) {
        if (slice != m_segAxialSlice && slice >= 0 && slice < m_segDims[2]) {
            m_segAxialSlice = slice;
            emit segAxialSliceChanged(slice);
        }
    }

    void setSegSagittalSlice(int slice) {
        if (slice != m_segSagittalSlice && slice >= 0 && slice < m_segDims[0]) {
            m_segSagittalSlice = slice;
            emit segSagittalSliceChanged(slice);
        }
    }

    void setSegCoronalSlice(int slice) {
        if (slice != m_segCoronalSlice && slice >= 0 && slice < m_segDims[1]) {
            m_segCoronalSlice = slice;
            emit segCoronalSliceChanged(slice);
        }
    }

    void setWindowWidth(double width) {
        if (width != m_windowWidth && width > 0) {
            m_windowWidth = width;
            emit windowWidthChanged(width);
        }
    }

    void setWindowLevel(double level) {
        if (level != m_windowLevel) {
            m_windowLevel = level;
            emit windowLevelChanged(level);
        }
    }
    Q_INVOKABLE bool loadDicomDirectory(const QString& path);

    Q_INVOKABLE void loadSegBrainDirectory(const QString& path);
    
    Q_INVOKABLE void setSegDataMode(bool enabled) {
        if (m_isSegDataMode != enabled) {
            m_isSegDataMode = enabled;
            emit segDataModeChanged();
        }
    }

signals:
    void axialSliceChanged(int slice);
    void sagittalSliceChanged(int slice);
    void coronalSliceChanged(int slice);
    void segAxialSliceChanged(int slice);
    void segSagittalSliceChanged(int slice);
    void segCoronalSliceChanged(int slice);
    void windowWidthChanged(double width);
    void windowLevelChanged(double level);
    void dataLoaded();
    void segDataLoaded();
    void segDataModeChanged();

private:
    vtkSmartPointer<vtkImageData> m_imageData;
    std::unique_ptr<BrainRegionVisualizer> m_region;
    BrainSegmentationTableModel* m_segmentationTableModel;
    int m_dims[3] = {1, 1, 1};
    int m_segDims[3] = {1, 1, 1};
    int m_axialSlice = 0;
    int m_sagittalSlice = 0;
    int m_coronalSlice = 0;
    int m_segAxialSlice = 0;
    int m_segSagittalSlice = 0;
    int m_segCoronalSlice = 0;
    bool m_isSegDataMode = false;
    double m_windowWidth = 2000;
    double m_windowLevel = 0;
    QString m_dicomInfo;
};

