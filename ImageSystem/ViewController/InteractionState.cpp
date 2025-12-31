#include "InteractionState.h"

#include "Model/DicomDataModel.h"

#include <vtkRendererCollection.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropPicker.h>
#include <vtkSphereSource.h>
#include <cmath>

namespace
{
    vtkRenderer* ResolveRenderer(vtkRenderWindowInteractor* interactor, vtkRenderer* current)
    {
        if (current) return current;
        if (!interactor || !interactor->GetRenderWindow()) return nullptr;
        auto* renderers = interactor->GetRenderWindow()->GetRenderers();
        return renderers ? renderers->GetFirstRenderer() : nullptr;
    }

    vtkCamera* ResolveCamera(vtkRenderer* renderer)
    {
        return renderer ? renderer->GetActiveCamera() : nullptr;
    }

    int CurrentSliceNumber(const InteractionContext& ctx)
    {
        if (!ctx.model) return 0;
        const bool seg = ctx.model->isSegDataMode();
        switch (ctx.orientation)
        {
        case static_cast<SliceOrientation>(0): // Axial
            return seg ? ctx.model->segAxialSlice() : ctx.model->axialSlice();
        case static_cast<SliceOrientation>(1): // Sagittal
            return seg ? ctx.model->segSagittalSlice() : ctx.model->sagittalSlice();
        case static_cast<SliceOrientation>(2): // Coronal
            return seg ? ctx.model->segCoronalSlice() : ctx.model->coronalSlice();
        default:
            return 0;
        }
    }

    bool PickOnSlice(InteractionContext& ctx, int x, int y, double outWorld[3])
    {
        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        if (!ctx.renderer) return false;

        auto picker = vtkSmartPointer<vtkPropPicker>::New();
        const int ok = picker->Pick(x, y, 0, ctx.renderer);
        if (!ok) return false;

        picker->GetPickPosition(outWorld);
        return true;
    }

    vtkSmartPointer<vtkActor> CreateWorldLine(const double p0[3], const double p1[3], const double rgb[3], double lineWidth = 2.0)
    {
        auto line = vtkSmartPointer<vtkLineSource>::New();
        line->SetPoint1(p0);
        line->SetPoint2(p1);
        line->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(line->GetOutputPort());

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
        actor->GetProperty()->SetLineWidth(lineWidth);
        actor->PickableOff();
        return actor;
    }

    vtkSmartPointer<vtkBillboardTextActor3D> CreateWorldText(const double pos[3], const std::string& text, const double rgb[3])
    {
        auto t = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        t->SetPosition(pos[0], pos[1], pos[2]);
        t->SetInput(text.c_str());
        t->GetTextProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
        t->GetTextProperty()->SetFontSize(16);
        t->GetTextProperty()->SetBold(1);
        t->PickableOff();
        return t;
    }

    vtkSmartPointer<vtkActor> CreateWorldPoint(const double p[3], const double rgb[3], double radius = 1.5)
    {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetCenter(p);
        sphere->SetRadius(radius);
        sphere->SetThetaResolution(16);
        sphere->SetPhiResolution(16);
        sphere->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(rgb[0], rgb[1], rgb[2]);
        actor->PickableOff();
        return actor;
    }

    std::array<double, 3> MidPoint(const std::array<double, 3>& a, const std::array<double, 3>& b)
    {
        return { (a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, (a[2] + b[2]) * 0.5 };
    }
}

// ===== Window/Level =====
class WindowLevelState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        ctx.startX = ctx.lastX = x;
        ctx.startY = ctx.lastY = y;
        if (ctx.model)
        {
            ctx.startWW = ctx.model->windowWidth();
            ctx.startWL = ctx.model->windowLevel();
        }
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        if (!ctx.model) return;

        const int dx = x - ctx.startX;
        const int dy = y - ctx.startY;

        // 约定：左右=窗宽，上下=窗位
        double ww = ctx.startWW + dx * 4.0;
        double wl = ctx.startWL + dy * 4.0;
        if (ww < 1.0) ww = 1.0;

        ctx.model->setWindowWidth(ww);
        ctx.model->setWindowLevel(wl);
        ctx.lastX = x;
        ctx.lastY = y;
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }
};

