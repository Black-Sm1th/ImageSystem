#ifndef CUSTOMINTERACTORSTYLE_H
#define CUSTOMINTERACTORSTYLE_H

#include <vtkInteractorStyleTrackballCamera.h>

class CustomInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
	static CustomInteractorStyle* New();
	vtkTypeMacro(CustomInteractorStyle, vtkInteractorStyleTrackballCamera);

	virtual void OnLeftButtonDown() override;

private:
	std::vector<vtkSmartPointer<vtkActor>> highlightedActors;
};

#endif