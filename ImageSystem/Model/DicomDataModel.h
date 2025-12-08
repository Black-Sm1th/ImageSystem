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
    int axialSlice() const;
    int sagittalSlice() const;
    int coronalSlice() const;
    int maxAxialSlice() const;
    int maxSagittalSlice() const;
    int maxCoronalSlice() const;
    
    // SegData的切片
    int segAxialSlice() const;
    int segSagittalSlice() const;
    int segCoronalSlice() const;
    int maxSegAxialSlice() const;
    int maxSegSagittalSlice() const;
    int maxSegCoronalSlice() const;
    
    bool isSegDataMode() const;
    
    double windowWidth() const;
    double windowLevel() const;
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
    Q_INVOKABLE bool loadDicomDirectory(const QString& path);

    Q_INVOKABLE void loadSegBrainDirectory(const QString& path);
    
    Q_INVOKABLE void setSegDataMode(bool enabled);

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
    BrainRegionVisualizer* m_region;
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