// ===== Contrast =====
class ContrastState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        ctx.startX = ctx.lastX = x;
        ctx.startY = ctx.lastY = y;
        if (ctx.model)
        {
            ctx.startWW = ctx.model->windowWidth();
            ctx.startWL = ctx.model->windowLevel();
        }
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        if (!ctx.model) return;

        const int dx = x - ctx.startX;
        const int dy = y - ctx.startY;

        // 对比度通常用乘法更“线性好用”
        // dx>0：对比度增强（窗宽变大）；dx<0：窗宽变小
        const double factor = std::exp(static_cast<double>(dx) * 0.005);
        double ww = ctx.startWW * factor;
        if (ww < 1.0) ww = 1.0;

        // dy 小幅调亮度（窗位）
        double wl = ctx.startWL + dy * 2.0;

        ctx.model->setWindowWidth(ww);
        ctx.model->setWindowLevel(wl);
        ctx.lastX = x;
        ctx.lastY = y;
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }
};

// ===== Zoom =====
class ZoomState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        ctx.startX = ctx.lastX = x;
        ctx.startY = ctx.lastY = y;

        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        ctx.camera = ResolveCamera(ctx.renderer);
        if (ctx.camera)
        {
            ctx.startParallelScale = ctx.camera->GetParallelScale();
        }
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        ctx.camera = ResolveCamera(ctx.renderer);
        if (!ctx.camera) return;

        const int dy = y - ctx.lastY;
        const double currentScale = ctx.camera->GetParallelScale();
        const double scaleFactor = 1.0 + (dy * 0.01);
        const double newScale = currentScale * scaleFactor;

        if (newScale > 1.0 && newScale < 10000.0)
        {
            ctx.camera->SetParallelScale(newScale);
            if (ctx.rescaleAxisActor) ctx.rescaleAxisActor();
            if (ctx.requestRender) ctx.requestRender();
            else if (ctx.interactor) ctx.interactor->Render();
        }
        ctx.lastX = x;
        ctx.lastY = y;
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }
};

// ===== Pan =====
class PanState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        ctx.startX = ctx.lastX = x;
        ctx.startY = ctx.lastY = y;

        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        ctx.camera = ResolveCamera(ctx.renderer);
        if (ctx.camera)
        {
            ctx.camera->GetPosition(ctx.startCamPos);
            ctx.camera->GetFocalPoint(ctx.startCamFocal);
        }
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        ctx.camera = ResolveCamera(ctx.renderer);
        if (!ctx.renderer || !ctx.camera) return;

        const int dx = x - ctx.lastX;
        const int dy = y - ctx.lastY;

        const double scale = ctx.camera->GetParallelScale();
        int* size = ctx.renderer->GetSize();
        if (!size || size[0] <= 0 || size[1] <= 0) return;

        const double fx = -dx * scale * 2.0 / size[1];
        const double fy = -dy * scale * 2.0 / size[1];

        double* position = ctx.camera->GetPosition();
        double* focalPoint = ctx.camera->GetFocalPoint();
        double* viewUp = ctx.camera->GetViewUp();

        double vpn[3] = { position[0] - focalPoint[0], position[1] - focalPoint[1], position[2] - focalPoint[2] };
        double norm = std::sqrt(vpn[0] * vpn[0] + vpn[1] * vpn[1] + vpn[2] * vpn[2]);
        if (norm > 0.0)
        {
            vpn[0] /= norm; vpn[1] /= norm; vpn[2] /= norm;
        }

        double right[3] = {
            viewUp[1] * vpn[2] - viewUp[2] * vpn[1],
            viewUp[2] * vpn[0] - viewUp[0] * vpn[2],
            viewUp[0] * vpn[1] - viewUp[1] * vpn[0]
        };

        double newPos[3] = {
            position[0] + right[0] * fx + viewUp[0] * fy,
            position[1] + right[1] * fx + viewUp[1] * fy,
            position[2] + right[2] * fx + viewUp[2] * fy
        };
        double newFocal[3] = {
            focalPoint[0] + right[0] * fx + viewUp[0] * fy,
            focalPoint[1] + right[1] * fx + viewUp[1] * fy,
            focalPoint[2] + right[2] * fx + viewUp[2] * fy
        };

        ctx.camera->SetPosition(newPos);
        ctx.camera->SetFocalPoint(newFocal);
        if (ctx.rescaleAxisActor) ctx.rescaleAxisActor();
        if (ctx.requestRender) ctx.requestRender();
        else if (ctx.interactor) ctx.interactor->Render();

        ctx.lastX = x;
        ctx.lastY = y;
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }
};

