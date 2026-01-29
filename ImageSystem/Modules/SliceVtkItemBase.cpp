#include "SliceVtkItemBase.h"
#include "Modules/CommonFunc.h"
#include "ViewController/MainViewController.h"
#include <algorithm>
#include <vtkTransform.h>
#include <QQuickItemGrabResult>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QFont>
#include <QPainterPath>
vtkStandardNewMacro(SliceInteractorStyle);
vtkStandardNewMacro(SliceViewData);
vtkStandardNewMacro(VolumeViewData);

// ===== SliceInteractorStyle implementations =====
SliceInteractorStyle::SliceInteractorStyle()
    : m_orientation(SliceOrientation::Axial)
{
    m_dataModel = GET_SINGLETON(DicomDataModel);
    m_ctx.model = m_dataModel;
    m_ctx.orientation = m_orientation;
}

void SliceInteractorStyle::SetOrientation(SliceOrientation orientation)
{
    m_orientation = orientation;
    m_ctx.orientation = orientation;
}

void SliceInteractorStyle::SetToolMode(ToolMode mode)
{
    m_toolMode = mode;
    // 同步到全局模型，便于从 Qt 层/全局键盘事件切换
    if (m_dataModel) {
        m_dataModel->setToolMode(static_cast<int>(mode));
    }
}

void SliceInteractorStyle::setAxisActor(vtkSmartPointer<vtkAxisActor2D> axisActor)
{
    m_axisActor = axisActor;
    m_ctx.axisActor = axisActor;
}

void SliceInteractorStyle::rescaleAxisActor()
{
    if (!m_axisActor) {
        return;
    }

    vtkRenderWindowInteractor* interactor = this->GetInteractor();
    if (!interactor || !interactor->GetRenderWindow()) {
        return;
    }

    vtkRenderer* renderer = this->GetCurrentRenderer();
    if (!renderer) {
        vtkRendererCollection* renderers = interactor->GetRenderWindow()->GetRenderers();
        renderer = renderers ? renderers->GetFirstRenderer() : nullptr;
    }
    if (!renderer) {
        return;
    }

    vtkCamera* camera = renderer->GetActiveCamera();
    if (!camera) {
        return;
    }

    int* size = renderer->GetSize();
    if (!size || size[0] <= 0 || size[1] <= 0) {
        return;
    }

    const double parallelScale = camera->GetParallelScale();
    if (parallelScale <= 0.0) {
        return;
    }

    // 平行投影下：可见世界高度 = 2 * parallelScale（单位与数据一致，这里按 mm 使用）
    const double mmPerPixel = (2.0 * parallelScale) / static_cast<double>(size[1]);

    // ===== 固定长度标尺（不随缩放“跳变”）=====
    // 固定物理长度：你可以把这里改成 20/50/100 mm
    const double lengthMm = 50.0;
    const double lengthPx = lengthMm / mmPerPixel;

    // 放在图像最左侧：竖直标尺（垂直居中）
    // margin 越小越贴边；设为 1-2 可以避免被边界裁剪
    const int margin = 2;
    const int x0 = margin;
    const int y0Raw = static_cast<int>(std::round((static_cast<double>(size[1]) - lengthPx) * 0.5));
    const int y0 = std::max(0, y0Raw);

    m_axisActor->GetPoint1Coordinate()->SetValue(x0, y0);
    m_axisActor->GetPoint2Coordinate()->SetValue(x0, y0 + lengthPx);
    m_axisActor->SetRange(0.0, lengthMm);

    // 去掉文字（不显示标题/刻度数字）
    m_axisActor->TitleVisibilityOff();
    m_axisActor->LabelVisibilityOff();
    m_axisActor->TickVisibilityOn();
    //m_axisActor->MinorTicksOff();
    m_axisActor->Modified();
}

void SliceInteractorStyle::OnMouseWheelForward()
{
    int currentSlice = getCurrentSlice();
    int maxSlice = getMaxSlice();
    if (currentSlice < maxSlice) {
        setSlice(currentSlice + 1);
    }
}

void SliceInteractorStyle::OnMouseWheelBackward()
{
    int currentSlice = getCurrentSlice();
    if (currentSlice > 0) {
        setSlice(currentSlice - 1);
    }
}

void SliceInteractorStyle::OnLeftButtonDown()
{
    BeginInteraction(std::nullopt);
}

void SliceInteractorStyle::OnLeftButtonUp()
{
    vtkRenderWindowInteractor* interactor = this->GetInteractor();
    int* pos = interactor ? interactor->GetEventPosition() : nullptr;
    const int x = pos ? pos[0] : 0;
    const int y = pos ? pos[1] : 0;

    if (m_state) {
        m_state->OnExit(m_ctx, x, y);
        m_state.reset();
    }
}

void SliceInteractorStyle::OnRightButtonDown()
{
    // 十字线启用时禁止缩放（因为十字线位置会失效）
    if (m_dataModel->crosshairEnabled()) {
        return;
    }
    // 右键始终缩放（不受当前左键 toolMode 影响）
    BeginInteraction(ToolMode::Zoom);
}

void SliceInteractorStyle::OnRightButtonUp()
{
    OnLeftButtonUp();
}

void SliceInteractorStyle::OnMiddleButtonDown()
{
    // 十字线启用时禁止平移（因为十字线位置会失效）
    if (m_dataModel->crosshairEnabled()) {
        return;
    }
    // 中键始终平移（不受当前左键 toolMode 影响）
    BeginInteraction(ToolMode::Pan);
}

void SliceInteractorStyle::BeginInteraction(std::optional<ToolMode> forcedMode)
{
    vtkRenderWindowInteractor* interactor = this->GetInteractor();
    if (!interactor) {
        return;
    }

    int* pos = interactor->GetEventPosition();
    const int x = pos ? pos[0] : 0;
    const int y = pos ? pos[1] : 0;

    // 组装上下文（每次按下都刷新 renderer/camera 指针）
    m_ctx.model = m_dataModel;
    m_ctx.orientation = m_orientation;
    m_ctx.interactor = interactor;
    m_ctx.renderer = this->GetCurrentRenderer();
    if (!m_ctx.renderer && interactor->GetRenderWindow()) {
        vtkRendererCollection* renderers = interactor->GetRenderWindow()->GetRenderers();
        m_ctx.renderer = renderers ? renderers->GetFirstRenderer() : nullptr;
    }
    m_ctx.camera = m_ctx.renderer ? m_ctx.renderer->GetActiveCamera() : nullptr;
    m_ctx.axisActor = m_axisActor;
    m_ctx.rescaleAxisActor = [this]() { this->rescaleAxisActor(); };
    m_ctx.requestRender = [this]() { if (this->Interactor) this->Interactor->Render(); };

    // 标注回调已经在 SetAnnotationCallback 中设置，无需每次重新设置

    ToolMode effective = m_toolMode;
    if (forcedMode.has_value()) {
        // 右/中键强制模式：不要被 DataModel.toolMode 覆盖
        effective = *forcedMode;
    }
    else {
        // 默认从全局模型读取当前工具（这样键盘/工具栏统一生效）
        if (m_dataModel) {
            effective = static_cast<ToolMode>(m_dataModel->toolMode());
        }

        // 支持按键修饰符临时切换（可按需调整映射）
        // Shift+Ctrl: 距离测量；Shift+Alt: 角度测量；Shift: 平移；Ctrl: 缩放；Alt: 对比度；否则使用当前工具
        const bool shift = interactor->GetShiftKey();
        const bool ctrl = interactor->GetControlKey();
        const bool alt = interactor->GetAltKey();
        if (shift && ctrl) effective = ToolMode::MeasureDistance;
        else if (shift && alt) effective = ToolMode::MeasureAngle;
        else if (shift) effective = ToolMode::Pan;
        else if (ctrl) effective = ToolMode::Zoom;
        else if (alt) effective = ToolMode::Contrast;
    }

    // 如果离开测量工具，清理未完成的“点”预览（避免残留在画面上）
    if (effective != ToolMode::MeasureDistance && effective != ToolMode::MeasureAngle) {
        if (m_ctx.renderer) {
            for (auto& a : m_ctx.pendingPointActors) {
                if (a) m_ctx.renderer->RemoveViewProp(a);
            }
            if (m_ctx.pendingAngleFirstLine) {
                m_ctx.renderer->RemoveViewProp(m_ctx.pendingAngleFirstLine);
            }
        }
        m_ctx.pendingPointActors.clear();
        m_ctx.pendingPoints.clear();
        m_ctx.pendingMode = ToolMode::None;
        m_ctx.pendingAngleFirstLine = nullptr;
    }

    m_state = CreateState(effective);
    if (m_state) {
        m_state->OnEnter(m_ctx, x, y);
    }
}

