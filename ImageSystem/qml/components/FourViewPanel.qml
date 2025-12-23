import QtQuick 2.15
import QtQuick.Controls 2.15
import com.vtk.dicom 1.0

// 可复用的四视图显示面板
Item {
    id: root
    
    // 十字线功能总开关（先屏蔽：false）
    property bool crosshairEnabled: false

    // 公共十字线属性（荧光绿）
    readonly property color crossColor: "#00ff7f"
    readonly property int crossThickness: 2
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
                    // axial slice 不变，使用当前
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
            Canvas {
                id: canvasAxial
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = root.crossColor
                    ctx.lineWidth = root.crossThickness
                    ctx.beginPath()
                    ctx.moveTo(axialView.crossX, 0)
                    ctx.lineTo(axialView.crossX, height)
                    ctx.moveTo(0, axialView.crossY)
                    ctx.lineTo(width, axialView.crossY)
                    ctx.stroke()
                }
                Connections {
                    target: axialView
                    function onCrossXChanged() { canvasAxial.requestPaint() }
                    function onCrossYChanged() { canvasAxial.requestPaint() }
                    function onWidthChanged() { canvasAxial.requestPaint() }
                    function onHeightChanged() { canvasAxial.requestPaint() }
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
            Canvas {
                id: canvasSag
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = root.crossColor
                    ctx.lineWidth = root.crossThickness
                    ctx.beginPath()
                    ctx.moveTo(sagittalView.crossX, 0)
                    ctx.lineTo(sagittalView.crossX, height)
                    ctx.moveTo(0, sagittalView.crossY)
                    ctx.lineTo(width, sagittalView.crossY)
                    ctx.stroke()
                }
                Connections {
                    target: sagittalView
                    function onCrossXChanged() { canvasSag.requestPaint() }
                    function onCrossYChanged() { canvasSag.requestPaint() }
                    function onWidthChanged() { canvasSag.requestPaint() }
                    function onHeightChanged() { canvasSag.requestPaint() }
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
            Canvas {
                id: canvasCor
                anchors.fill: parent
                z: 11
                visible: root.crosshairEnabled
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = root.crossColor
                    ctx.lineWidth = root.crossThickness
                    ctx.beginPath()
                    ctx.moveTo(coronalView.crossX, 0)
                    ctx.lineTo(coronalView.crossX, height)
                    ctx.moveTo(0, coronalView.crossY)
                    ctx.lineTo(width, coronalView.crossY)
                    ctx.stroke()
                }
                Connections {
                    target: coronalView
                    function onCrossXChanged() { canvasCor.requestPaint() }
                    function onCrossYChanged() { canvasCor.requestPaint() }
                    function onWidthChanged() { canvasCor.requestPaint() }
                    function onHeightChanged() { canvasCor.requestPaint() }
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

