#pragma once
#include <memory>
#include <functional>

#include <vtkSmartPointer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkAxisActor2D.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>

#include <vector>
#include <array>
class DicomDataModel;
enum class SliceOrientation : int;

// 当前左键交互模式
enum class ToolMode
{
    None = 0,
    WindowLevel,      // 左右=窗宽，上下=窗位
    Contrast,         // 对比度（默认仅调窗宽，后续可扩展为 gamma/曲线）
    Pan,              // 平移
    Zoom,             // 缩放（平行投影 scale）
    MeasureDistance,  // 距离测量
    MeasureAngle,     // 角度测量
    AnnotationRectangle, //标注 - 矩形
    AnnotationCircle,    //标注 - 圆形
    AnnotationPen,       //标注 - 画笔
};

// 一次交互所需上下文（由 SliceInteractorStyle 组装）
struct InteractionContext
{
    DicomDataModel* model{ nullptr };
    SliceOrientation orientation{ static_cast<SliceOrientation>(0) };

    vtkRenderWindowInteractor* interactor{ nullptr };
    vtkRenderer* renderer{ nullptr };
    vtkCamera* camera{ nullptr };

    // 可选：用于标尺重算/刷新
    vtkSmartPointer<vtkAxisActor2D> axisActor;
    std::function<void()> rescaleAxisActor;
    std::function<void()> requestRender;

    // 鼠标点
    int startX{ 0 }, startY{ 0 };
    int lastX{ 0 }, lastY{ 0 };

    // WW/WL 起始值
    double startWW{ 0.0 };
    double startWL{ 0.0 };

    // Zoom 起始值
    double startParallelScale{ 0.0 };

    // Pan 起始相机
    double startCamPos[3]{ 0, 0, 0 };
    double startCamFocal[3]{ 0, 0, 0 };

    // ===== 测量（无限个，按切片显示/隐藏）=====
    struct MeasurementItem
    {
        ToolMode type{ ToolMode::None }; // MeasureDistance / MeasureAngle
        bool segMode{ false };           // 创建时是否在 segDataMode
        int sliceNumber{ 0 };            // 创建时所在切片号（按 orientation）
        std::vector<vtkSmartPointer<vtkActor>> actors;                  // 线段等
        vtkSmartPointer<vtkBillboardTextActor3D> textActor;             // 数值文本
    };

    // 未完成的点（距离：2 点，角度：3 点）
    std::vector<std::array<double, 3>> pendingPoints;
    ToolMode pendingMode{ ToolMode::None };

    // 未完成测量的"点"预览（每次点击就显示一个点）
    bool pendingSegMode{ false };
    int pendingSliceNumber{ 0 };
    std::vector<vtkSmartPointer<vtkActor>> pendingPointActors;

    // 角度测量：第二次点击时显示的预览线（p1->p0），第三次点击后并入最终测量
    vtkSmartPointer<vtkActor> pendingAngleFirstLine;

    // 已完成的测量集合
    std::vector<MeasurementItem> measurements;

    // ===== 矩形标注（可多个，按切片显示/隐藏）=====
    struct AnnotationRectItem
    {
        bool segMode{ false };           // 创建时是否在 segDataMode
        int sliceNumber{ 0 };            // 创建时所在切片号
        double startWorld[3]{ 0,0,0 };   // 起始点（世界坐标）
        double endWorld[3]{ 0,0,0 };     // 结束点（世界坐标） 
        std::string labelText;           // 标注文字
        std::vector<vtkSmartPointer<vtkActor>> actors;  // 矩形边框
        vtkSmartPointer<vtkBillboardTextActor3D> textActor;  // 标注文本
    };

    // 矩形标注拖动过程中的临时actor（松手后移入AnnotationRectItem）
    vtkSmartPointer<vtkActor> pendingRectActors[4];  // 四条边
    double pendingRectStart[3]{ 0,0,0 };
    double pendingRectEnd[3]{ 0,0,0 };

    // 已完成的矩形标注集合
    std::vector<AnnotationRectItem> annotations;

    // ===== 圆形标注（可多个，按切片显示/隐藏）=====
    struct AnnotationCircleItem
    {
        bool segMode{ false };           // 创建时是否在 segDataMode
        int sliceNumber{ 0 };            // 创建时所在切片号
        double centerWorld[3]{ 0,0,0 };  // 圆心（世界坐标）
        double radius{ 0.0 };            // 半径
        std::string labelText;           // 标注文字
        std::vector<vtkSmartPointer<vtkActor>> actors;  // 圆形边框
        vtkSmartPointer<vtkBillboardTextActor3D> textActor;  // 标注文本
    };

    // 圆形标注拖动过程中的临时数据
    vtkSmartPointer<vtkActor> pendingCircleActor;
    double pendingCircleCenter[3]{ 0,0,0 };
    double pendingCircleRadius{ 0.0 };

    // 已完成的圆形标注集合
    std::vector<AnnotationCircleItem> circleAnnotations;

    // 标注完成后的回调（通知QML层显示输入框）
    // annotationType: 0=矩形, 1=圆形, 2=画笔
    std::function<void(double screenX, double screenY, int annotationIndex, int annotationType)> onAnnotationCreated;
};

class IInteractionState
{
public:
    virtual ~IInteractionState() = default;

    // 进入：按下时调用
    virtual void OnEnter(InteractionContext& ctx, int x, int y) = 0;
    // 过程：移动时调用（pressed）
    virtual void OnMove(InteractionContext& ctx, int x, int y) = 0;
    // 退出：抬起时调用
    virtual void OnExit(InteractionContext& ctx, int x, int y) = 0;
};

// 工厂：根据工具模式创建状态
std::unique_ptr<IInteractionState> CreateState(ToolMode mode);