void SliceInteractorStyle::OnMiddleButtonUp()
{
    OnLeftButtonUp();
}

void SliceInteractorStyle::OnMouseMove()
{
    vtkRenderWindowInteractor* interactor = this->GetInteractor();
    if (!interactor) return;

    int* pos = interactor->GetEventPosition();
    const int x = pos ? pos[0] : 0;
    const int y = pos ? pos[1] : 0;

    if (m_state) {
        m_state->OnMove(m_ctx, x, y);
    }
}

void SliceInteractorStyle::OnKeyPress()
{
    vtkRenderWindowInteractor* interactor = this->GetInteractor();
    if (!interactor) {
        vtkInteractorStyleImage::OnKeyPress();
        return;
    }

    const char key = interactor->GetKeyCode(); // '1'..'6'
    switch (key)
    {
    case '1':
        SetToolMode(ToolMode::WindowLevel);
        qDebug() << "[Interaction] ToolMode = WindowLevel (1)";
        break;
    case '2':
        SetToolMode(ToolMode::Contrast);
        qDebug() << "[Interaction] ToolMode = Contrast (2)";
        break;
    case '3':
        SetToolMode(ToolMode::Zoom);
        qDebug() << "[Interaction] ToolMode = Zoom (3)";
        break;
    case '4':
        SetToolMode(ToolMode::Pan);
        qDebug() << "[Interaction] ToolMode = Pan (4)";
        break;
    case '5':
        SetToolMode(ToolMode::MeasureDistance);
        qDebug() << "[Interaction] ToolMode = MeasureDistance (5)";
        break;
    case '6':
        SetToolMode(ToolMode::MeasureAngle);
        qDebug() << "[Interaction] ToolMode = MeasureAngle (6)";
        break;
    case '0':
        SetToolMode(ToolMode::None);
        qDebug() << "[Interaction] ToolMode = None (0)";
        break;
    default:
        break;
    }

    vtkInteractorStyleImage::OnKeyPress();
}

void SliceInteractorStyle::ResetInteractionState()
{
    // 取消当前 state
    if (m_state) {
        m_state.reset();
    }

    // 清除测量：从 renderer 移除所有 actor，并清空列表
    if (m_ctx.renderer) {
        for (auto& it : m_ctx.measurements) {
            for (auto& a : it.actors) {
                // 统一用 vtkProp 级别移除，避免类型不匹配
                if (a) m_ctx.renderer->RemoveViewProp(a);
            }
            if (it.textActor) {
                m_ctx.renderer->RemoveViewProp(it.textActor);
            }
        }
        // 清除未完成测量的"点"预览
        for (auto& a : m_ctx.pendingPointActors) {
            if (a) m_ctx.renderer->RemoveViewProp(a);
        }
        // 清除角度测量的预览线
        if (m_ctx.pendingAngleFirstLine) {
            m_ctx.renderer->RemoveViewProp(m_ctx.pendingAngleFirstLine);
            m_ctx.pendingAngleFirstLine = nullptr;
        }
        // 清除矩形标注
        for (auto& it : m_ctx.annotations) {
            for (auto& a : it.actors) {
                if (a) m_ctx.renderer->RemoveViewProp(a);
            }
            if (it.textActor) {
                m_ctx.renderer->RemoveViewProp(it.textActor);
            }
        }
        // 清除临时矩形
        for (int i = 0; i < 4; ++i) {
            if (m_ctx.pendingRectActors[i]) {
                m_ctx.renderer->RemoveViewProp(m_ctx.pendingRectActors[i]);
                m_ctx.pendingRectActors[i] = nullptr;
            }
        }
        // 清除圆形标注
        for (auto& it : m_ctx.circleAnnotations) {
            for (auto& a : it.actors) {
                if (a) m_ctx.renderer->RemoveViewProp(a);
            }
            if (it.textActor) {
                m_ctx.renderer->RemoveViewProp(it.textActor);
            }
        }
        // 清除临时圆形
        if (m_ctx.pendingCircleActor) {
            m_ctx.renderer->RemoveViewProp(m_ctx.pendingCircleActor);
            m_ctx.pendingCircleActor = nullptr;
        }
        // 清除画笔标注
        for (auto& it : m_ctx.penAnnotations) {
            for (auto& a : it.actors) {
                if (a) m_ctx.renderer->RemoveViewProp(a);
            }
            if (it.textActor) {
                m_ctx.renderer->RemoveViewProp(it.textActor);
            }
        }
        // 清除临时画笔
        for (auto& a : m_ctx.pendingPenActors) {
            if (a) m_ctx.renderer->RemoveViewProp(a);
        }
        m_ctx.pendingPenActors.clear();
    }
    m_ctx.measurements.clear();
    m_ctx.annotations.clear();
    m_ctx.circleAnnotations.clear();
    m_ctx.penAnnotations.clear();
    m_ctx.pendingPoints.clear();
    m_ctx.pendingPenPoints.clear();
    m_ctx.pendingMode = ToolMode::None;
    m_ctx.pendingPointActors.clear();
    m_ctx.pendingSegMode = false;
    m_ctx.pendingSliceNumber = 0;
}

