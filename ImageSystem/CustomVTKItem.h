
// CustomVTKItem.h
#ifndef CUSTOMVTKITEM_H
#define CUSTOMVTKITEM_H

#include <QQuickVTKRenderItem.h> 
#include <QString>

class CustomInteractorStyle;

class CustomVTKItem : public QQuickVTKRenderItem
{
    Q_OBJECT
    Q_PROPERTY(QString dicomDirectory READ dicomDirectory WRITE setDicomDirectory NOTIFY dicomDirectoryChanged)
    
public:
    CustomVTKItem();

    void InitInteractorStyle();
    void InitData();
    
    QString dicomDirectory() const { return m_dicomDirectory; }
    void setDicomDirectory(const QString& dir);
    
signals:
    void dicomDirectoryChanged();

protected:
    bool event(QEvent* ev) override;
    
private:
    void LoadDICOMData();
    QString m_dicomDirectory;
};

#endif 