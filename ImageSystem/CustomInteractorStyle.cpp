#include "CustomInteractorStyle.h"
#include <vtkCellPicker.h>
#include <vtkActor.h>
#include <vtkDataSet.h>
#include <vtkGenericCell.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkLineSource.h>
#include <vtkExtractSelection.h>
#include <vtkSelectionNode.h>
#include <vtkSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

vtkStandardNewMacro(CustomInteractorStyle);

void CustomInteractorStyle::OnLeftButtonDown()
{
    // 在每次选取前清除旧的高亮显示
    for (auto& actor : this->highlightedActors) {
        this->GetDefaultRenderer()->RemoveActor(actor);
    }
    this->highlightedActors.clear();// 清空高亮显示actors的列表

    int* clickPos = this->GetInteractor()->GetEventPosition();
    vtkNew<vtkCellPicker> picker;
    //picker->SetTolerance(0.0005); // 设置选取容差
    picker->SetTolerance(0.01); // 增加选取容差以提高拾取的易用性
    picker->Pick(clickPos[0], clickPos[1], 0, this->GetDefaultRenderer());

    vtkIdType cellId = picker->GetCellId();

    if (cellId < 0)
    {
        // 没有选中任何单元
        std::cout << "No cell picked." << std::endl;
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }
    else
    {
        vtkActor* actor = picker->GetActor();
        if (actor)
        {
            vtkDataSet* dataSet = actor->GetMapper()->GetInput();

            // 获取拾取到的单元
            vtkSmartPointer<vtkGenericCell> pickedCell = vtkSmartPointer<vtkGenericCell>::New();
            picker->GetDataSet()->GetCell(cellId, pickedCell);

            int cellType = pickedCell->GetCellType();

            vtkIdList* ids = pickedCell->GetPointIds();

            if (cellType == VTK_VERTEX || cellType == VTK_POLY_VERTEX) // 选中了点	
            {
                double* pointPos = dataSet->GetPoint(ids->GetId(0));// 获取选中点的位置

                vtkNew<vtkSphereSource> sphereSource;
                sphereSource->SetCenter(pointPos);
                sphereSource->SetRadius(0.01);

                vtkNew<vtkPolyDataMapper> mapper;
                mapper->SetInputConnection(sphereSource->GetOutputPort());

                vtkNew<vtkActor> actor;
                actor->SetMapper(mapper);
                actor->GetProperty()->SetColor(0.0, 0.0, 1.0); // 蓝色

                this->GetDefaultRenderer()->AddActor(actor);
                this->highlightedActors.push_back(actor); // 将新的高亮actor添加到列表中
            }
            else if (cellType == VTK_LINE || cellType == VTK_POLY_LINE) //选中了边
            {
                // 由于边由两个点组成，我们需要获取这两个点的位置
                double point1Pos[3];
                double point2Pos[3];
                dataSet->GetPoint(ids->GetId(0), point1Pos);
                dataSet->GetPoint(ids->GetId(1), point2Pos);

                // 创建一个线段来表示选中的边
                vtkNew<vtkLineSource> lineSource;
                lineSource->SetPoint1(point1Pos);
                lineSource->SetPoint2(point2Pos);

                // 创建映射器并将线段的输出连接到映射器
                vtkNew<vtkPolyDataMapper> lineMapper;
                lineMapper->SetInputConnection(lineSource->GetOutputPort());

                // 创建actor并设置映射器
                vtkNew<vtkActor> lineActor;
                lineActor->SetMapper(lineMapper);
                lineActor->GetProperty()->SetColor(0.0, 1.0, 0.0); // 设置为绿色表示高亮
                lineActor->GetProperty()->SetLineWidth(3);

                // 将actor添加到渲染器中
                this->GetDefaultRenderer()->AddActor(lineActor);
                this->highlightedActors.push_back(lineActor); // 将新的高亮actor添加到列表中
            }
            else
            {
                vtkNew<vtkIdTypeArray> ids_array;
                ids_array->SetNumberOfComponents(1);
                ids_array->InsertNextValue(cellId);

                vtkNew<vtkSelectionNode> selectionNode;
                selectionNode->SetFieldType(vtkSelectionNode::CELL);
                selectionNode->SetContentType(vtkSelectionNode::INDICES);
                selectionNode->SetSelectionList(ids_array);

                vtkNew<vtkSelection> selection;
                selection->AddNode(selectionNode);

                vtkNew<vtkExtractSelection> extractSelection;
                extractSelection->SetInputData(0, dataSet);
                extractSelection->SetInputData(1, selection);
                extractSelection->Update();

                // Create a mapper and actor for selected cell
                vtkNew<vtkGeometryFilter> geometryFilter;
                geometryFilter->SetInputConnection(extractSelection->GetOutputPort());
                geometryFilter->Update();

                vtkNew<vtkPolyDataMapper> mapper;
                mapper->SetInputConnection(geometryFilter->GetOutputPort());

                vtkNew<vtkActor> actor;
                actor->SetMapper(mapper);
                actor->GetProperty()->SetColor(1.0, 0.0, 0.0); // 设置为红色高亮

                this->GetDefaultRenderer()->AddActor(actor);
                this->highlightedActors.push_back(actor); // 将新的高亮actor添加到列表中
            }
        }
        else
        {
            vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
        }
    }
    this->GetDefaultRenderer()->GetRenderWindow()->Render();
}