void SliceInteractorStyle::UpdateMeasurementVisibility(int sliceNumber, bool segMode)
{
    if (!m_ctx.renderer) {
        // 尝试从当前 interactor 推导 renderer
        vtkRenderWindowInteractor* interactor = this->GetInteractor();
        if (interactor && interactor->GetRenderWindow()) {
            vtkRendererCollection* renderers = interactor->GetRenderWindow()->GetRenderers();
            m_ctx.renderer = renderers ? renderers->GetFirstRenderer() : nullptr;
        }
    }
    if (!m_ctx.renderer) return;

    for (auto& it : m_ctx.measurements) {
        const bool visible = (it.segMode == segMode) && (it.sliceNumber == sliceNumber);
        for (auto& a : it.actors) {
            if (a) a->SetVisibility(visible ? 1 : 0);
        }
        if (it.textActor) it.textActor->SetVisibility(visible ? 1 : 0);
    }

    // 矩形标注也要跟着切片显示/隐藏
    for (auto& it : m_ctx.annotations) {
        const bool visible = (it.segMode == segMode) && (it.sliceNumber == sliceNumber);
        for (auto& a : it.actors) {
            if (a) a->SetVisibility(visible ? 1 : 0);
        }
        if (it.textActor) it.textActor->SetVisibility(visible ? 1 : 0);
    }

    // 圆形标注也要跟着切片显示/隐藏
    for (auto& it : m_ctx.circleAnnotations) {
        const bool visible = (it.segMode == segMode) && (it.sliceNumber == sliceNumber);
        for (auto& a : it.actors) {
            if (a) a->SetVisibility(visible ? 1 : 0);
        }
        if (it.textActor) it.textActor->SetVisibility(visible ? 1 : 0);
    }

    // 画笔标注也要跟着切片显示/隐藏
    for (auto& it : m_ctx.penAnnotations) {
        const bool visible = (it.segMode == segMode) && (it.sliceNumber == sliceNumber);
        for (auto& a : it.actors) {
            if (a) a->SetVisibility(visible ? 1 : 0);
        }
        if (it.textActor) it.textActor->SetVisibility(visible ? 1 : 0);
    }

    // 未完成测量的"点"预览也要跟着切片显示/隐藏
    const bool pendingVisible =
        (m_ctx.pendingMode == ToolMode::MeasureDistance || m_ctx.pendingMode == ToolMode::MeasureAngle) &&
        (m_ctx.pendingSegMode == segMode) &&
        (m_ctx.pendingSliceNumber == sliceNumber);
    for (auto& a : m_ctx.pendingPointActors) {
        if (a) a->SetVisibility(pendingVisible ? 1 : 0);
    }
    if (m_ctx.pendingAngleFirstLine) {
        m_ctx.pendingAngleFirstLine->SetVisibility(pendingVisible ? 1 : 0);
    }
}

void SliceInteractorStyle::SetAnnotationCallback(std::function<void(double screenX, double screenY, int annotationIndex, SliceOrientation orientation, int annotationType)> callback)
{
    // 包装回调，添加orientation和annotationType参数
    m_ctx.onAnnotationCreated = [callback, this](double screenX, double screenY, int annotationIndex, int annotationType) {
        if (callback) {
            callback(screenX, screenY, annotationIndex, m_orientation, annotationType);
        }
        };
}

void SliceInteractorStyle::UpdateAnnotationText(int index, const std::string& text)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.annotations.size())) {
        return;
    }
    auto& item = m_ctx.annotations[index];
    item.labelText = text;

    // 移除旧的文本actor
    if (item.textActor && m_ctx.renderer) {
        m_ctx.renderer->RemoveActor(item.textActor);
    }
    // 如果文本不为空，创建新的文本actor
    if (!text.empty()) {
        // 根据视图方向计算文本位置（显示在矩形右上角）
        const double rgb[3]{ 0.0, 1.0, 1.0 };  // 青色
        double textPos[3];

        // 确定矩形的边界
        double minX = std::min(item.startWorld[0], item.endWorld[0]);
        double maxX = std::max(item.startWorld[0], item.endWorld[0]);
        double minY = std::min(item.startWorld[1], item.endWorld[1]);
        double maxY = std::max(item.startWorld[1], item.endWorld[1]);
        double minZ = std::min(item.startWorld[2], item.endWorld[2]);
        double maxZ = std::max(item.startWorld[2], item.endWorld[2]);

        // 计算中点
        double midX = (minX + maxX) * 0.5;
        double midY = (minY + maxY) * 0.5;

        // 文本向上偏移量（世界坐标单位，通常是mm）
        const double textOffset = 3.0;  // 增加偏移量，使文字更靠上

        switch (m_orientation) {
        case SliceOrientation::Axial: // XY平面
            // 相机ViewUp(0,1,0)：Y向上
            // 矩形正上方 = 上边中间 + 向上偏移
            textPos[0] = midX;
            textPos[1] = maxY + textOffset;  // Y向上，增加Y值
            textPos[2] = item.startWorld[2];
            break;

        case SliceOrientation::Sagittal: // YZ平面
            // 相机ViewUp(0,0,-1)：-Z向上
            // 矩形正上方 = 上边中间 + 向上偏移
            textPos[0] = item.startWorld[0];
            textPos[1] = midY;
            textPos[2] = minZ - textOffset;  // -Z向上，减小Z值
            break;

        case SliceOrientation::Coronal: // XZ平面
            // 相机ViewUp(0,0,-1)：-Z向上
            // 矩形正上方 = 上边中间 + 向上偏移
            textPos[0] = midX;
            textPos[1] = item.startWorld[1];
            textPos[2] = minZ - textOffset;  // -Z向上，减小Z值
            break;

        default:
            textPos[0] = midX;
            textPos[1] = maxY + textOffset;
            textPos[2] = item.endWorld[2];
            break;
        }

        auto textActor = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        textActor->SetPosition(textPos[0], textPos[1], textPos[2]);
        textActor->SetInput(text.c_str());
        auto textProp = textActor->GetTextProperty();
        textProp->SetColor(rgb[0], rgb[1], rgb[2]);
        textProp->SetFontSize(16);
        textProp->SetBold(1);
        // 设置文本水平居中对齐
        textProp->SetJustificationToCentered();
        // 设置文本垂直居中对齐
        textProp->SetVerticalJustificationToCentered();
        textActor->PickableOff();

        item.textActor = textActor;
        if (m_ctx.renderer) {
            m_ctx.renderer->AddActor(textActor);
        }
    }

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

void SliceInteractorStyle::DeleteAnnotation(int index)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.annotations.size())) {
        return;
    }

    auto& item = m_ctx.annotations[index];

    // 从renderer中移除所有actors
    if (m_ctx.renderer) {
        for (auto& actor : item.actors) {
            if (actor) {
                m_ctx.renderer->RemoveViewProp(actor);
            }
        }
        if (item.textActor) {
            m_ctx.renderer->RemoveViewProp(item.textActor);
        }
    }

    // 从向量中删除该标注
    m_ctx.annotations.erase(m_ctx.annotations.begin() + index);

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

