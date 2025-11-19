
// CustomVTKItem.cpp
#include "CustomVTKItem.h"
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkDICOMImageReader.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkNamedColors.h>
#include "CustomInteractorStyle.h"
#include <QDebug>

CustomVTKItem::CustomVTKItem()
{
    m_dicomDirectory = "";
}

void CustomVTKItem::InitInteractorStyle()
{
	vtkNew<CustomInteractorStyle> style;
	this->renderWindow()->renderWindow()->GetInteractor()->SetInteractorStyle(style);
	style->SetDefaultRenderer(renderer());
}

void CustomVTKItem::InitData()
{
    vtkNew<vtkNamedColors> colors;
    this->renderer()->SetBackground(colors->GetColor3d("SlateGray").GetData());
    
    // 如果已经设置了DICOM目录，则加载数据
    if (!m_dicomDirectory.isEmpty()) {
        LoadDICOMData();
    } else {
        qDebug() << "DICOM directory not set. Please set it using setDicomDirectory()";
    }
}

void CustomVTKItem::setDicomDirectory(const QString& dir)
{
    if (m_dicomDirectory != dir) {
        m_dicomDirectory = dir;
        emit dicomDirectoryChanged();
        
        // 如果renderer已经初始化，立即加载数据
        if (renderer()) {
            LoadDICOMData();
            update();
        }
    }
}

void CustomVTKItem::LoadDICOMData()
{
    if (m_dicomDirectory.isEmpty()) {
        qDebug() << "DICOM directory is empty!";
        return;
    }
    
    qDebug() << "Loading DICOM data from:" << m_dicomDirectory;
    
    // 创建DICOM读取器
    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetDirectoryName(m_dicomDirectory.toStdString().c_str());
    reader->Update();
    
    // 检查是否成功读取
    if (reader->GetOutput()->GetNumberOfPoints() == 0) {
        qDebug() << "Failed to read DICOM data or directory is empty!";
        return;
    }
    
    qDebug() << "DICOM data loaded successfully";
    
    // 创建体渲染映射器
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputConnection(reader->GetOutputPort());
    
    // 创建颜色传输函数
    vtkSmartPointer<vtkColorTransferFunction> colorFunc = vtkSmartPointer<vtkColorTransferFunction>::New();
    colorFunc->AddRGBPoint(-3024, 0.0, 0.0, 0.0);
    colorFunc->AddRGBPoint(-77, 0.54902, 0.25098, 0.14902);
    colorFunc->AddRGBPoint(94, 0.882353, 0.603922, 0.290196);
    colorFunc->AddRGBPoint(179, 1.0, 0.937033, 0.954531);
    colorFunc->AddRGBPoint(260, 0.615686, 0.0, 0.0);
    colorFunc->AddRGBPoint(3071, 0.827451, 0.658824, 1.0);
    
    // 创建不透明度传输函数
    vtkSmartPointer<vtkPiecewiseFunction> opacityFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityFunc->AddPoint(-3024, 0.0);
    opacityFunc->AddPoint(-77, 0.0);
    opacityFunc->AddPoint(94, 0.29);
    opacityFunc->AddPoint(179, 0.55);
    opacityFunc->AddPoint(260, 0.84);
    opacityFunc->AddPoint(3071, 0.875);
    
    // 创建梯度不透明度函数
    vtkSmartPointer<vtkPiecewiseFunction> gradientFunc = vtkSmartPointer<vtkPiecewiseFunction>::New();
    gradientFunc->AddPoint(0, 0.0);
    gradientFunc->AddPoint(90, 0.5);
    gradientFunc->AddPoint(100, 1.0);
    
    // 创建体属性
    vtkSmartPointer<vtkVolumeProperty> volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorFunc);
    volumeProperty->SetScalarOpacity(opacityFunc);
    volumeProperty->SetGradientOpacity(gradientFunc);
    volumeProperty->SetInterpolationTypeToLinear();
    volumeProperty->ShadeOn();
    volumeProperty->SetAmbient(0.4);
    volumeProperty->SetDiffuse(0.6);
    volumeProperty->SetSpecular(0.2);
    
    // 创建体对象
    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);
    
    // 添加到渲染器
    this->renderer()->AddVolume(volume);
    this->renderer()->ResetCamera();
    
    qDebug() << "Volume rendering setup complete";
}

bool CustomVTKItem::event(QEvent* ev)
{
	return QQuickVTKRenderItem::event(ev);
}