// ===== Measure Distance（点击两点生成线；可无限条；按切片显示/隐藏）=====
class MeasureDistanceState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        if (!ctx.interactor) return;
        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        if (!ctx.renderer) return;

        // 切换到距离模式时清理未完成点
        if (ctx.pendingMode != ToolMode::MeasureDistance) {
            ctx.pendingMode = ToolMode::MeasureDistance;
            ctx.pendingPoints.clear();
            ctx.pendingPointActors.clear();
        }

        double w[3]{ 0,0,0 };
        if (!PickOnSlice(ctx, x, y, w)) {
            return; // 第一次点不上的常见原因就是 pick 失败
        }

        ctx.pendingPoints.push_back({ w[0], w[1], w[2] });
        if (ctx.pendingPoints.size() == 1) {
            ctx.pendingSegMode = ctx.model ? ctx.model->isSegDataMode() : false;
            ctx.pendingSliceNumber = CurrentSliceNumber(ctx);
        }

        // 每次点击都立刻落一个点（预览点）
        const double rgbPt[3]{ 1.0, 1.0, 0.0 };
        auto ptActor = CreateWorldPoint(w, rgbPt, 1.5);
        ctx.pendingPointActors.push_back(ptActor);
        ctx.renderer->AddActor(ptActor);
        if (ctx.requestRender) ctx.requestRender();
        if (ctx.pendingPoints.size() < 2) {
            return;
        }

        const auto p0 = ctx.pendingPoints[0];
        const auto p1 = ctx.pendingPoints[1];
        ctx.pendingPoints.clear();

        // 把预览点并入完成的测量项里（点也要保留）
        std::vector<vtkSmartPointer<vtkActor>> pointActors = std::move(ctx.pendingPointActors);
        ctx.pendingPointActors.clear();

        const double dx = p0[0] - p1[0];
        const double dy = p0[1] - p1[1];
        const double dz = p0[2] - p1[2];
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        const double rgb[3]{ 1.0, 1.0, 0.0 };
        auto lineActor = CreateWorldLine(p0.data(), p1.data(), rgb, 2.5);
        const auto mid = MidPoint(p0, p1);
        auto textActor = CreateWorldText(mid.data(), std::to_string(dist).substr(0, 6) + " mm", rgb);

        InteractionContext::MeasurementItem item;
        item.type = ToolMode::MeasureDistance;
        item.segMode = ctx.pendingSegMode;
        item.sliceNumber = ctx.pendingSliceNumber;
        for (auto& pa : pointActors) item.actors.push_back(pa);
        item.actors.push_back(lineActor);
        item.textActor = textActor;
        ctx.measurements.push_back(item);

        ctx.renderer->AddActor(lineActor);
        ctx.renderer->AddActor(textActor);
        if (ctx.requestRender) ctx.requestRender();
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
        // 点击式测量：不需要拖动
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }
};