void SliceInteractorStyle::UpdateCircleAnnotationText(int index, const std::string& text)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.circleAnnotations.size())) {
        return;
    }

    auto& item = m_ctx.circleAnnotations[index];
    item.labelText = text;

    // 移除旧的文本actor
    if (item.textActor && m_ctx.renderer) {
        m_ctx.renderer->RemoveActor(item.textActor);
    }

    // 如果文本不为空，创建新的文本actor
    if (!text.empty()) {
        const double rgb[3]{ 1.0, 0.5, 0.0 };  // 橙色
        const double textOffset = 3.0;
        double textPos[3];

        switch (m_orientation) {
        case SliceOrientation::Axial:
            textPos[0] = item.centerWorld[0];
            textPos[1] = item.centerWorld[1] + item.radius + textOffset;
            textPos[2] = item.centerWorld[2];
            break;
        case SliceOrientation::Sagittal:
            textPos[0] = item.centerWorld[0];
            textPos[1] = item.centerWorld[1];
            textPos[2] = item.centerWorld[2] - item.radius - textOffset;
            break;
        case SliceOrientation::Coronal:
            textPos[0] = item.centerWorld[0];
            textPos[1] = item.centerWorld[1];
            textPos[2] = item.centerWorld[2] - item.radius - textOffset;
            break;
        default:
            textPos[0] = item.centerWorld[0];
            textPos[1] = item.centerWorld[1] + item.radius + textOffset;
            textPos[2] = item.centerWorld[2];
            break;
        }

        auto textActor = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        textActor->SetPosition(textPos[0], textPos[1], textPos[2]);
        textActor->SetInput(text.c_str());
        auto textProp = textActor->GetTextProperty();
        textProp->SetColor(rgb[0], rgb[1], rgb[2]);
        textProp->SetFontSize(16);
        textProp->SetBold(1);
        textProp->SetJustificationToCentered();
        textProp->SetVerticalJustificationToCentered();
        textActor->PickableOff();

        item.textActor = textActor;
        if (m_ctx.renderer) {
            m_ctx.renderer->AddActor(textActor);
        }
    }

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

void SliceInteractorStyle::DeleteCircleAnnotation(int index)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.circleAnnotations.size())) {
        return;
    }

    auto& item = m_ctx.circleAnnotations[index];

    // 从renderer中移除所有actors
    if (m_ctx.renderer) {
        for (auto& actor : item.actors) {
            if (actor) {
                m_ctx.renderer->RemoveViewProp(actor);
            }
        }
        if (item.textActor) {
            m_ctx.renderer->RemoveViewProp(item.textActor);
        }
    }

    // 从向量中删除该标注
    m_ctx.circleAnnotations.erase(m_ctx.circleAnnotations.begin() + index);

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

void SliceInteractorStyle::UpdatePenAnnotationText(int index, const std::string& text)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.penAnnotations.size())) {
        return;
    }

    auto& item = m_ctx.penAnnotations[index];
    item.labelText = text;

    // 移除旧的文本actor
    if (item.textActor && m_ctx.renderer) {
        m_ctx.renderer->RemoveActor(item.textActor);
    }

    // 如果文本不为空，创建新的文本actor
    if (!text.empty() && !item.pathPoints.empty()) {
        const double rgb[3]{ 0.5, 1.0, 0.5 };  // 绿色
        const double textOffset = 3.0;

        // 计算路径边界
        double minX = item.pathPoints[0][0], maxX = minX;
        double minY = item.pathPoints[0][1], maxY = minY;
        double minZ = item.pathPoints[0][2], maxZ = minZ;

        for (const auto& pt : item.pathPoints) {
            minX = std::min(minX, pt[0]);
            maxX = std::max(maxX, pt[0]);
            minY = std::min(minY, pt[1]);
            maxY = std::max(maxY, pt[1]);
            minZ = std::min(minZ, pt[2]);
            maxZ = std::max(maxZ, pt[2]);
        }

        double midX = (minX + maxX) * 0.5;
        double midY = (minY + maxY) * 0.5;
        double textPos[3];

        switch (m_orientation) {
        case SliceOrientation::Axial:
            textPos[0] = midX;
            textPos[1] = maxY + textOffset;
            textPos[2] = item.pathPoints[0][2];
            break;
        case SliceOrientation::Sagittal:
            textPos[0] = item.pathPoints[0][0];
            textPos[1] = midY;
            textPos[2] = minZ - textOffset;
            break;
        case SliceOrientation::Coronal:
            textPos[0] = midX;
            textPos[1] = item.pathPoints[0][1];
            textPos[2] = minZ - textOffset;
            break;
        default:
            textPos[0] = midX;
            textPos[1] = maxY + textOffset;
            textPos[2] = item.pathPoints[0][2];
            break;
        }

        auto textActor = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        textActor->SetPosition(textPos[0], textPos[1], textPos[2]);
        textActor->SetInput(text.c_str());
        auto textProp = textActor->GetTextProperty();
        textProp->SetColor(rgb[0], rgb[1], rgb[2]);
        textProp->SetFontSize(16);
        textProp->SetBold(1);
        textProp->SetJustificationToCentered();
        textProp->SetVerticalJustificationToCentered();
        textActor->PickableOff();

        item.textActor = textActor;
        if (m_ctx.renderer) {
            m_ctx.renderer->AddActor(textActor);
        }
    }

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

void SliceInteractorStyle::DeletePenAnnotation(int index)
{
    if (index < 0 || index >= static_cast<int>(m_ctx.penAnnotations.size())) {
        return;
    }

    auto& item = m_ctx.penAnnotations[index];

    // 从renderer中移除所有actors
    if (m_ctx.renderer) {
        for (auto& actor : item.actors) {
            if (actor) {
                m_ctx.renderer->RemoveViewProp(actor);
            }
        }
        if (item.textActor) {
            m_ctx.renderer->RemoveViewProp(item.textActor);
        }
    }

    // 从向量中删除该标注
    m_ctx.penAnnotations.erase(m_ctx.penAnnotations.begin() + index);

    // 刷新渲染
    if (m_ctx.requestRender) {
        m_ctx.requestRender();
    }
    else if (this->Interactor) {
        this->Interactor->Render();
    }
}

int SliceInteractorStyle::getCurrentSlice() const
{
    // 根据当前数据模式选择切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->segAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->segSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->segCoronalSlice();
        }
    }
    else {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->axialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->sagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->coronalSlice();
        }
    }
    return 0;
}

int SliceInteractorStyle::getMaxSlice() const
{
    // 根据当前数据模式选择最大切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->maxSegAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->maxSegSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->maxSegCoronalSlice();
        }
    }
    else {
        switch (m_orientation) {
        case SliceOrientation::Axial: return m_dataModel->maxAxialSlice();
        case SliceOrientation::Sagittal: return m_dataModel->maxSagittalSlice();
        case SliceOrientation::Coronal: return m_dataModel->maxCoronalSlice();
        }
    }
    return 0;
}

void SliceInteractorStyle::setSlice(int slice)
{
    // 根据当前数据模式设置切片
    if (m_dataModel->isSegDataMode()) {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            m_dataModel->setSegAxialSlice(slice);
            break;
        case SliceOrientation::Sagittal:
            m_dataModel->setSegSagittalSlice(slice);
            break;
        case SliceOrientation::Coronal:
            m_dataModel->setSegCoronalSlice(slice);
            break;
        }
    }
    else {
        switch (m_orientation) {
        case SliceOrientation::Axial:
            m_dataModel->setAxialSlice(slice);
            break;
        case SliceOrientation::Sagittal:
            m_dataModel->setSagittalSlice(slice);
            break;
        case SliceOrientation::Coronal:
            m_dataModel->setCoronalSlice(slice);
            break;
        }
    }
}

