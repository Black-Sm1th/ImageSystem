import QtQuick 2.15
import QtQuick.Controls 2.15
import com.vtk.dicom 1.0

// 可复用的四视图显示面板
Item {
    id: root
    
    // 十字线功能总开关（Key_8 可切换）
    property bool crosshairEnabled: $DicomDataModel.crosshairEnabled

    // 十字线样式
    readonly property int crossThicknessThin: 1
    readonly property real crossHandleDiameter: 9
    // 圆球距离中心的固定偏移（像素）
    readonly property real crossHandleOffsetFixed: 160
    // 鼠标靠近线段多少像素时显示圆球
    readonly property real crossHoverThreshold: 8

    // 视图配色：两条线分别使用两种颜色（绿黄 / 绿红 / 红黄）
    readonly property color crossGreen: "#00ff7f"
    readonly property color crossYellow: "#ffd400"
    readonly property color crossRed: "#ff3b30"
    // 图像体素维度（由 C++ 暴露）
    readonly property int dimX: $DicomDataModel.dimX
    readonly property int dimY: $DicomDataModel.dimY
    readonly property int dimZ: $DicomDataModel.dimZ

    function clamp(v, minv, maxv) {
        return Math.max(minv, Math.min(maxv, v));
    }

    // 根据体素坐标，更新三视图十字线（以当前视图尺寸映射）
    function updateCrosshairFromVoxel(i, j, k) {
        if (dimX > 1 && dimY > 1) {
            axialView.crossX = (i / (dimX - 1)) * axialView.width;
            axialView.crossY = (j / (dimY - 1)) * axialView.height;
            canvasAxial.requestPaint();
        }
        if (dimY > 1 && dimZ > 1) {
            sagittalView.crossX = (j / (dimY - 1)) * sagittalView.width;
            sagittalView.crossY = (k / (dimZ - 1)) * sagittalView.height;
            canvasSag.requestPaint();
        }
        if (dimX > 1 && dimZ > 1) {
            coronalView.crossX = (i / (dimX - 1)) * coronalView.width;
            coronalView.crossY = (k / (dimZ - 1)) * coronalView.height;
            canvasCor.requestPaint();
        }
    }

    // 左上：轴向视图
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 5
        anchors.leftMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#000000"
        border.color: "#404040"
        border.width: 1

        AxialView {
            id: axialView
            anchors.fill: parent
            anchors.margins: 2
            property real crossX: width/2
            property real crossY: height/2
            property real crossAngle: 0
        }
        // 叠加层，放在 VTK 之上捕获鼠标并绘制十字线
        Item {
            anchors.fill: parent
            anchors.margins: 2
            z: 10
            MouseArea {
                anchors.fill: parent
                enabled: root.crosshairEnabled
                hoverEnabled: true
                onPressed: {
                    var i = clamp(Math.round(mouse.x / width * (root.dimX - 1)), 0, root.dimX - 1)
                    var j = clamp(Math.round(mouse.y / height * (root.dimY - 1)), 0, root.dimY - 1)
                    $DicomDataModel.sagittalSlice = i
                    $DicomDataModel.coronalSlice = j
                    updateCrosshairFromVoxel(i, j, $DicomDataModel.axialSlice)
                }
                onPositionChanged: {
                    if (pressed) {
                        var i = clamp(Math.round(mouse.x / width * (root.dimX - 1)), 0, root.dimX - 1)
                        var j = clamp(Math.round(mouse.y / height * (root.dimY - 1)), 0, root.dimY - 1)
                        $DicomDataModel.sagittalSlice = i
                        $DicomDataModel.coronalSlice = j
                        updateCrosshairFromVoxel(i, j, $DicomDataModel.axialSlice)
                    }
                }
            }
            // 十字线（支持旋转 + 端点拖拽）
            Item {
                id: axialCrossOverlay
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                clip: true

                readonly property real handleRadius: root.crossHandleDiameter * 0.5
                // 线段足够长，被 clip 裁切到边界
                function maxCornerDistance(cx, cy, w, h) {
                    var d1 = Math.sqrt(cx*cx + cy*cy)
                    var d2 = Math.sqrt((w-cx)*(w-cx) + cy*cy)
                    var d3 = Math.sqrt(cx*cx + (h-cy)*(h-cy))
                    var d4 = Math.sqrt((w-cx)*(w-cx) + (h-cy)*(h-cy))
                    return Math.max(d1, d2, d3, d4)
                }
                readonly property real armLength: maxCornerDistance(axialView.crossX, axialView.crossY, width, height) + 10
                readonly property real handleOffset: root.crossHandleOffsetFixed
                readonly property color vColor: root.crossGreen
                readonly property color hColor: root.crossYellow
                property real rotateOffsetDeg: 0

                // 圆球显示控制
                property bool showHandles: false
                property bool isDragging: false
                readonly property bool handlesVisible: showHandles || isDragging

                // 角度弧度
                readonly property real angleRad: axialView.crossAngle * Math.PI / 180
                // 第一条线（原垂直线）方向：初始向上 = -90度，旋转后 = crossAngle - 90
                readonly property real vAngle: angleRad - Math.PI / 2
                // 第二条线（原水平线）方向：初始向右 = 0度，旋转后 = crossAngle
                readonly property real hAngle: angleRad

                function pointerAngleDeg(mx, my) {
                    var dx = mx - axialView.crossX
                    var dy = my - axialView.crossY
                    return Math.atan2(dy, dx) * 180 / Math.PI
                }

                // 计算点到直线的距离
                function distanceToLine(px, py, cx, cy, angle) {
                    return Math.abs((px - cx) * Math.sin(angle) - (py - cy) * Math.cos(angle))
                }

                // 检测鼠标是否靠近任一线段
                function isNearCrossLine(mx, my) {
                    var d1 = distanceToLine(mx, my, axialView.crossX, axialView.crossY, vAngle)
                    var d2 = distanceToLine(mx, my, axialView.crossX, axialView.crossY, hAngle)
                    return Math.min(d1, d2) < root.crossHoverThreshold
                }

                // 悬停检测 MouseArea
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onPositionChanged: {
                        axialCrossOverlay.showHandles = axialCrossOverlay.isNearCrossLine(mouse.x, mouse.y)
                    }
                    onExited: axialCrossOverlay.showHandles = false
                }

                Canvas {
                    id: canvasAxial
                    anchors.fill: parent

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var cx = axialView.crossX
                        var cy = axialView.crossY
                        var arm = axialCrossOverlay.armLength
                        var vAng = axialCrossOverlay.vAngle
                        var hAng = axialCrossOverlay.hAngle

                        // 第一条线（vColor）
                        ctx.strokeStyle = axialCrossOverlay.vColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(vAng), cy + arm * Math.sin(vAng))
                        ctx.lineTo(cx - arm * Math.cos(vAng), cy - arm * Math.sin(vAng))
                        ctx.stroke()

                        // 第二条线（hColor）
                        ctx.strokeStyle = axialCrossOverlay.hColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(hAng), cy + arm * Math.sin(hAng))
                        ctx.lineTo(cx - arm * Math.cos(hAng), cy - arm * Math.sin(hAng))
                        ctx.stroke()
                    }

                    Connections {
                        target: axialView
                        function onCrossXChanged() { canvasAxial.requestPaint() }
                        function onCrossYChanged() { canvasAxial.requestPaint() }
                        function onCrossAngleChanged() { canvasAxial.requestPaint() }
                    }
                    Connections {
                        target: axialCrossOverlay
                        function onWidthChanged() { canvasAxial.requestPaint() }
                        function onHeightChanged() { canvasAxial.requestPaint() }
                    }
                }

                // 4 个圆球控制点（位置用三角函数算）
                Rectangle {
                    id: vTop_a
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: axialCrossOverlay.vColor
                    visible: axialCrossOverlay.handlesVisible
                    x: axialView.crossX + axialCrossOverlay.handleOffset * Math.cos(axialCrossOverlay.vAngle) - width/2
                    y: axialView.crossY + axialCrossOverlay.handleOffset * Math.sin(axialCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { axialCrossOverlay.isDragging = true; startAngle = axialView.crossAngle - axialCrossOverlay.pointerAngleDeg(mouse.x + vTop_a.x, mouse.y + vTop_a.y) }
                        onPositionChanged: if (pressed) axialView.crossAngle = axialCrossOverlay.pointerAngleDeg(mouse.x + vTop_a.x, mouse.y + vTop_a.y) + startAngle
                        onReleased: axialCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: vBottom_a
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: axialCrossOverlay.vColor
                    visible: axialCrossOverlay.handlesVisible
                    x: axialView.crossX - axialCrossOverlay.handleOffset * Math.cos(axialCrossOverlay.vAngle) - width/2
                    y: axialView.crossY - axialCrossOverlay.handleOffset * Math.sin(axialCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { axialCrossOverlay.isDragging = true; startAngle = axialView.crossAngle - axialCrossOverlay.pointerAngleDeg(mouse.x + vBottom_a.x, mouse.y + vBottom_a.y) }
                        onPositionChanged: if (pressed) axialView.crossAngle = axialCrossOverlay.pointerAngleDeg(mouse.x + vBottom_a.x, mouse.y + vBottom_a.y) + startAngle
                        onReleased: axialCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hRight_a
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: axialCrossOverlay.hColor
                    visible: axialCrossOverlay.handlesVisible
                    x: axialView.crossX + axialCrossOverlay.handleOffset * Math.cos(axialCrossOverlay.hAngle) - width/2
                    y: axialView.crossY + axialCrossOverlay.handleOffset * Math.sin(axialCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { axialCrossOverlay.isDragging = true; startAngle = axialView.crossAngle - axialCrossOverlay.pointerAngleDeg(mouse.x + hRight_a.x, mouse.y + hRight_a.y) }
                        onPositionChanged: if (pressed) axialView.crossAngle = axialCrossOverlay.pointerAngleDeg(mouse.x + hRight_a.x, mouse.y + hRight_a.y) + startAngle
                        onReleased: axialCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hLeft_a
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: axialCrossOverlay.hColor
                    visible: axialCrossOverlay.handlesVisible
                    x: axialView.crossX - axialCrossOverlay.handleOffset * Math.cos(axialCrossOverlay.hAngle) - width/2
                    y: axialView.crossY - axialCrossOverlay.handleOffset * Math.sin(axialCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { axialCrossOverlay.isDragging = true; startAngle = axialView.crossAngle - axialCrossOverlay.pointerAngleDeg(mouse.x + hLeft_a.x, mouse.y + hLeft_a.y) }
                        onPositionChanged: if (pressed) axialView.crossAngle = axialCrossOverlay.pointerAngleDeg(mouse.x + hLeft_a.x, mouse.y + hLeft_a.y) + startAngle
                        onReleased: axialCrossOverlay.isDragging = false
                    }
                }
            }
        }
    }

    // 右上：矢状视图
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.rightMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        SagittalView {
            id: sagittalView
            anchors.fill: parent
            anchors.margins: 2
            property real crossX: width/2
            property real crossY: height/2
            property real crossAngle: 0
        }
        Item {
            anchors.fill: parent
            anchors.margins: 2
            z: 10
            MouseArea {
                anchors.fill: parent
                enabled: root.crosshairEnabled
                hoverEnabled: true
                onPressed: {
                    var j = clamp(Math.round(mouse.x / width * (root.dimY - 1)), 0, root.dimY - 1)
                    var k = clamp(Math.round(mouse.y / height * (root.dimZ - 1)), 0, root.dimZ - 1)
                    $DicomDataModel.coronalSlice = j
                    $DicomDataModel.axialSlice = k
                    updateCrosshairFromVoxel($DicomDataModel.sagittalSlice, j, k)
                }
                onPositionChanged: {
                    if (pressed) {
                        var j = clamp(Math.round(mouse.x / width * (root.dimY - 1)), 0, root.dimY - 1)
                        var k = clamp(Math.round(mouse.y / height * (root.dimZ - 1)), 0, root.dimZ - 1)
                        $DicomDataModel.coronalSlice = j
                        $DicomDataModel.axialSlice = k
                        updateCrosshairFromVoxel($DicomDataModel.sagittalSlice, j, k)
                    }
                }
            }
            Item {
                id: sagCrossOverlay
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                clip: true

                readonly property real handleRadius: root.crossHandleDiameter * 0.5
                function maxCornerDistance(cx, cy, w, h) {
                    var d1 = Math.sqrt(cx*cx + cy*cy)
                    var d2 = Math.sqrt((w-cx)*(w-cx) + cy*cy)
                    var d3 = Math.sqrt(cx*cx + (h-cy)*(h-cy))
                    var d4 = Math.sqrt((w-cx)*(w-cx) + (h-cy)*(h-cy))
                    return Math.max(d1, d2, d3, d4)
                }
                readonly property real armLength: maxCornerDistance(sagittalView.crossX, sagittalView.crossY, width, height) + 10
                readonly property real handleOffset: root.crossHandleOffsetFixed
                readonly property color vColor: root.crossGreen
                readonly property color hColor: root.crossRed
                property real rotateOffsetDeg: 0

                // 圆球显示控制
                property bool showHandles: false
                property bool isDragging: false
                readonly property bool handlesVisible: showHandles || isDragging

                readonly property real angleRad: sagittalView.crossAngle * Math.PI / 180
                readonly property real vAngle: angleRad - Math.PI / 2
                readonly property real hAngle: angleRad

                function pointerAngleDeg(mx, my) {
                    var dx = mx - sagittalView.crossX
                    var dy = my - sagittalView.crossY
                    return Math.atan2(dy, dx) * 180 / Math.PI
                }

                function distanceToLine(px, py, cx, cy, angle) {
                    return Math.abs((px - cx) * Math.sin(angle) - (py - cy) * Math.cos(angle))
                }

                function isNearCrossLine(mx, my) {
                    var d1 = distanceToLine(mx, my, sagittalView.crossX, sagittalView.crossY, vAngle)
                    var d2 = distanceToLine(mx, my, sagittalView.crossX, sagittalView.crossY, hAngle)
                    return Math.min(d1, d2) < root.crossHoverThreshold
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onPositionChanged: {
                        sagCrossOverlay.showHandles = sagCrossOverlay.isNearCrossLine(mouse.x, mouse.y)
                    }
                    onExited: sagCrossOverlay.showHandles = false
                }

                Canvas {
                    id: canvasSag
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var cx = sagittalView.crossX
                        var cy = sagittalView.crossY
                        var arm = sagCrossOverlay.armLength
                        var vAng = sagCrossOverlay.vAngle
                        var hAng = sagCrossOverlay.hAngle

                        ctx.strokeStyle = sagCrossOverlay.vColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(vAng), cy + arm * Math.sin(vAng))
                        ctx.lineTo(cx - arm * Math.cos(vAng), cy - arm * Math.sin(vAng))
                        ctx.stroke()

                        ctx.strokeStyle = sagCrossOverlay.hColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(hAng), cy + arm * Math.sin(hAng))
                        ctx.lineTo(cx - arm * Math.cos(hAng), cy - arm * Math.sin(hAng))
                        ctx.stroke()
                    }
                    Connections {
                        target: sagittalView
                        function onCrossXChanged() { canvasSag.requestPaint() }
                        function onCrossYChanged() { canvasSag.requestPaint() }
                        function onCrossAngleChanged() { canvasSag.requestPaint() }
                    }
                    Connections {
                        target: sagCrossOverlay
                        function onWidthChanged() { canvasSag.requestPaint() }
                        function onHeightChanged() { canvasSag.requestPaint() }
                    }
                }

                Rectangle {
                    id: vTop_s
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: sagCrossOverlay.vColor
                    visible: sagCrossOverlay.handlesVisible
                    x: sagittalView.crossX + sagCrossOverlay.handleOffset * Math.cos(sagCrossOverlay.vAngle) - width/2
                    y: sagittalView.crossY + sagCrossOverlay.handleOffset * Math.sin(sagCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { sagCrossOverlay.isDragging = true; startAngle = sagittalView.crossAngle - sagCrossOverlay.pointerAngleDeg(mouse.x + vTop_s.x, mouse.y + vTop_s.y) }
                        onPositionChanged: if (pressed) sagittalView.crossAngle = sagCrossOverlay.pointerAngleDeg(mouse.x + vTop_s.x, mouse.y + vTop_s.y) + startAngle
                        onReleased: sagCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: vBottom_s
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: sagCrossOverlay.vColor
                    visible: sagCrossOverlay.handlesVisible
                    x: sagittalView.crossX - sagCrossOverlay.handleOffset * Math.cos(sagCrossOverlay.vAngle) - width/2
                    y: sagittalView.crossY - sagCrossOverlay.handleOffset * Math.sin(sagCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { sagCrossOverlay.isDragging = true; startAngle = sagittalView.crossAngle - sagCrossOverlay.pointerAngleDeg(mouse.x + vBottom_s.x, mouse.y + vBottom_s.y) }
                        onPositionChanged: if (pressed) sagittalView.crossAngle = sagCrossOverlay.pointerAngleDeg(mouse.x + vBottom_s.x, mouse.y + vBottom_s.y) + startAngle
                        onReleased: sagCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hRight_s
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: sagCrossOverlay.hColor
                    visible: sagCrossOverlay.handlesVisible
                    x: sagittalView.crossX + sagCrossOverlay.handleOffset * Math.cos(sagCrossOverlay.hAngle) - width/2
                    y: sagittalView.crossY + sagCrossOverlay.handleOffset * Math.sin(sagCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { sagCrossOverlay.isDragging = true; startAngle = sagittalView.crossAngle - sagCrossOverlay.pointerAngleDeg(mouse.x + hRight_s.x, mouse.y + hRight_s.y) }
                        onPositionChanged: if (pressed) sagittalView.crossAngle = sagCrossOverlay.pointerAngleDeg(mouse.x + hRight_s.x, mouse.y + hRight_s.y) + startAngle
                        onReleased: sagCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hLeft_s
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: sagCrossOverlay.hColor
                    visible: sagCrossOverlay.handlesVisible
                    x: sagittalView.crossX - sagCrossOverlay.handleOffset * Math.cos(sagCrossOverlay.hAngle) - width/2
                    y: sagittalView.crossY - sagCrossOverlay.handleOffset * Math.sin(sagCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { sagCrossOverlay.isDragging = true; startAngle = sagittalView.crossAngle - sagCrossOverlay.pointerAngleDeg(mouse.x + hLeft_s.x, mouse.y + hLeft_s.y) }
                        onPositionChanged: if (pressed) sagittalView.crossAngle = sagCrossOverlay.pointerAngleDeg(mouse.x + hLeft_s.x, mouse.y + hLeft_s.y) + startAngle
                        onReleased: sagCrossOverlay.isDragging = false
                    }
                }
            }
        }
    }

    // 左下：冠状视图
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 5
        anchors.leftMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        CoronalView {
            id: coronalView
            anchors.fill: parent
            anchors.margins: 2
            property real crossX: width/2
            property real crossY: height/2
            property real crossAngle: 0
        }
        Item {
            anchors.fill: parent
            anchors.margins: 2
            z: 10
            MouseArea {
                anchors.fill: parent
                enabled: root.crosshairEnabled
                hoverEnabled: true
                onPressed: {
                    var i = clamp(Math.round(mouse.x / width * (root.dimX - 1)), 0, root.dimX - 1)
                    var k = clamp(Math.round(mouse.y / height * (root.dimZ - 1)), 0, root.dimZ - 1)
                    $DicomDataModel.sagittalSlice = i
                    $DicomDataModel.axialSlice = k
                    updateCrosshairFromVoxel(i, $DicomDataModel.coronalSlice, k)
                }
                onPositionChanged: {
                    if (pressed) {
                        var i = clamp(Math.round(mouse.x / width * (root.dimX - 1)), 0, root.dimX - 1)
                        var k = clamp(Math.round(mouse.y / height * (root.dimZ - 1)), 0, root.dimZ - 1)
                        $DicomDataModel.sagittalSlice = i
                        $DicomDataModel.axialSlice = k
                        updateCrosshairFromVoxel(i, $DicomDataModel.coronalSlice, k)
                    }
                }
            }
            Item {
                id: corCrossOverlay
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                clip: true

                readonly property real handleRadius: root.crossHandleDiameter * 0.5
                function maxCornerDistance(cx, cy, w, h) {
                    var d1 = Math.sqrt(cx*cx + cy*cy)
                    var d2 = Math.sqrt((w-cx)*(w-cx) + cy*cy)
                    var d3 = Math.sqrt(cx*cx + (h-cy)*(h-cy))
                    var d4 = Math.sqrt((w-cx)*(w-cx) + (h-cy)*(h-cy))
                    return Math.max(d1, d2, d3, d4)
                }
                readonly property real armLength: maxCornerDistance(coronalView.crossX, coronalView.crossY, width, height) + 10
                readonly property real handleOffset: root.crossHandleOffsetFixed
                readonly property color vColor: root.crossRed
                readonly property color hColor: root.crossYellow
                property real rotateOffsetDeg: 0

                // 圆球显示控制
                property bool showHandles: false
                property bool isDragging: false
                readonly property bool handlesVisible: showHandles || isDragging

                readonly property real angleRad: coronalView.crossAngle * Math.PI / 180
                readonly property real vAngle: angleRad - Math.PI / 2
                readonly property real hAngle: angleRad

                function pointerAngleDeg(mx, my) {
                    var dx = mx - coronalView.crossX
                    var dy = my - coronalView.crossY
                    return Math.atan2(dy, dx) * 180 / Math.PI
                }

                function distanceToLine(px, py, cx, cy, angle) {
                    return Math.abs((px - cx) * Math.sin(angle) - (py - cy) * Math.cos(angle))
                }

                function isNearCrossLine(mx, my) {
                    var d1 = distanceToLine(mx, my, coronalView.crossX, coronalView.crossY, vAngle)
                    var d2 = distanceToLine(mx, my, coronalView.crossX, coronalView.crossY, hAngle)
                    return Math.min(d1, d2) < root.crossHoverThreshold
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onPositionChanged: {
                        corCrossOverlay.showHandles = corCrossOverlay.isNearCrossLine(mouse.x, mouse.y)
                    }
                    onExited: corCrossOverlay.showHandles = false
                }

                Canvas {
                    id: canvasCor
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var cx = coronalView.crossX
                        var cy = coronalView.crossY
                        var arm = corCrossOverlay.armLength
                        var vAng = corCrossOverlay.vAngle
                        var hAng = corCrossOverlay.hAngle

                        ctx.strokeStyle = corCrossOverlay.vColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(vAng), cy + arm * Math.sin(vAng))
                        ctx.lineTo(cx - arm * Math.cos(vAng), cy - arm * Math.sin(vAng))
                        ctx.stroke()

                        ctx.strokeStyle = corCrossOverlay.hColor
                        ctx.lineWidth = root.crossThicknessThin
                        ctx.beginPath()
                        ctx.moveTo(cx + arm * Math.cos(hAng), cy + arm * Math.sin(hAng))
                        ctx.lineTo(cx - arm * Math.cos(hAng), cy - arm * Math.sin(hAng))
                        ctx.stroke()
                    }
                    Connections {
                        target: coronalView
                        function onCrossXChanged() { canvasCor.requestPaint() }
                        function onCrossYChanged() { canvasCor.requestPaint() }
                        function onCrossAngleChanged() { canvasCor.requestPaint() }
                    }
                    Connections {
                        target: corCrossOverlay
                        function onWidthChanged() { canvasCor.requestPaint() }
                        function onHeightChanged() { canvasCor.requestPaint() }
                    }
                }

                Rectangle {
                    id: vTop_c
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: corCrossOverlay.vColor
                    visible: corCrossOverlay.handlesVisible
                    x: coronalView.crossX + corCrossOverlay.handleOffset * Math.cos(corCrossOverlay.vAngle) - width/2
                    y: coronalView.crossY + corCrossOverlay.handleOffset * Math.sin(corCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { corCrossOverlay.isDragging = true; startAngle = coronalView.crossAngle - corCrossOverlay.pointerAngleDeg(mouse.x + vTop_c.x, mouse.y + vTop_c.y) }
                        onPositionChanged: if (pressed) coronalView.crossAngle = corCrossOverlay.pointerAngleDeg(mouse.x + vTop_c.x, mouse.y + vTop_c.y) + startAngle
                        onReleased: corCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: vBottom_c
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: corCrossOverlay.vColor
                    visible: corCrossOverlay.handlesVisible
                    x: coronalView.crossX - corCrossOverlay.handleOffset * Math.cos(corCrossOverlay.vAngle) - width/2
                    y: coronalView.crossY - corCrossOverlay.handleOffset * Math.sin(corCrossOverlay.vAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { corCrossOverlay.isDragging = true; startAngle = coronalView.crossAngle - corCrossOverlay.pointerAngleDeg(mouse.x + vBottom_c.x, mouse.y + vBottom_c.y) }
                        onPositionChanged: if (pressed) coronalView.crossAngle = corCrossOverlay.pointerAngleDeg(mouse.x + vBottom_c.x, mouse.y + vBottom_c.y) + startAngle
                        onReleased: corCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hRight_c
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: corCrossOverlay.hColor
                    visible: corCrossOverlay.handlesVisible
                    x: coronalView.crossX + corCrossOverlay.handleOffset * Math.cos(corCrossOverlay.hAngle) - width/2
                    y: coronalView.crossY + corCrossOverlay.handleOffset * Math.sin(corCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { corCrossOverlay.isDragging = true; startAngle = coronalView.crossAngle - corCrossOverlay.pointerAngleDeg(mouse.x + hRight_c.x, mouse.y + hRight_c.y) }
                        onPositionChanged: if (pressed) coronalView.crossAngle = corCrossOverlay.pointerAngleDeg(mouse.x + hRight_c.x, mouse.y + hRight_c.y) + startAngle
                        onReleased: corCrossOverlay.isDragging = false
                    }
                }
                Rectangle {
                    id: hLeft_c
                    width: root.crossHandleDiameter; height: root.crossHandleDiameter; radius: width/2
                    color: corCrossOverlay.hColor
                    visible: corCrossOverlay.handlesVisible
                    x: coronalView.crossX - corCrossOverlay.handleOffset * Math.cos(corCrossOverlay.hAngle) - width/2
                    y: coronalView.crossY - corCrossOverlay.handleOffset * Math.sin(corCrossOverlay.hAngle) - height/2
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.crosshairEnabled
                        property real startAngle: 0
                        onPressed: { corCrossOverlay.isDragging = true; startAngle = coronalView.crossAngle - corCrossOverlay.pointerAngleDeg(mouse.x + hLeft_c.x, mouse.y + hLeft_c.y) }
                        onPositionChanged: if (pressed) coronalView.crossAngle = corCrossOverlay.pointerAngleDeg(mouse.x + hLeft_c.x, mouse.y + hLeft_c.y) + startAngle
                        onReleased: corCrossOverlay.isDragging = false
                    }
                }
            }
        }
    }

    // 右下：3D体渲染视图
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 5
        anchors.rightMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        VolumeView {
            id: volumeView
            anchors.fill: parent
            anchors.margins: 2
        }
    }
}
