import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import com.vtk.dicom 1.0
import QtGraphicalEffects 1.0
import "./components"
ApplicationWindow {
    id: win
    visible: true
    // 按当前屏幕可用区域（不含任务栏）比例显示，并限制最小尺寸
    property int minWindowWidth: 800
    property int minWindowHeight: 600
    width: Math.max(minWindowWidth, Math.floor(Screen.desktopAvailableWidth * 0.82))
    height: Math.max(minWindowHeight, Math.floor(Screen.desktopAvailableHeight * 0.82))
    Component.onCompleted: {
        var sw = Screen.desktopAvailableWidth
        var sh = Screen.desktopAvailableHeight
        var sx = Screen.virtualX
        var sy = Screen.virtualY
        win.x = Math.floor((sw - win.width) / 2 + sx)
        win.y = Math.floor((sh - win.height) / 2 + sy)
    }
    //title: qsTr("脑健康影像辅助定量分析系统")AetherDesk
    title: qsTr("AetherDesk")
    color: "#000000"
    property int analysisPanelIndex: 0
    property bool showAIPanel: false
    font.family: "Alibaba PuHuiTi 3.0"
    font.pixelSize: 14
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint
    // 对话框消息组件
    MessageBox {
        id: dialogMessageBox
        anchors.fill: parent
    }

    LogWindow {
        id: logWindow
    }

    Window {
        id: dataStoreWindow
        width: 1824
        height: 950
        visible: false
        title: qsTr("数据上传")
        color: "#0C1120"
        flags: Qt.Window | Qt.FramelessWindowHint

        function centerWindow() {
            var sw = Screen.desktopAvailableWidth
            var sh = Screen.desktopAvailableHeight
            var sx = Screen.virtualX
            var sy = Screen.virtualY
            x = Math.floor((sw - width) / 2 + sx)
            y = Math.floor((sh - height) / 2 + sy)
        }

        Component.onCompleted: centerWindow()
        onVisibleChanged: if (visible) centerWindow()

        DataStorePanel {
            id: dataStorePanel
            anchors.fill: parent
            messageManager: dialogMessageBox
            onViewAnalysisRequested: {
                analysisPanelIndex = 2
                dataStoreWindow.hide()
                win.raise()
                win.requestActivate()
                brainpanel.loadOutputDirectory(caseInfo.outputPath, caseInfo)
            }
        }
    }
    
    // 监听 C++ 层的消息请求
    Connections {
        target: $DicomDataModel
        function onShowMessageRequested(type, text) {
            dialogMessageBox.showMessage(type, text, 2000)
        }
    }
    // 窗口边缘调整大小相关属性
    property int resizeEdgeSize: 6

    // 左边缘
    MouseArea {
        id: leftEdge
        width: resizeEdgeSize
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: resizeEdgeSize
        anchors.bottomMargin: resizeEdgeSize
        cursorShape: Qt.SizeHorCursor
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var newWidth = win.width - dx
                if (newWidth >= minWindowWidth) {
                    win.x += dx
                    win.width = newWidth
                }
            }
        }
    }

    // 右边缘
    MouseArea {
        id: rightEdge
        width: resizeEdgeSize
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: resizeEdgeSize
        anchors.bottomMargin: resizeEdgeSize
        cursorShape: Qt.SizeHorCursor
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var newWidth = win.width + dx
                if (newWidth >= minWindowWidth) {
                    win.width = newWidth
                }
            }
        }
    }

    // 上边缘
    MouseArea {
        id: topEdge
        height: resizeEdgeSize
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: resizeEdgeSize
        anchors.rightMargin: resizeEdgeSize
        cursorShape: Qt.SizeVerCursor
        z: 100  // 确保在标题栏之上
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dy = mouse.y - pressPos.y
                var newHeight = win.height - dy
                if (newHeight >= minWindowHeight) {
                    win.y += dy
                    win.height = newHeight
                }
            }
        }
    }

    // 下边缘
    MouseArea {
        id: bottomEdge
        height: resizeEdgeSize
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: resizeEdgeSize
        anchors.rightMargin: resizeEdgeSize
        cursorShape: Qt.SizeVerCursor
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dy = mouse.y - pressPos.y
                var newHeight = win.height + dy
                if (newHeight >= minWindowHeight) {
                    win.height = newHeight
                }
            }
        }
    }

    // 左上角
    MouseArea {
        id: topLeftCorner
        width: resizeEdgeSize
        height: resizeEdgeSize
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeFDiagCursor
        z: 100  // 确保在标题栏之上
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var dy = mouse.y - pressPos.y
                var newWidth = win.width - dx
                var newHeight = win.height - dy
                if (newWidth >= minWindowWidth) {
                    win.x += dx
                    win.width = newWidth
                }
                if (newHeight >= minWindowHeight) {
                    win.y += dy
                    win.height = newHeight
                }
            }
        }
    }

    // 右上角
    MouseArea {
        id: topRightCorner
        width: resizeEdgeSize
        height: resizeEdgeSize
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeBDiagCursor
        z: 100  // 确保在标题栏之上
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var dy = mouse.y - pressPos.y
                var newWidth = win.width + dx
                var newHeight = win.height - dy
                if (newWidth >= minWindowWidth) {
                    win.width = newWidth
                }
                if (newHeight >= minWindowHeight) {
                    win.y += dy
                    win.height = newHeight
                }
            }
        }
    }

    // 左下角
    MouseArea {
        id: bottomLeftCorner
        width: resizeEdgeSize
        height: resizeEdgeSize
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeBDiagCursor
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var dy = mouse.y - pressPos.y
                var newWidth = win.width - dx
                var newHeight = win.height + dy
                if (newWidth >= minWindowWidth) {
                    win.x += dx
                    win.width = newWidth
                }
                if (newHeight >= minWindowHeight) {
                    win.height = newHeight
                }
            }
        }
    }

    // 右下角
    MouseArea {
        id: bottomRightCorner
        width: resizeEdgeSize
        height: resizeEdgeSize
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeFDiagCursor
        property point pressPos

        onPressed: pressPos = Qt.point(mouse.x, mouse.y)
        onPositionChanged: {
            if (pressed) {
                var dx = mouse.x - pressPos.x
                var dy = mouse.y - pressPos.y
                var newWidth = win.width + dx
                var newHeight = win.height + dy
                if (newWidth >= minWindowWidth) {
                    win.width = newWidth
                }
                if (newHeight >= minWindowHeight) {
                    win.height = newHeight
                }
            }
        }
    }
    // 自定义标题栏
    Rectangle {
        id: customTitleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        color: "#5F656D"

        // 拖动区域 - 允许拖动窗口
        MouseArea {
            id: titleBarDragArea
            anchors.fill: parent
            anchors.rightMargin: windowControlButtons.width

            property point clickPos

            onPressed: {
                clickPos = Qt.point(mouse.x, mouse.y)
            }

            onPositionChanged: {
                if (pressed) {
                    var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                    win.x += delta.x
                    win.y += delta.y
                }
            }

            // 双击最大化/还原
            onDoubleClicked: {
                if (win.visibility === Window.Maximized) {
                    win.showNormal()
                } else {
                    win.showMaximized()
                }
            }
        }

        // 左侧：应用图标和标题
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Text {
                text: "AetherDesk"
                color: "#B2FFFFFF"
                font.pixelSize: 14
                font.weight: Font.Medium
                font.family: "Alibaba PuHuiTi 3.0"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 右侧：窗口控制按钮
        Row {
            id: windowControlButtons
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            // 最小化按钮
            Rectangle {
                width: 46
                height: parent.height
                color: minimizeArea.containsMouse ? "#3C3C3C" : "transparent"

                Text {
                    text: "—"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: minimizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.showMinimized()
                }
            }

            // 最大化/还原按钮
            Rectangle {
                width: 46
                height: parent.height
                color: maximizeArea.containsMouse ? "#3C3C3C" : "transparent"

                Text {
                    text: win.visibility === Window.Maximized ? "❐" : "☐"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: maximizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (win.visibility === Window.Maximized) {
                            win.showNormal()
                        } else {
                            win.showMaximized()
                        }
                    }
                }
            }

            // 关闭按钮
            Rectangle {
                width: 46
                height: parent.height
                color: closeArea.containsMouse ? "#E81123" : "transparent"

                Text {
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.quit()
                }
            }
        }
    }
    // 顶部功能栏
    Rectangle {
        id: topToolbar
        anchors.top: customTitleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 66
        color: "#CC303338"
        
        FileDialog {
            id: fileDialog
            title: qsTr("选择要上传的文件")
            selectFolder: true
            onAccepted: {
                $DicomDataModel.loadDicomDirectory(fileDialog.fileUrls[0])
            }
        }
        Row{
            height: parent.height
            leftPadding: 20
            spacing: 20
            Image{
                id: openButton
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/image/fileUpload.png"
                opacity: 1.0
                scale: 1.0
                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                Behavior on scale {
                    NumberAnimation { duration: 200 }
                }

                MouseArea{
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    onEntered: {
                        openButton.opacity = 0.8
                        openButton.scale = 1.05
                    }

                    onExited: {
                        openButton.opacity = 1.0
                        openButton.scale = 1.0
                    }

                    onPressed: {
                        openButton.source = "qrc:/image/fileUpload-pressed.png"
                        openButton.scale = 0.95
                    }

                    onReleased: {
                        openButton.source = "qrc:/image/fileUpload.png"
                        openButton.scale = containsMouse ? 1.05 : 1.0
                    }

                    onClicked: {
                        dataStoreWindow.show()
                        dataStoreWindow.raise()
                        dataStoreWindow.requestActivate()
                    }
                }
            }

            Rectangle {
                width: 1
                height: 50
                color: "#80FFFFFF"
                anchors.verticalCenter: parent.verticalCenter
            }

            // DICOM信息显示
            Label {
                id: infoText
                anchors.verticalCenter: parent.verticalCenter
                text: $DicomDataModel.hasData ? $DicomDataModel.dicomInfo : "未加载 DICOM 数据"
                color: "#80FFFFFF"
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }
        Row{
            anchors.right: parent.right
            height: parent.height
            rightPadding: 20
            spacing: 15
            
            // 带背景图片的按钮
            Item {
                id: kidneyButton
                anchors.verticalCenter: parent.verticalCenter
                width: kidneyBtnImage.width
                height: kidneyBtnImage.height
                
                property bool isSelected: analysisPanelIndex === 1
                
                onIsSelectedChanged: {
                    if (isSelected) {
                        kidneySelectAnimation.start()
                    } else {
                        kidneyBtnImage.source = "qrc:/image/btnBackground.png"
                    }
                }
                
                Image {
                    id: kidneyBtnImage
                    source: analysisPanelIndex === 1 ? "qrc:/image/btnBackgroundSelected4.png" : "qrc:/image/btnBackground.png"
                    opacity: 1.0
                    scale: 1.0
                    
                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                    
                    Behavior on scale {
                        NumberAnimation { duration: 200 }
                    }
                    
                    Row{
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 5
                        Image{
                            source: "qrc:/image/kidneyIcon.png"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        // 按钮文字
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "肾功能"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                    }
                }
                
                SequentialAnimation {
                    id: kidneySelectAnimation
                    PropertyAction {
                        target: kidneyBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected1.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: kidneyBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected2.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: kidneyBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected3.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: kidneyBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected4.png"
                    }
                }
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    
                    onEntered: {
                        if (!kidneyButton.isSelected) {
                            kidneyBtnImage.opacity = 0.8
                            kidneyBtnImage.scale = 1.05
                        }
                    }
                    
                    onExited: {
                        if (!kidneyButton.isSelected) {
                            kidneyBtnImage.opacity = 1.0
                            kidneyBtnImage.scale = 1.0
                        }
                    }
                    
                    onPressed: {
                        kidneyBtnImage.scale = 0.95
                    }
                    
                    onReleased: {
                        kidneyBtnImage.scale = containsMouse && !kidneyButton.isSelected ? 1.05 : 1.0
                    }
                    
                    onClicked: {
                        if(analysisPanelIndex === 1){
                            analysisPanelIndex = 0
                        } else{
                            analysisPanelIndex = 1
                        }
                    }
                }
            }
            Item {
                id: brainButton
                anchors.verticalCenter: parent.verticalCenter
                width: brainBtnImage.width
                height: brainBtnImage.height

                property bool isSelected: analysisPanelIndex === 2
                
                onIsSelectedChanged: {
                    if (isSelected) {
                        brainSelectAnimation.start()
                    } else {
                        brainBtnImage.source = "qrc:/image/btnBackground.png"
                    }
                }

                Image {
                    id: brainBtnImage
                    source: analysisPanelIndex === 2 ? "qrc:/image/btnBackgroundSelected4.png" : "qrc:/image/btnBackground.png"
                    opacity: 1.0
                    scale: 1.0

                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }

                    Behavior on scale {
                        NumberAnimation { duration: 200 }
                    }

                    Row{
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 5
                        Image{
                            source: "qrc:/image/brainIcon.png"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        // 按钮文字
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "脑功能"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.family: "Alibaba PuHuiTi 3.0"
                        }
                    }
                }

                SequentialAnimation {
                    id: brainSelectAnimation
                    PropertyAction {
                        target: brainBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected1.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: brainBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected2.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: brainBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected3.png"
                    }
                    PauseAnimation { duration: 100 }
                    PropertyAction {
                        target: brainBtnImage
                        property: "source"
                        value: "qrc:/image/btnBackgroundSelected4.png"
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    onEntered: {
                        if (!brainButton.isSelected) {
                            brainBtnImage.opacity = 0.8
                            brainBtnImage.scale = 1.05
                        }
                    }

                    onExited: {
                        if (!brainButton.isSelected) {
                            brainBtnImage.opacity = 1.0
                            brainBtnImage.scale = 1.0
                        }
                    }

                    onPressed: {
                        brainBtnImage.scale = 0.95
                    }

                    onReleased: {
                        brainBtnImage.scale = containsMouse && !brainButton.isSelected ? 1.05 : 1.0
                    }

                    onClicked: {
                        if(analysisPanelIndex === 2){
                            analysisPanelIndex = 0
                        } else{
                            analysisPanelIndex = 2
                        }
                    }
                }
            }

            Image{
                id: aiBtnImage
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/image/aiBtn.png"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true

                    onEntered: {
                        aiBtnImage.opacity = 0.8
                        aiBtnImage.scale = 1.05
                    }

                    onExited: {
                        aiBtnImage.opacity = 1.0
                        aiBtnImage.scale = 1.0
                    }

                    onPressed: {
                        aiBtnImage.scale = 0.95
                    }

                    onReleased: {
                        aiBtnImage.scale = containsMouse ? 1.05 : 1.0
                    }

                    onClicked: {
                        win.showAIPanel = true
                    }
                }
            }
        }
    }
    // 顶部扩展栏
    Rectangle {
        id: topExpandBar
        width: parent.width
        height: 16
        anchors.top: topToolbar.bottom
        color: "transparent"
        
        property bool leftExpanded: true
        property bool rightExpanded: true
        property bool brainRightExpanded: true
        
        Rectangle{
            id: leftExpandButton
            width: 60
            height: parent.height
            color: "#171717"
            
            Image{
                id: leftArrowImage
                source: topExpandBar.leftExpanded ? "qrc:/image/arrowLeft.png" : "qrc:/image/arrowRight.png"
                anchors.centerIn: parent
            }
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    topExpandBar.leftExpanded = !topExpandBar.leftExpanded
                }
            }
        }
        
        Rectangle {
            id: rightExpandButton
            width: (analysisPanelIndex === 2 && brainpanel.currentIndex === 5) ? 0 : (analysisPanelIndex === 2 && (brainpanel.currentIndex === 2 || brainpanel.currentIndex === 3) ? 500 : 400)
            height: parent.height
            visible: brainpanel.currentIndex !== 5 || analysisPanelIndex !== 2
            color: "#171717"
            anchors.right: parent.right
            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }
            Image{
                id: rightArrowImage
                source: {
                    if (analysisPanelIndex === 2) {
                        return topExpandBar.brainRightExpanded ? "qrc:/image/arrowRight.png" : "qrc:/image/arrowLeft.png"
                    } else {
                        return topExpandBar.rightExpanded ? "qrc:/image/arrowRight.png" : "qrc:/image/arrowLeft.png"
                    }
                }
                anchors.centerIn: parent
            }
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (analysisPanelIndex === 2) {
                        topExpandBar.brainRightExpanded = !topExpandBar.brainRightExpanded
                    } else {
                        topExpandBar.rightExpanded = !topExpandBar.rightExpanded
                    }
                }
            }
        }
    }
    
    // 左侧工具栏
    Rectangle{
        id: leftToolbar
        anchors.top: topExpandBar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: topExpandBar.leftExpanded ? 60 : 0
        color: "#CC303338"
        visible: width > 0
        clip: true
        Behavior on width {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }
        Column{
            id: leftToolColumn
            anchors.fill: parent
            leftPadding: 12
            rightPadding: 12
            topPadding: 16
            bottomPadding: 16
            spacing: 16
            LeftToolButton {
                id: moveToolBtn
                iconSource: "qrc:/image/toolMove.png"
                isSelected: $DicomDataModel.toolMode === 0 && !$DicomDataModel.crosshairEnabled
                onClicked: {
                    if(!moveToolBtn.isSelected){
                        $DicomDataModel.setToolMode(0)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: windowToolBtn
                iconSource: "qrc:/image/toolWindow.png"
                visible: !$DicomDataModel.isSegDataMode
                isSelected: $DicomDataModel.toolMode === 1
                onClicked: {
                    if(!windowToolBtn.isSelected){
                        $DicomDataModel.setToolMode(1)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: vectorToolBtn
                iconSource: "qrc:/image/toolVector.png"
                isSelected: $DicomDataModel.toolMode === 3
                onClicked: {
                    if(!vectorToolBtn.isSelected){
                        $DicomDataModel.setToolMode(3)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: largeToolBtn
                iconSource: "qrc:/image/toolLarge.png"
                isSelected: $DicomDataModel.toolMode === 4
                onClicked: {
                    if(!largeToolBtn.isSelected){
                        $DicomDataModel.setToolMode(4)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: measureDistanceToolBtn
                iconSource: "qrc:/image/toolDistance.png"
                isSelected: $DicomDataModel.toolMode === 5
                onClicked: {
                    if(!measureDistanceToolBtn.isSelected){
                        $DicomDataModel.setToolMode(5)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: measureAngleToolBtn
                iconSource: "qrc:/image/toolAngle.png"
                isSelected: $DicomDataModel.toolMode === 6
                onClicked: {
                    if(!measureAngleToolBtn.isSelected){
                        $DicomDataModel.setToolMode(6)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: lineToolBtn
                iconSource: "qrc:/image/toolLine.png"
                isSelected: $DicomDataModel.toolMode === 0 && $DicomDataModel.crosshairEnabled
                onClicked: {
                    if(!lineToolBtn.isSelected){
                        $DicomDataModel.setToolMode(0)
                        $DicomDataModel.setCrosshairEnabled(true)
                    }else{
                        $DicomDataModel.setToolMode(0)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }
                }
            }
            LeftToolButton {
                id: annotationRecToolBtn
                iconSource: "qrc:/image/toolRectangle.png"
                isSelected: $DicomDataModel.toolMode === 7
                onClicked: {
                    if(!annotationRecToolBtn.isSelected){
                        $DicomDataModel.setToolMode(7)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: annotationCircleToolBtn
                iconSource: "qrc:/image/toolCircle.png"
                isSelected: $DicomDataModel.toolMode === 8
                onClicked: {
                    if(!annotationCircleToolBtn.isSelected){
                        $DicomDataModel.setToolMode(8)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: annotationPenToolBtn
                iconSource: "qrc:/image/toolPen.png"
                isSelected: $DicomDataModel.toolMode === 9
                onClicked: {
                    if(!annotationPenToolBtn.isSelected){
                        $DicomDataModel.setToolMode(9)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(0)
                    }
                }
            }
            LeftToolButton {
                id: screenshotToolBtn
                iconSource: "qrc:/image/toolScreenshot.png"
                isSelected: expandSelection.visible
                onClicked: {
                    expandSelection.visible = !expandSelection.visible
                }
            }

            LeftToolButton {
                id: resetToolBtn
                iconSource: "qrc:/image/toolReset.png"
                onClicked: {
                    $DicomDataModel.resetAllInteractions()
                }
            }
        }
    }

    // 截图选择面板
    Rectangle{
        id: expandSelection
        z: 1000
        y: screenshotToolBtn.y + topToolbar.height + topExpandBar.height
        x: screenshotToolBtn.x + screenshotToolBtn.width + 8
        height: expandColumn.height + 20
        width: 120
        color: "#171717"
        radius: 4
        visible: false
        
        Column{
            id: expandColumn
            width: parent.width
            anchors.centerIn: parent
            spacing: 8
            padding: 10
            
            CustomButton {
                text: qsTr("轴向视图")
                width: parent.width - 20
                height: 32
                backgroundColor: "#3C7EFF"
                onClicked: {
                    screenshotFileDialog.viewType = 0
                    screenshotFileDialog.open()
                    expandSelection.visible = false
                }
            }
            
            CustomButton {
                text: qsTr("矢状视图")
                width: parent.width - 20
                height: 32
                backgroundColor: "#3C7EFF"
                onClicked: {
                    screenshotFileDialog.viewType = 1
                    screenshotFileDialog.open()
                    expandSelection.visible = false
                }
            }
            
            CustomButton {
                text: qsTr("冠状视图")
                width: parent.width - 20
                height: 32
                backgroundColor: "#3C7EFF"
                onClicked: {
                    screenshotFileDialog.viewType = 2
                    screenshotFileDialog.open()
                    expandSelection.visible = false
                }
            }
            
            CustomButton {
                text: qsTr("3D视图")
                width: parent.width - 20
                height: 32
                backgroundColor: "#3C7EFF"
                onClicked: {
                    screenshotFileDialog.viewType = 3
                    screenshotFileDialog.open()
                    expandSelection.visible = false
                }
            }
        }
    }
    
    // 截图保存对话框
    FileDialog {
        id: screenshotFileDialog
        title: qsTr("选择保存位置")
        selectFolder: false
        selectExisting: false
        nameFilters: ["PNG图片 (*.png)", "JPEG图片 (*.jpg)", "所有文件 (*)"]
        defaultSuffix: "png"
        property int viewType: 0  // 0=Axial, 1=Sagittal, 2=Coronal, 3=Volume
        
        onAccepted: {
            var filePath = screenshotFileDialog.fileUrl.toString()
            if (filePath.startsWith("file:///")) {
                filePath = filePath.substring(8)
            }
            $MainViewController.captureViewScreenshot(viewType, filePath)
        }
    }
    // 右侧控制面板
    Item {
        id: rightPanel
        anchors.top: topExpandBar.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 16
        width: topExpandBar.rightExpanded ? 400 : 0
        visible: analysisPanelIndex !== 2 && width > 0
        clip: true

        Behavior on width {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }
        Image {
            source: "qrc:/image/rightBackground.png"
            anchors.fill: parent
        }
        Column{
            anchors.fill: parent
            padding: 16
            spacing: 12
            Rectangle {
                id: sliceRec
                width: parent.width - 32
                height: sliceCol.height
                color: "#E016171B"
                radius: 8
                Column {
                    id: sliceCol
                    width: parent.width
                    spacing: 12
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 16
                    bottomPadding: 24
                    Row {
                        height: 32
                        width: parent.width - 24
                        spacing: 6
                        Image {
                            source: "qrc:/image/sliceIcon.png"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("切片控制")
                            color: "#E5FFFFFF"
                            font.pixelSize: 18
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    Column {
                        width: parent.width - 24
                        spacing: 8
                        Rectangle{
                            width: parent.width
                            height: 24
                            color: "transparent"
                            Label {
                                text: qsTr("轴向切片")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr($DicomDataModel.axialSlice + " / " + $DicomDataModel.maxAxialSlice)
                                color: "#3C7EFF"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                            }
                        }
                        Slider {
                            id: axialSlider
                            width: parent.width
                            from: 0
                            to: $DicomDataModel.maxAxialSlice
                            stepSize: 1
                            enabled: $DicomDataModel.hasData

                            Component.onCompleted: {
                                value = $DicomDataModel.axialSlice
                            }

                            Connections {
                                target: $DicomDataModel
                                function onAxialSliceChanged(slice) {
                                    if (!axialSlider.pressed) {
                                        axialSlider.value = slice
                                    }
                                }
                            }

                            onMoved: {
                                $DicomDataModel.axialSlice = value
                            }

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: parent.availableWidth
                                height: 4
                                radius: 6
                                color: "#454854"

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#3C7EFF"
                                    radius: 6
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: 10
                                height: 10
                                radius: 10
                                color: parent.pressed ? "#0078d4" : "#3C7EFF"
                                border.color: "#ffffff"
                                border.width: 2
                            }
                        }
                    }
                    Column {
                        width: parent.width - 24
                        spacing: 8
                        Rectangle{
                            width: parent.width
                            height: 24
                            color: "transparent"
                            Label {
                                text: qsTr("矢状切片")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr($DicomDataModel.sagittalSlice + " / " + $DicomDataModel.maxSagittalSlice)
                                color: "#3C7EFF"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                            }
                        }
                        Slider {
                            id: sagittalSlider
                            width: parent.width
                            from: 0
                            to: $DicomDataModel.maxSagittalSlice
                            stepSize: 1
                            enabled: $DicomDataModel.hasData

                            Component.onCompleted: {
                                value = $DicomDataModel.sagittalSlice
                            }

                            Connections {
                                target: $DicomDataModel
                                function onSagittalSliceChanged(slice) {
                                    if (!sagittalSlider.pressed) {
                                        sagittalSlider.value = slice
                                    }
                                }
                            }

                            onMoved: {
                                $DicomDataModel.sagittalSlice = value
                            }

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: parent.availableWidth
                                height: 4
                                radius: 6
                                color: "#454854"

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#3C7EFF"
                                    radius: 6
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: 10
                                height: 10
                                radius: 10
                                color: parent.pressed ? "#0078d4" : "#3C7EFF"
                                border.color: "#ffffff"
                                border.width: 2
                            }
                        }
                    }
                    Column {
                        width: parent.width - 24
                        spacing: 8
                        Rectangle{
                            width: parent.width
                            height: 24
                            color: "transparent"
                            Label {
                                text: qsTr("冠状切片")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr($DicomDataModel.coronalSlice + " / " + $DicomDataModel.maxCoronalSlice)
                                color: "#3C7EFF"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                            }
                        }
                        Slider {
                            id: coronalSlider
                            width: parent.width
                            from: 0
                            to: $DicomDataModel.maxCoronalSlice
                            stepSize: 1
                            enabled: $DicomDataModel.hasData

                            Component.onCompleted: {
                                value = $DicomDataModel.coronalSlice
                            }

                            Connections {
                                target: $DicomDataModel
                                function onCoronalSliceChanged(slice) {
                                    if (!coronalSlider.pressed) {
                                        coronalSlider.value = slice
                                    }
                                }
                            }

                            onMoved: {
                                $DicomDataModel.coronalSlice = value
                            }

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: parent.availableWidth
                                height: 4
                                radius: 6
                                color: "#454854"

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#3C7EFF"
                                    radius: 6
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: 10
                                height: 10
                                radius: 10
                                color: parent.pressed ? "#0078d4" : "#3C7EFF"
                                border.color: "#ffffff"
                                border.width: 2
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: windowRec
                width: parent.width - 32
                height: windowCol.height
                color: "#E016171B"
                radius: 8
                Column {
                    id: windowCol
                    width: parent.width
                    spacing: 12
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 16
                    bottomPadding: 24
                    Row {
                        height: 32
                        width: parent.width - 24
                        spacing: 6
                        Image {
                            source: "qrc:/image/windowIcon.png"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("窗宽窗位")
                            color: "#E5FFFFFF"
                            font.pixelSize: 18
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    Column {
                        width: parent.width - 24
                        spacing: 8
                        Rectangle{
                            width: parent.width
                            height: 24
                            color: "transparent"
                            Label {
                                text: qsTr("窗宽(Width)")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr(Math.round($DicomDataModel.windowWidth).toString())
                                color: "#27C346"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                            }
                        }
                        Slider {
                            id: windowWidthSlider
                            width: parent.width
                            from: 1
                            to: 4000
                            stepSize: 10
                            enabled: $DicomDataModel.hasData

                            Component.onCompleted: {
                                value = $DicomDataModel.windowWidth
                            }

                            Connections {
                                target: $DicomDataModel
                                function onWindowWidthChanged(width) {
                                    if (!windowWidthSlider.pressed) {
                                        windowWidthSlider.value = width
                                    }
                                }
                            }

                            onMoved: {
                                $DicomDataModel.windowWidth = value
                            }

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: parent.availableWidth
                                height: 4
                                radius: 6
                                color: "#454854"

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#27C346"
                                    radius: 6
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: 10
                                height: 10
                                radius: 10
                                color: parent.pressed ? "#00aa00" : "#27C346"
                                border.color: "#ffffff"
                                border.width: 2
                            }
                        }
                    }
                    Column {
                        width: parent.width - 24
                        spacing: 8
                        Rectangle{
                            width: parent.width
                            height: 24
                            color: "transparent"
                            Label {
                                text: qsTr("窗位(Level)")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr(Math.round($DicomDataModel.windowLevel).toString())
                                color: "#27C346"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                            }
                        }
                        Slider {
                            id: windowLevelSlider
                            width: parent.width
                            from: 1
                            to: 4000
                            stepSize: 10
                            enabled: $DicomDataModel.hasData

                            Component.onCompleted: {
                                value = $DicomDataModel.windowLevel
                            }

                            Connections {
                                target: $DicomDataModel
                                function onWindowLevelChanged(width) {
                                    if (!windowLevelSlider.pressed) {
                                        windowLevelSlider.value = width
                                    }
                                }
                            }

                            onMoved: {
                                $DicomDataModel.windowLevel = value
                            }

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: parent.availableWidth
                                height: 4
                                radius: 6
                                color: "#454854"

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#27C346"
                                    radius: 6
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                width: 10
                                height: 10
                                radius: 10
                                color: parent.pressed ? "#00aa00" : "#27C346"
                                border.color: "#ffffff"
                                border.width: 2
                            }
                        }
                    }
                }
            }

            Rectangle{
                id: infoRec
                width: parent.width - 32
                height: parent.height - 24 - 32 - sliceRec.height - windowRec.height
                color: "#E016171B"
                radius: 8
                ScrollView{
                    anchors.fill: parent
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 16
                    bottomPadding: 24
                    clip: true
                    Column {
                        id: infoCol
                        width: parent.width - 24
                        spacing: 12
                        Row {
                            height: 32
                            width: parent.width
                            spacing: 6
                            Image {
                                source: "qrc:/image/infoIcon.png"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("使用说明")
                                color: "#E5FFFFFF"
                                font.pixelSize: 18
                                font.weight: Font.Medium
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row{
                            height: 29
                            width: parent.width
                            spacing: 10
                            Rectangle {
                                width: 10
                                height: 10
                                radius:10
                                color: "#D26913"
                                border.width: 2
                                border.color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("三视图左键拖动：调整窗宽窗位")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row{
                            height: 29
                            width: parent.width
                            spacing: 10
                            Rectangle {
                                width: 10
                                height: 10
                                radius:10
                                color: "#D26913"
                                border.width: 2
                                border.color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("鼠标滚轮：切片浏览")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row{
                            height: 29
                            width: parent.width
                            spacing: 10
                            Rectangle {
                                width: 10
                                height: 10
                                radius:10
                                color: "#D26913"
                                border.width: 2
                                border.color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("3D视图：旋转查看")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row{
                            height: 29
                            width: parent.width
                            spacing: 10
                            Rectangle {
                                width: 10
                                height: 10
                                radius:10
                                color: "#D26913"
                                border.width: 2
                                border.color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("预设窗口：快速切换")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Row{
                            height: 29
                            width: parent.width
                            spacing: 10
                            Rectangle {
                                width: 10
                                height: 10
                                radius:10
                                color: "#D26913"
                                border.width: 2
                                border.color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: qsTr("右下角实时显示窗宽窗位")
                                color: "#E5FFFFFF"
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }
    //左侧功能区域
    Rectangle {
        id: leftAnalysisPanel
        anchors.top: topExpandBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: leftToolbar.right
        anchors.leftMargin: 16
        width: 280
        color: "transparent"
        visible: analysisPanelIndex !== 0 && analysisPanelIndex !== 2
        KidneyPanel {
            id: kidneypanel
            visible: analysisPanelIndex === 1
        }
    }

    // 主四视图容器（当不在脑分割面板时使用）
    Item {
        id: mainFourViewContainer
        anchors.top: topExpandBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: analysisPanelIndex !== 0 ? leftAnalysisPanel.right : leftToolbar.right
        anchors.right: rightPanel.left
        anchors.bottomMargin: 16
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        visible: analysisPanelIndex !== 2
    }
    // 全局唯一的四视图实例
    FourViewPanel {
        id: fourViewPanel
        parent: analysisPanelIndex === 2 ? brainpanel.fourViewContainer : mainFourViewContainer
        anchors.fill: parent
    }
    BrainPanel{
        id: brainpanel
        visible: analysisPanelIndex === 2
        anchors.left: leftToolbar.right
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        anchors.top: topExpandBar.bottom
        anchors.bottom: parent.bottom
        fourViewPanel: fourViewPanel
        messageManager: dialogMessageBox
        rightPanelExpanded: topExpandBar.brainRightExpanded
        dataStorePanel: dataStorePanel
    }
    DropShadow {
        id:aiPanelShaow
        anchors.fill: aiPanel
        source: aiPanel
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        color: "#66072662"
        samples: 32
        visible: win.showAIPanel
    }
    Rectangle{
        id: aiPanel
        visible: win.showAIPanel
        height: parent.height * 0.86
        width: 450
        color: "#111217"
        border.width: 1
        border.color: "#20252D"
        radius: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 20
        Image {
            anchors.fill: parent
            source: "qrc:/image/aiPanelBackground.png"
        }
        Column{
            anchors.fill: parent
            padding: 20
            spacing: 20
            Rectangle{
                id: aiTitle
                width: parent.width - 40
                height: 28
                color: "transparent"
                Row{
                    height: parent.height
                    anchors.left: parent.left
                    spacing: 8
                    Image{
                        source: "qrc:/image/aiStar.png"
                    }
                    Label{
                        text: qsTr("AI助手")
                        color: "#FFFFFF"
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Image{
                    source: "qrc:/image/close.png"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: {
                            parent.opacity = 0.8
                            parent.scale = 1.05
                        }

                        onExited: {
                            parent.opacity = 1.0
                            parent.scale = 1.0
                        }

                        onPressed: {
                            parent.scale = 0.95
                        }

                        onReleased: {
                            parent.scale = containsMouse ? 1.05 : 1.0
                        }
                        onClicked: {
                            win.showAIPanel = false
                        }
                    }
                }
            }
            Rectangle {
                id: messagesArea
                width: parent.width - 40
                height: parent.height - aiTitle.height - sendArea.height - 40 - 40
                color: "transparent"
                ScrollView {
                    id: scrollView
                    anchors.fill: parent
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    Column {
                        id: messagesColumn
                        width: scrollView.width
                        spacing: 20
                        // 使用Repeater显示消息
                        Repeater {
                            id: messagesRepeater
                            model: $chatManager.messages
                            // 消息气泡
                            delegate: Column {
                                id: messageItem
                                width: messageBubble.width
                                anchors.right: modelData.type === "user" ? parent.right : undefined
                                anchors.rightMargin: modelData.type === "user" ? 24 : 0
                                anchors.left: (modelData.type === "ai" || modelData.type === "thinking" || modelData.type === "interrupt") ? parent.left : undefined
                                anchors.leftMargin: (modelData.type === "ai" || modelData.type === "thinking" || modelData.type === "interrupt") ? 24 : 0
                                spacing: 6

                                // 消息气泡
                                Rectangle {
                                    id: messageBubble
                                    width: (modelData.type !== "thinking" && modelData.type !== "interrupt") ? messageContent.width : thinkingRow.width
                                    height: (modelData.type !== "thinking" && modelData.type !== "interrupt") ? messageContent.height : thinkingRow.height
                                    color: modelData.type === "user" ? "#333C7EFF" : "#0FFFFFFF"
                                    radius: 12

                                    // 普通消息内容
                                    Text {
                                        id: messageContent
                                        anchors.centerIn: parent
                                        width: Math.min(implicitWidth, messagesColumn.width - 48)
                                        text: {
                                            if (modelData.type === "thinking" || modelData.type === "interrupt") {
                                                return ""
                                            }
                                            // 将字面量的\n转换为实际的换行符
                                            var content = modelData.content || ""
                                            return content.replace(/\\n/g, "\n")
                                        }
                                        padding: 12
                                        font.family: "Alibaba PuHuiTi 3.0"
                                        font.pixelSize: 16
                                        color: "#FFFFFF"
                                        wrapMode: Text.Wrap
                                        textFormat: Text.MarkdownText
                                        visible: (modelData.type !== "thinking" && modelData.type !== "interrupt")
                                    }

                                    // 思考中动画
                                    Row {
                                        id: thinkingRow
                                        anchors.centerIn: parent
                                        spacing: 2
                                        visible: modelData.type === "thinking" || modelData.type === "interrupt"

                                        Text {
                                            text: qsTr(modelData.content)
                                            font.weight: Font.Bold
                                            font.family: "Alibaba PuHuiTi 3.0"
                                            font.pixelSize: 16
                                            color: "#FFFFFF"
                                        }

                                        Text {
                                            id: dots
                                            text: "."
                                            font.weight: Font.Bold
                                            visible: modelData.type === "thinking"
                                            font.family: "Alibaba PuHuiTi 3.0"
                                            font.pixelSize: 16
                                            color: "#FFFFFF"

                                            Timer {
                                                id: dotsTimer
                                                interval: 500
                                                running: modelData.type === "thinking"
                                                repeat: true
                                                property int dotCount: 1

                                                onTriggered: {
                                                    dotCount = (dotCount % 3) + 1
                                                    dots.text = ".".repeat(dotCount)
                                                }
                                            }
                                        }
                                    }
                                }

                                // AI消息的操作按钮（只在最后一条AI消息显示）
                                Row {
                                    id: actionButtons
                                    spacing: 4
                                    anchors.right: parent.right
                                    visible: modelData.type === "ai" && index === ($chatManager.messages.length - 1) && !$chatManager.isSending && index !== 0
                                    Image {
                                        source: "qrc:/image/copyBtn.png"
                                        MouseArea {
                                            id: copyBtnArea
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                // 将字面量的\n转换为实际的换行符后再复制
                                                var content = modelData.content || ""
                                                var formattedContent = content.replace(/\\n/g, "\n")
                                                $chatManager.copyToClipboard(formattedContent)
                                                dialogMessageBox.success("已复制！")
                                            }
                                            onEntered: parent.scale = 1.1
                                            onExited: parent.scale = 1.0
                                            onPressed: parent.scale = 0.9
                                            onReleased: parent.scale = 1
                                            hoverEnabled: true
                                        }
                                    }
                                    Image {
                                        source: "qrc:/image/resetBtn.png"
                                        MouseArea {
                                            id: regenerateBtnArea
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: $chatManager.regenerateLastResponse()
                                            hoverEnabled: true
                                            onEntered: parent.scale = 1.1
                                            onExited: parent.scale = 1.0
                                            onPressed: parent.scale = 0.9
                                            onReleased: parent.scale = 1
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Rectangle{
                id: sendArea
                color: "#33FFFFFF"
                border.width: 1
                border.color: "#FFFFFF"
                height: 56
                radius: 60
                width: parent.width - 40
                Image {
                    id: linkBtn
                    source: "qrc:/image/linkBtn.png"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: {
                            parent.opacity = 0.8
                            parent.scale = 1.05
                        }

                        onExited: {
                            parent.opacity = 1.0
                            parent.scale = 1.0
                        }

                        onPressed: {
                            parent.scale = 0.95
                        }

                        onReleased: {
                            parent.scale = containsMouse ? 1.05 : 1.0
                        }
                        onClicked: {

                        }
                    }
                }
                SingleLineTextInput{
                    id: messageInput
                    anchors.left: linkBtn.right
                    anchors.leftMargin: 8
                    backgroundColor: "transparent"
                    textColor: "#E5FFFFFF"
                    anchors.right: sendBtn.left
                    anchors.rightMargin: 25
                    inputHeight: 28
                    borderWidth: 0
                    anchors.verticalCenter: parent.verticalCenter
                    Keys.onPressed: {
                        var message = messageInput.text.trim()
                        if (message.length > 0 && !$chatManager.isSending) {
                            $chatManager.sendMessage(message)
                            messageInput.text = ""
                        }
                    }
                }
                Image{
                    id: sendBtn
                    source: "qrc:/image/sendBtn.png"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    MouseArea{
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: {
                            parent.opacity = 0.8
                            parent.scale = 1.05
                        }

                        onExited: {
                            parent.opacity = 1.0
                            parent.scale = 1.0
                        }

                        onPressed: {
                            parent.scale = 0.95
                        }

                        onReleased: {
                            parent.scale = containsMouse ? 1.05 : 1.0
                        }
                        onClicked: {
                            var message = messageInput.text.trim()
                            if (message.length > 0 && !$chatManager.isSending) {
                                $chatManager.sendMessage(message)
                                messageInput.text = ""
                            }
                        }
                    }
                }
            }
        }
    }

    // 标注输入框（全局，用于所有视图）
    AnnotationInputBox {
        id: annotationInputBox
        parent: win.contentItem
        
        onTextConfirmed: {
            // 根据标注类型调用不同的更新方法
            if (annotationType === 0) {
                // 矩形标注
                $MainViewController.updateAnnotationText(orientation, annotationIndex, text)
            } else if (annotationType === 1) {
                // 圆形标注
                $MainViewController.updateCircleAnnotationText(orientation, annotationIndex, text)
            } else if (annotationType === 2) {
                // 画笔标注
                $MainViewController.updatePenAnnotationText(orientation, annotationIndex, text)
            }
        }
        
        onCancelled: {
            // 根据标注类型调用不同的删除方法
            if (annotationType === 0) {
                // 矩形标注
                $MainViewController.deleteAnnotation(orientation, annotationIndex)
            } else if (annotationType === 1) {
                // 圆形标注
                $MainViewController.deleteCircleAnnotation(orientation, annotationIndex)
            } else if (annotationType === 2) {
                // 画笔标注
                $MainViewController.deletePenAnnotation(orientation, annotationIndex)
            }
        }
    }

    // Python 控制台 (Ctrl+P 切换)
    PythonConsole {
        id: pythonConsole
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        width: Math.min(parent.width * 0.7, 900)
        height: Math.min(parent.height * 0.45, 500)
        z: 9999
    }

    Shortcut {
        sequence: "Ctrl+P"
        context: Qt.ApplicationShortcut
        onActivated: pythonConsole.toggle()
    }

    // 连接MainViewController的标注创建信号
    Connections {
        target: $MainViewController
        
        function onAnnotationCreated(screenX, screenY, annotationIndex, orientation, annotationType) {
            // 根据orientation选择对应的视图容器
            var targetContainer = null
            if (orientation === 0) {
                // Axial - 轴向视图
                targetContainer = fourViewPanel.axialViewContainer
            } else if (orientation === 1) {
                // Sagittal - 矢状视图
                targetContainer = fourViewPanel.sagittalViewContainer
            } else if (orientation === 2) {
                // Coronal - 冠状视图
                targetContainer = fourViewPanel.coronalViewContainer
            }
            
            // 显示输入框，传入targetContainer让show方法内部设置parent
            if (targetContainer) {
                annotationInputBox.show(screenX, screenY, annotationIndex, orientation, annotationType, targetContainer)
            }
        }
    }
}