// ===== SliceVtkItemBase implementations =====
// 静态成员变量定义
std::map<SliceOrientation, vtkSmartPointer<SliceInteractorStyle>> SliceVtkItemBase::s_interactorStyles;

SliceVtkItemBase::SliceVtkItemBase(SliceOrientation orientation, const char* viewName)
    : m_orientation(orientation),
    m_viewName(viewName)
{
    m_dataModel = GET_SINGLETON(DicomDataModel);
}

SliceInteractorStyle* SliceVtkItemBase::GetInteractorStyle(SliceOrientation orientation)
{
    auto it = s_interactorStyles.find(orientation);
    if (it != s_interactorStyles.end()) {
        return it->second.GetPointer();
    }
    return nullptr;
}

QQuickVTKItem::vtkUserData SliceVtkItemBase::initializeVTK(vtkRenderWindow* renderWindow)
{
    // 创建用户数据对象
    vtkSmartPointer<SliceViewData> data = vtkSmartPointer<SliceViewData>::New();

    // 连接信号（在GUI线程）
    connect(m_dataModel, &DicomDataModel::dataLoaded,
        this, &SliceVtkItemBase::onDataLoaded);
    connect(m_dataModel, &DicomDataModel::segDataLoaded,
        this, &SliceVtkItemBase::onSegDataLoaded);
    connect(m_dataModel, &DicomDataModel::segRefreshRenderer,
        this, &SliceVtkItemBase::onSegRefreshRenderer);
    connect(m_dataModel, &DicomDataModel::segSliceRefreshRequested,
        this, &SliceVtkItemBase::onSegRefreshRenderer);
    connect(m_dataModel, &DicomDataModel::axialSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    connect(m_dataModel, &DicomDataModel::sagittalSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    connect(m_dataModel, &DicomDataModel::coronalSliceChanged,
        this, &SliceVtkItemBase::onSliceChanged);
    // 连接SegData切片变化信号
    connect(m_dataModel, &DicomDataModel::segAxialSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
    connect(m_dataModel, &DicomDataModel::segSagittalSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
    connect(m_dataModel, &DicomDataModel::segCoronalSliceChanged,
        this, &SliceVtkItemBase::onSegSliceChanged);
    connect(m_dataModel, &DicomDataModel::windowWidthChanged,
        this, &SliceVtkItemBase::onWindowChanged);
    connect(m_dataModel, &DicomDataModel::windowLevelChanged,
        this, &SliceVtkItemBase::onWindowChanged);
    connect(m_dataModel, &DicomDataModel::interactionResetRequested,
        this, &SliceVtkItemBase::onInteractionResetRequested);
    connect(m_dataModel, &DicomDataModel::crosshairEnabledChanged,
        this, &SliceVtkItemBase::onCrosshairEnabledChanged);
    connect(m_dataModel, &DicomDataModel::screenshotRequested,
        this, &SliceVtkItemBase::onScreenshotRequested);

    // 在渲染线程初始化VTK对象
    vtkSmartPointer<vtkImageData> imageData = m_dataModel->getImageData();
    if (imageData) {
        setupView(renderWindow, data, imageData);
    }

    return data;
}

void SliceVtkItemBase::onInteractionResetRequested()
{
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            resetViewState(rw, data);
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onCrosshairEnabledChanged(bool enabled)
{
    if (!enabled) {
        return;  // 关闭十字线时不需要重置相机
    }

    // 开启十字线时，重置相机到0.95填充比例
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        Q_UNUSED(rw);
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            if (data->renderer && data->imageSlice) {
                setupCamera(data->renderer);
                applyParallelScale(data->imageSlice, data->renderer);
                data->renderer->ResetCameraClippingRange();
            }
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onScreenshotRequested(int viewType, QString filePath)
{
    // 检查是否是当前视图
    int currentViewType = static_cast<int>(m_orientation);
    if (viewType != currentViewType) {
        return;  // 不是当前视图，忽略
    }

    // 使用 Qt 的 grabToImage 方法截图，可以捕获所有渲染内容包括标注文字
    auto result = grabToImage();
    if (!result) {
        emit m_dataModel->showMessageRequested("error", QStringLiteral("截图失败"));
        return;
    }

    connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, filePath]() {
        QImage image = result->image();
        if (image.isNull()) {
            emit m_dataModel->showMessageRequested("error", QStringLiteral("截图失败：图像为空"));
            return;
        }

        // 根据文件扩展名选择格式
        QString ext = filePath.right(4).toLower();
        const char* format = "PNG";
        int quality = -1;

        if (ext == ".jpg" || ext == "jpeg") {
            format = "JPEG";
            quality = 95;
        }

        QString savePath = filePath;
        if (ext != ".png" && ext != ".jpg" && ext != "jpeg") {
            savePath = filePath + ".png";
        }

        if (image.save(savePath, format, quality)) {
            emit m_dataModel->showMessageRequested("success", QStringLiteral("截图保存成功"));
        }
        else {
            emit m_dataModel->showMessageRequested("error", QStringLiteral("截图保存失败"));
        }
        });
}

void SliceVtkItemBase::onDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        vtkSmartPointer<vtkImageData> imageData = m_dataModel->getImageData();
        if (imageData && userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            setupView(rw, data, imageData);
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onSegDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            // 清除旧的渲染器
            rw->GetRenderers()->RemoveAllItems();

            switch (m_orientation) {
            case SliceOrientation::Axial:
                data->imageSlice = m_dataModel->getSegImageData(0);
                break;
            case SliceOrientation::Sagittal:
                data->imageSlice = m_dataModel->getSegImageData(2);
                break;
            case SliceOrientation::Coronal:
                data->imageSlice = m_dataModel->getSegImageData(1);
                break;
            }

            // 创建渲染器
            data->renderer = vtkSmartPointer<vtkRenderer>::New();
            data->renderer->AddViewProp(data->imageSlice);
            data->renderer->SetBackground(0, 0, 0);

            // ===== 标尺（Scale Bar）=====
            if (!data->axisActor) {
                data->axisActor = vtkSmartPointer<vtkAxisActor2D>::New();
                data->axisActor->GetPoint1Coordinate()->SetCoordinateSystemToDisplay();
                data->axisActor->GetPoint2Coordinate()->SetCoordinateSystemToDisplay();
                data->axisActor->SetNumberOfLabels(5);
                data->axisActor->AdjustLabelsOff();
                data->axisActor->GetTitleTextProperty()->SetBold(1);
                data->axisActor->GetTitleTextProperty()->SetItalic(0);
                data->axisActor->GetTitleTextProperty()->SetShadow(1);
                data->axisActor->GetTitleTextProperty()->SetFontFamilyToArial();
                data->axisActor->GetTitleTextProperty()->SetFontSize(14);
                data->axisActor->GetProperty()->SetColor(1, 1, 1);
            }
            data->renderer->AddActor2D(data->axisActor);

            // 添加标题文本标签
            vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
            textMapper->SetInput(m_viewName);
            textMapper->GetTextProperty()->SetFontSize(20);
            textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

            vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
            textActor->SetMapper(textMapper);
            textActor->SetPosition(10, 10);
            data->renderer->AddActor2D(textActor);

            rw->AddRenderer(data->renderer);

            // 设置交互样式
            vtkSmartPointer<SliceInteractorStyle> style = vtkSmartPointer<SliceInteractorStyle>::New();
            style->SetOrientation(m_orientation);
            style->setAxisActor(data->axisActor);
            data->interactorStyle = style;
            rw->GetInteractor()->SetInteractorStyle(style);

            // 设置相机方向
            setupCamera(data->renderer);
            applyParallelScale(data->imageSlice, data->renderer);
            style->rescaleAxisActor();
            // 保存到静态映射中
            s_interactorStyles[m_orientation] = style;
            // 设置标注回调
            auto mainVC = GET_SINGLETON(MainViewController);
            style->SetAnnotationCallback([mainVC](double screenX, double screenY, int annotationIndex, SliceOrientation orientation, int annotationType) {
                if (mainVC) {
                    emit mainVC->annotationCreated(screenX, screenY, annotationIndex, static_cast<int>(orientation), annotationType);
                }
                });
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onSegRefreshRenderer()
{
    // 分割视图显示模式改变时，需要刷新渲染
    scheduleRender();
}

void SliceVtkItemBase::onSliceChanged(int slice)
{
    Q_UNUSED(slice);
    // 使用 dispatch_async 在渲染线程更新切片
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        Q_UNUSED(rw);
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            if (data->imageMapper) {
                data->imageMapper->SetSliceNumber(getCurrentSlice());
                data->imageMapper->Modified();
            }
            // 更新测量显示/隐藏（普通模式）
            if (data->interactorStyle) {
                data->interactorStyle->UpdateMeasurementVisibility(getCurrentSlice(), false);
            }
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onSegSliceChanged(int slice)
{
    Q_UNUSED(slice);
    // 使用 dispatch_async 在渲染线程更新SegData切片
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        Q_UNUSED(rw);
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            if (data->imageSlice && data->imageSlice->GetMapper()) {
                vtkImageSliceMapper* mapper = vtkImageSliceMapper::SafeDownCast(data->imageSlice->GetMapper());
                if (mapper) {
                    mapper->SetSliceNumber(getSegCurrentSlice());
                    mapper->Modified();
                }
            }
            // 更新测量显示/隐藏（分割模式）
            if (data->interactorStyle) {
                data->interactorStyle->UpdateMeasurementVisibility(getSegCurrentSlice(), true);
            }
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::onWindowChanged()
{
    // 使用 dispatch_async 在渲染线程更新窗宽窗位
    dispatch_async([this](vtkRenderWindow* rw, vtkUserData userData) {
        Q_UNUSED(rw);
        if (userData) {
            SliceViewData* data = static_cast<SliceViewData*>(userData.GetPointer());
            if (data->imageSlice) {
                data->imageSlice->GetProperty()->SetColorWindow(
                    m_dataModel->windowWidth());
                data->imageSlice->GetProperty()->SetColorLevel(
                    m_dataModel->windowLevel());
                data->imageSlice->GetProperty()->Modified();
                updateWWWLText(data);
            }
        }
        });
    scheduleRender();
}

void SliceVtkItemBase::setupView(vtkRenderWindow* renderWindow, SliceViewData* data, vtkImageData* imageData)
{
    // 清除旧的渲染器
    renderWindow->GetRenderers()->RemoveAllItems();

    // 创建图像切片
    data->imageMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
    data->imageMapper->SetInputData(imageData);
    setMapperOrientation(data->imageMapper);
    data->imageMapper->SetSliceNumber(getCurrentSlice());

    data->imageSlice = vtkSmartPointer<vtkImageSlice>::New();
    data->imageSlice->SetMapper(data->imageMapper);
    data->imageSlice->GetProperty()->SetColorWindow(m_dataModel->windowWidth());
    data->imageSlice->GetProperty()->SetColorLevel(m_dataModel->windowLevel());

    data->axisActor = vtkSmartPointer<vtkAxisActor2D>::New();
    data->axisActor->GetPoint1Coordinate()->SetCoordinateSystemToDisplay();
    data->axisActor->GetPoint2Coordinate()->SetCoordinateSystemToDisplay();
    data->axisActor->SetNumberOfLabels(5);
    data->axisActor->AdjustLabelsOff();
    data->axisActor->GetTitleTextProperty()->SetBold(1);
    data->axisActor->GetTitleTextProperty()->SetItalic(0);
    data->axisActor->GetTitleTextProperty()->SetShadow(1);
    data->axisActor->GetTitleTextProperty()->SetFontFamilyToArial();
    data->axisActor->GetTitleTextProperty()->SetFontSize(14);
    data->axisActor->GetProperty()->SetColor(1, 1, 1);

    // 创建渲染器
    data->renderer = vtkSmartPointer<vtkRenderer>::New();
    data->renderer->AddViewProp(data->imageSlice);
    data->renderer->AddActor2D(data->axisActor);
    data->renderer->SetBackground(0, 0, 0);

    // 添加标题文本标签
    vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
    textMapper->SetInput(m_viewName);
    textMapper->GetTextProperty()->SetFontSize(20);
    textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

    vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
    textActor->SetMapper(textMapper);
    textActor->SetPosition(10, 10);
    data->renderer->AddActor2D(textActor);

    // 添加窗宽窗位显示文本（右下角）
    data->wwwlTextMapper = vtkSmartPointer<vtkTextMapper>::New();
    updateWWWLText(data);
    data->wwwlTextMapper->GetTextProperty()->SetFontSize(16);
    data->wwwlTextMapper->GetTextProperty()->SetColor(0.0, 1.0, 0.0);
    data->wwwlTextMapper->GetTextProperty()->SetJustificationToRight();

    data->wwwlTextActor = vtkSmartPointer<vtkActor2D>::New();
    data->wwwlTextActor->SetMapper(data->wwwlTextMapper);
    data->wwwlTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    data->wwwlTextActor->SetPosition(0.98, 0.02);
    data->renderer->AddActor2D(data->wwwlTextActor);

    renderWindow->AddRenderer(data->renderer);

    // 设置交互样式
    vtkSmartPointer<SliceInteractorStyle> style = vtkSmartPointer<SliceInteractorStyle>::New();
    style->SetOrientation(m_orientation);
    style->setAxisActor(data->axisActor);
    data->interactorStyle = style;
    renderWindow->GetInteractor()->SetInteractorStyle(style);

    // 保存到静态映射中
    s_interactorStyles[m_orientation] = style;

    // 设置标注回调
    auto mainVC = GET_SINGLETON(MainViewController);
    style->SetAnnotationCallback([mainVC](double screenX, double screenY, int annotationIndex, SliceOrientation orientation, int annotationType) {
        if (mainVC) {
            emit mainVC->annotationCreated(screenX, screenY, annotationIndex, static_cast<int>(orientation), annotationType);
        }
        });

    // 设置相机方向并锁定并行缩放（保持视口尺寸稳定）
    setupCamera(data->renderer);
    applyParallelScale(data->imageSlice, data->renderer);
    style->rescaleAxisActor();
}

void SliceVtkItemBase::resetViewState(vtkRenderWindow* rw, SliceViewData* data)
{
    if (!rw || !data) return;

    // 1) 清除测量/取消 state
    if (data->interactorStyle) {
        data->interactorStyle->ResetInteractionState();
    }

    // 2) 重置相机（平移/缩放回初始）
    if (data->renderer) {
        setupCamera(data->renderer);
        if (data->imageSlice) {
            applyParallelScale(data->imageSlice, data->renderer);
        }
        data->renderer->ResetCameraClippingRange();
    }

    // 3) 标尺重算
    if (data->interactorStyle) {
        data->interactorStyle->rescaleAxisActor();
        // reset 后隐藏所有测量（因为已清空）
        const bool segMode = m_dataModel->isSegDataMode();
        const int slice = segMode ? getSegCurrentSlice() : getCurrentSlice();
        data->interactorStyle->UpdateMeasurementVisibility(slice, segMode);
    }
}

void SliceVtkItemBase::setMapperOrientation(vtkImageSliceMapper* mapper)
{
    switch (m_orientation) {
    case SliceOrientation::Axial:
        mapper->SetOrientationToZ();
        break;
    case SliceOrientation::Sagittal:
        mapper->SetOrientationToX();
        break;
    case SliceOrientation::Coronal:
        mapper->SetOrientationToY();
        break;
    }
}

void SliceVtkItemBase::setupCamera(vtkRenderer* renderer)
{
    vtkCamera* camera = renderer->GetActiveCamera();

    switch (m_orientation) {
    case SliceOrientation::Axial:
        camera->SetViewUp(0, 1, 0);
        camera->SetPosition(0, 0, 1);
        camera->SetFocalPoint(0, 0, 0);
        break;
    case SliceOrientation::Sagittal:
        camera->SetViewUp(0, 0, -1);
        camera->SetPosition(1, 0, 0);
        camera->SetFocalPoint(0, 0, 0);
        break;
    case SliceOrientation::Coronal:
        camera->SetViewUp(0, 0, -1);
        camera->SetPosition(0, -1, 0);
        camera->SetFocalPoint(0, 0, 0);
        break;
    }

    // 只根据图像边界重置相机，忽略测量工具等其他actor
    // 在分割模式下使用分割数据的边界
    vtkImageData* imageData = nullptr;
    if (m_dataModel->isSegDataMode()) {
        // 从分割视图获取图像数据
        vtkSmartPointer<vtkImageSlice> segSlice;
        switch (m_orientation) {
        case SliceOrientation::Axial:
            segSlice = m_dataModel->getSegImageData(0);
            break;
        case SliceOrientation::Sagittal:
            segSlice = m_dataModel->getSegImageData(2);
            break;
        case SliceOrientation::Coronal:
            segSlice = m_dataModel->getSegImageData(1);
            break;
        }
        if (segSlice && segSlice->GetMapper()) {
            auto* mapper = vtkImageSliceMapper::SafeDownCast(segSlice->GetMapper());
            if (mapper) {
                imageData = vtkImageData::SafeDownCast(mapper->GetInput());
            }
        }
    }
    else {
        imageData = m_dataModel->getImageData();
    }

    if (imageData) {
        double bounds[6];
        imageData->GetBounds(bounds);
        renderer->ResetCamera(bounds);
    }
    else {
        renderer->ResetCamera();
    }
}

void SliceVtkItemBase::applyParallelScale(vtkImageSlice* imageSlice, vtkRenderer* renderer)
{
    if (!imageSlice || !renderer) {
        return;
    }
    vtkCamera* cam = renderer->GetActiveCamera();
    if (!cam) {
        return;
    }
    cam->SetParallelProjection(true);

    auto* mapper = vtkImageSliceMapper::SafeDownCast(imageSlice->GetMapper());
    if (!mapper) {
        return;
    }
    auto* img = vtkImageData::SafeDownCast(mapper->GetInput());
    if (!img) {
        return;
    }

    int extent[6];
    double spacing[3];
    img->GetExtent(extent);
    img->GetSpacing(spacing);

    double w = (extent[1] - extent[0] + 1) * spacing[0];
    double h = (extent[3] - extent[2] + 1) * spacing[1];
    if (m_orientation == SliceOrientation::Sagittal) {
        w = (extent[3] - extent[2] + 1) * spacing[1];
        h = (extent[5] - extent[4] + 1) * spacing[2];
    }
    else if (m_orientation == SliceOrientation::Coronal) {
        w = (extent[1] - extent[0] + 1) * spacing[0];
        h = (extent[5] - extent[4] + 1) * spacing[2];
    }

    // 让切片占用视口 80%（可按需调整）
    const double targetFill = 0.95;
    cam->SetParallelScale(0.5 * std::max(w, h) / targetFill);
}

int SliceVtkItemBase::getCurrentSlice() const
{
    switch (m_orientation) {
    case SliceOrientation::Axial:
        return m_dataModel->axialSlice();
    case SliceOrientation::Sagittal:
        return m_dataModel->sagittalSlice();
    case SliceOrientation::Coronal:
        return m_dataModel->coronalSlice();
    }
    return 0;
}

int SliceVtkItemBase::getSegCurrentSlice() const
{
    switch (m_orientation) {
    case SliceOrientation::Axial:
        return m_dataModel->segAxialSlice();
    case SliceOrientation::Sagittal:
        return m_dataModel->segSagittalSlice();
    case SliceOrientation::Coronal:
        return m_dataModel->segCoronalSlice();
    }
    return 0;
}


void SliceVtkItemBase::updateWWWLText(SliceViewData* data)
{
    if (data->wwwlTextMapper) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "W: %.0f  L: %.0f",
            m_dataModel->windowWidth(),
            m_dataModel->windowLevel());
        data->wwwlTextMapper->SetInput(buffer);
    }
}

// ===== Derived Slice Items =====
AxialVtkItem::AxialVtkItem()
    : SliceVtkItemBase(SliceOrientation::Axial, "Axial View")
{
}

SagittalVtkItem::SagittalVtkItem()
    : SliceVtkItemBase(SliceOrientation::Sagittal, "Sagittal View")
{
}

CoronalVtkItem::CoronalVtkItem()
    : SliceVtkItemBase(SliceOrientation::Coronal, "Coronal View")
{
}

// ===== VolumeVtkItem implementations =====
QQuickVTKItem::vtkUserData VolumeVtkItem::initializeVTK(vtkRenderWindow* renderWindow)
{
    // 创建用户数据对象
    vtkSmartPointer<VolumeViewData> data = vtkSmartPointer<VolumeViewData>::New();

    // 连接信号
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::dataLoaded,
        this, &VolumeVtkItem::onDataLoaded);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::segDataLoaded,
        this, &VolumeVtkItem::onSegDataLoaded);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::segRefreshRenderer,
        this, &VolumeVtkItem::onSegRefreshRenderer);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::interactionResetRequested,
        this, &VolumeVtkItem::onInteractionResetRequested);
    connect(GET_SINGLETON(DicomDataModel), &DicomDataModel::screenshotRequested,
        this, &VolumeVtkItem::onScreenshotRequested);
    // 在渲染线程初始化VTK对象
    vtkSmartPointer<vtkImageData> imageData = GET_SINGLETON(DicomDataModel)->getImageData();
    if (imageData) {
        setupView(renderWindow, data, imageData);
    }

    return data;
}

void VolumeVtkItem::onInteractionResetRequested()
{
    // 仅重置相机（3D 没有测量 widget）
    dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
        if (!rw || !userData) return;
        VolumeViewData* data = static_cast<VolumeViewData*>(userData.GetPointer());
        if (!data || !data->renderer) return;
        vtkCamera* cam = data->renderer->GetActiveCamera();
        if (cam) {
            cam->SetViewUp(0, 0, -1);
            cam->SetPosition(0, 1, 0);
            cam->SetFocalPoint(0, 0, 0);
        }
        data->renderer->ResetCamera();
        });
    scheduleRender();
}

