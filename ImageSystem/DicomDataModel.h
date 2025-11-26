#pragma once
#include <QObject.h>
#include <QDebug.h>
#include "CommonFunc.h"
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkDICOMImageReader.h>
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
        Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged)
        Q_PROPERTY(double windowLevel READ windowLevel WRITE setWindowLevel NOTIFY windowLevelChanged)
        Q_PROPERTY(QString dicomInfo READ dicomInfo NOTIFY dataLoaded)
        Q_PROPERTY(bool hasData READ hasData NOTIFY dataLoaded)

public:
    int axialSlice() const { return m_axialSlice; }
    int sagittalSlice() const { return m_sagittalSlice; }
    int coronalSlice() const { return m_coronalSlice; }
    int maxAxialSlice() const { return m_dims[2] - 1; }
    int maxSagittalSlice() const { return m_dims[0] - 1; }
    int maxCoronalSlice() const { return m_dims[1] - 1; }
    double windowWidth() const { return m_windowWidth; }
    double windowLevel() const { return m_windowLevel; }
    QString dicomInfo() const { return m_dicomInfo; }
    bool hasData() const { return m_imageData != nullptr; }
    vtkSmartPointer<vtkImageData> getImageData() { return m_imageData; }
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

signals:
    void axialSliceChanged(int slice);
    void sagittalSliceChanged(int slice);
    void coronalSliceChanged(int slice);
    void windowWidthChanged(double width);
    void windowLevelChanged(double level);
    void dataLoaded();

private:
    vtkSmartPointer<vtkImageData> m_imageData;
    int m_dims[3] = {1, 1, 1};
    int m_axialSlice = 0;
    int m_sagittalSlice = 0;
    int m_coronalSlice = 0;
    double m_windowWidth = 2000;
    double m_windowLevel = 0;
    QString m_dicomInfo;
};