// ===== Measure Angle（点击三点确定角度；可无限个；按切片显示/隐藏）=====
class MeasureAngleState final : public IInteractionState
{
public:
    void OnEnter(InteractionContext& ctx, int x, int y) override
    {
        if (!ctx.interactor) return;
        ctx.renderer = ResolveRenderer(ctx.interactor, ctx.renderer);
        if (!ctx.renderer) return;

        if (ctx.pendingMode != ToolMode::MeasureAngle) {
            ctx.pendingMode = ToolMode::MeasureAngle;
            ctx.pendingPoints.clear();
            ctx.pendingPointActors.clear();
            if (ctx.pendingAngleFirstLine) {
                ctx.renderer->RemoveViewProp(ctx.pendingAngleFirstLine);
                ctx.pendingAngleFirstLine = nullptr;
            }
        }

        double w[3]{ 0,0,0 };
        if (!PickOnSlice(ctx, x, y, w)) {
            return;
        }

        ctx.pendingPoints.push_back({ w[0], w[1], w[2] });
        if (ctx.pendingPoints.size() == 1) {
            ctx.pendingSegMode = ctx.model ? ctx.model->isSegDataMode() : false;
            ctx.pendingSliceNumber = CurrentSliceNumber(ctx);
        }

        // 每次点击都立刻落一个点（预览点）
        // 角度顶点是第二个点（p1），给它一个不同颜色更好辨识
        const double rgbPt[3]{ (ctx.pendingPoints.size() == 2) ? 0.2 : 1.0,
                               (ctx.pendingPoints.size() == 2) ? 1.0 : 0.6,
                               (ctx.pendingPoints.size() == 2) ? 0.2 : 0.0 };
        auto ptActor = CreateWorldPoint(w, rgbPt, 1.5);
        ctx.pendingPointActors.push_back(ptActor);
        ctx.renderer->AddActor(ptActor);
        if (ctx.requestRender) ctx.requestRender();

        // 第二次点击：把前两点连成一条直线（预览第一条边，p1->p0）
        if (ctx.pendingPoints.size() == 2) {
            const auto p0 = ctx.pendingPoints[0];
            const auto p1 = ctx.pendingPoints[1]; // 暂定为顶点
            const double rgb[3]{ 1.0, 0.6, 0.0 };
            ctx.pendingAngleFirstLine = CreateWorldLine(p1.data(), p0.data(), rgb, 2.5);
            ctx.renderer->AddActor(ctx.pendingAngleFirstLine);
            if (ctx.requestRender) ctx.requestRender();
            return;
        }

        if (ctx.pendingPoints.size() < 3) {
            return;
        }

        const auto p0 = ctx.pendingPoints[0];
        const auto p1 = ctx.pendingPoints[1]; // 顶点
        const auto p2 = ctx.pendingPoints[2];
        ctx.pendingPoints.clear();

        // 把预览点并入完成的测量项里（点也要保留）
        std::vector<vtkSmartPointer<vtkActor>> pointActors = std::move(ctx.pendingPointActors);
        ctx.pendingPointActors.clear();

        // 计算角度（p0-p1 与 p2-p1）
        const double v1[3]{ p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2] };
        const double v2[3]{ p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
        const double n1 = std::sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);
        const double n2 = std::sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);
        double deg = 0.0;
        if (n1 > 0.0 && n2 > 0.0) {
            double dot = (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]) / (n1*n2);
            dot = std::max(-1.0, std::min(1.0, dot));
            deg = std::acos(dot) * 180.0 / 3.141592653589793;
        }

        const double rgb[3]{ 1.0, 0.6, 0.0 };
        // 若第二次点击已创建预览线，则复用作为最终第一条边
        const bool reusedPreviewLine = (ctx.pendingAngleFirstLine != nullptr);
        vtkSmartPointer<vtkActor> a1 = reusedPreviewLine ? ctx.pendingAngleFirstLine : CreateWorldLine(p1.data(), p0.data(), rgb, 2.5);
        ctx.pendingAngleFirstLine = nullptr;
        auto a2 = CreateWorldLine(p1.data(), p2.data(), rgb, 2.5);
        auto textActor = CreateWorldText(p1.data(), std::to_string(deg).substr(0, 6) + " deg", rgb);

        InteractionContext::MeasurementItem item;
        item.type = ToolMode::MeasureAngle;
        item.segMode = ctx.pendingSegMode;
        item.sliceNumber = ctx.pendingSliceNumber;
        for (auto& pa : pointActors) item.actors.push_back(pa);
        item.actors.push_back(a1);
        item.actors.push_back(a2);
        item.textActor = textActor;
        ctx.measurements.push_back(item);

        // a1 可能已在第二次点击时 AddActor 过，这里避免重复添加
        if (!reusedPreviewLine) ctx.renderer->AddActor(a1);
        ctx.renderer->AddActor(a2);
        ctx.renderer->AddActor(textActor);
        if (ctx.requestRender) ctx.requestRender();
    }

    void OnMove(InteractionContext& ctx, int x, int y) override
    {
        (void)ctx; (void)x; (void)y;
    }

    void OnExit(InteractionContext& ctx, int x, int y) override
    {
        (void)x; (void)y;
        (void)ctx;
    }
};

std::unique_ptr<IInteractionState> CreateState(ToolMode mode)
{
    switch (mode)
    {
    case ToolMode::WindowLevel:     return std::make_unique<WindowLevelState>();
    case ToolMode::Contrast:        return std::make_unique<ContrastState>();
    case ToolMode::Pan:             return std::make_unique<PanState>();
    case ToolMode::Zoom:            return std::make_unique<ZoomState>();
    case ToolMode::MeasureDistance: return std::make_unique<MeasureDistanceState>();
    case ToolMode::MeasureAngle:    return std::make_unique<MeasureAngleState>();
    default:                        return nullptr;
    }
}