void VolumeVtkItem::onScreenshotRequested(int viewType, QString filePath)
{
    // 检查是否是3D视图
    if (viewType != 3) {
        return;  // 不是3D视图，忽略
    }

    auto dicomModel = GET_SINGLETON(DicomDataModel);

    // 使用 Qt 的 grabToImage 方法截图
    auto result = grabToImage();
    if (!result) {
        emit dicomModel->showMessageRequested("error", QStringLiteral("截图失败"));
        return;
    }

    connect(result.data(), &QQuickItemGrabResult::ready, this, [dicomModel, result, filePath]() {
        QImage image = result->image();
        if (image.isNull()) {
            emit dicomModel->showMessageRequested("error", QStringLiteral("截图失败：图像为空"));
            return;
        }

        // 根据文件扩展名选择格式
        QString ext = filePath.right(4).toLower();
        const char* format = "PNG";
        int quality = -1;

        if (ext == ".jpg" || ext == "jpeg") {
            format = "JPEG";
            quality = 95;
        }

        QString savePath = filePath;
        if (ext != ".png" && ext != ".jpg" && ext != "jpeg") {
            savePath = filePath + ".png";
        }

        if (image.save(savePath, format, quality)) {
            emit dicomModel->showMessageRequested("success", QStringLiteral("截图保存成功"));
        }
        else {
            emit dicomModel->showMessageRequested("error", QStringLiteral("截图保存失败"));
        }
        });
}

void VolumeVtkItem::onDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
        vtkSmartPointer<vtkImageData> imageData = GET_SINGLETON(DicomDataModel)->getImageData();
        if (imageData && userData) {
            VolumeViewData* data = static_cast<VolumeViewData*>(userData.GetPointer());
            setupView(rw, data, imageData);
        }
        });
    scheduleRender();
}

void VolumeVtkItem::onSegRefreshRenderer()
{
    scheduleRender();
}

void VolumeVtkItem::onSegDataLoaded()
{
    // 使用 dispatch_async 在渲染线程更新VTK对象
    dispatch_async([](vtkRenderWindow* rw, vtkUserData userData) {
        VolumeViewData* data = static_cast<VolumeViewData*>(userData.GetPointer());
        data->renderer = GET_SINGLETON(DicomDataModel)->getSeg3DRenderer();
        data->renderer->SetBackground(0, 0, 0);
        rw->AddRenderer(data->renderer);


        // 设置3D交互样式
        vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
            vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        rw->GetInteractor()->SetInteractorStyle(style);
        data->renderer->GetActiveCamera()->SetViewUp(0, 0, -1);
        data->renderer->GetActiveCamera()->SetPosition(0, 1, 0);
        data->renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
        data->renderer->ResetCamera();
        });
    scheduleRender();
}

void VolumeVtkItem::setupView(vtkRenderWindow* renderWindow, VolumeViewData* data, vtkImageData* imageData)
{
    // 清除旧的渲染器
    renderWindow->GetRenderers()->RemoveAllItems();

    // 创建体渲染映射器
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper =
        vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    volumeMapper->SetInputData(imageData);

    // 颜色传输函数
    vtkSmartPointer<vtkColorTransferFunction> colorFunc =
        vtkSmartPointer<vtkColorTransferFunction>::New();
    colorFunc->AddRGBPoint(-3024, 0.0, 0.0, 0.0);
    colorFunc->AddRGBPoint(-77, 0.54902, 0.25098, 0.14902);
    colorFunc->AddRGBPoint(94, 0.882353, 0.603922, 0.290196);
    colorFunc->AddRGBPoint(179, 1.0, 0.937033, 0.954531);
    colorFunc->AddRGBPoint(260, 0.615686, 0.0, 0.0);
    colorFunc->AddRGBPoint(3071, 0.827451, 0.658824, 1.0);

    // 不透明度传输函数
    vtkSmartPointer<vtkPiecewiseFunction> opacityFunc =
        vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityFunc->AddPoint(-3024, 0.0);
    opacityFunc->AddPoint(-77, 0.0);
    opacityFunc->AddPoint(94, 0.29);
    opacityFunc->AddPoint(179, 0.55);
    opacityFunc->AddPoint(260, 0.84);
    opacityFunc->AddPoint(3071, 0.875);

    // 梯度不透明度函数
    vtkSmartPointer<vtkPiecewiseFunction> gradientFunc =
        vtkSmartPointer<vtkPiecewiseFunction>::New();
    gradientFunc->AddPoint(0, 0.0);
    gradientFunc->AddPoint(90, 0.5);
    gradientFunc->AddPoint(100, 1.0);

    // 体属性
    vtkSmartPointer<vtkVolumeProperty> volumeProperty =
        vtkSmartPointer<vtkVolumeProperty>::New();
    volumeProperty->SetColor(colorFunc);
    volumeProperty->SetScalarOpacity(opacityFunc);
    volumeProperty->SetGradientOpacity(gradientFunc);
    volumeProperty->SetInterpolationTypeToLinear();
    volumeProperty->ShadeOn();
    volumeProperty->SetAmbient(0.4);
    volumeProperty->SetDiffuse(0.6);
    volumeProperty->SetSpecular(0.2);

    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volumeMapper);
    volume->SetProperty(volumeProperty);

    data->renderer = vtkSmartPointer<vtkRenderer>::New();
    data->renderer->AddVolume(volume);
    data->renderer->SetBackground(0, 0, 0);

    // 添加文本标签
    vtkSmartPointer<vtkTextMapper> textMapper = vtkSmartPointer<vtkTextMapper>::New();
    textMapper->SetInput("3D Volume Rendering");
    textMapper->GetTextProperty()->SetFontSize(20);
    textMapper->GetTextProperty()->SetColor(1.0, 1.0, 0.0);

    vtkSmartPointer<vtkActor2D> textActor = vtkSmartPointer<vtkActor2D>::New();
    textActor->SetMapper(textMapper);
    textActor->SetPosition(10, 10);
    data->renderer->AddActor2D(textActor);

    renderWindow->AddRenderer(data->renderer);

    // 设置3D交互样式
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
        vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    renderWindow->GetInteractor()->SetInteractorStyle(style);
    data->renderer->GetActiveCamera()->SetViewUp(0, 0, -1);
    data->renderer->GetActiveCamera()->SetPosition(0, 1, 0);
    data->renderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
    data->renderer->ResetCamera();
}