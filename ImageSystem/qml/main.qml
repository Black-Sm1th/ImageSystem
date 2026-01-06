import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import com.vtk.dicom 1.0
import "./components"
ApplicationWindow {
    id: win
    visible: true
    width: 1920
    height: 1080
    title: qsTr("DICOM 医学影像查看器")
    color: "#000000"
    property int analysisPanelIndex: 0
    font.family: "Alibaba PuHuiTi 3.0"
    font.pixelSize: 14
    // 对话框消息组件
    MessageBox {
        id: dialogMessageBox
        anchors.fill: parent
    }
    // 顶部功能栏
    Rectangle {
        id: topToolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 82
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
                        fileDialog.open()
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
                font.pixelSize: 12
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
                            color: "#E5FFFFFF"
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
                            color: "#E5FFFFFF"
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
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/image/aiBtn.png"
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
            width: (analysisPanelIndex === 2 && (brainpanel.currentIndex === 4 || brainpanel.currentIndex === 5)) ? 0 : (analysisPanelIndex === 2 && (brainpanel.currentIndex === 2 || brainpanel.currentIndex === 3) ? 500 : 400)
            height: parent.height
            visible: brainpanel.currentIndex !== 4 && brainpanel.currentIndex !== 5 || analysisPanelIndex !== 2
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
            
            property int selectedToolIndex: -1
            
            LeftToolButton {
                id: moveToolBtn
                iconSource: "qrc:/image/toolMove.png"
                isSelected: leftToolColumn.selectedToolIndex === 0
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 0) ? -1 : 0
                    if(moveToolBtn.isSelected){
                        $DicomDataModel.setToolMode(0)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(1)
                    }
                }
            }
            LeftToolButton {
                id: vectorToolBtn
                iconSource: "qrc:/image/toolVector.png"
                isSelected: leftToolColumn.selectedToolIndex === 1
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 1) ? -1 : 1
                    if(vectorToolBtn.isSelected){
                        $DicomDataModel.setToolMode(3)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(1)
                    }
                }
            }
            LeftToolButton {
                id: largeToolBtn
                iconSource: "qrc:/image/toolLarge.png"
                isSelected: leftToolColumn.selectedToolIndex === 2
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 2) ? -1 : 2
                    if(largeToolBtn.isSelected){
                        $DicomDataModel.setToolMode(4)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(1)
                    }
                }
            }
            LeftToolButton {
                id: measureDistanceToolBtn
                iconSource: "qrc:/image/toolDistance.png"
                isSelected: leftToolColumn.selectedToolIndex === 3
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 3) ? -1 : 3
                    if(measureDistanceToolBtn.isSelected){
                        $DicomDataModel.setToolMode(5)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(1)
                    }
                }
            }
            LeftToolButton {
                id: measureAngleToolBtn
                iconSource: "qrc:/image/toolAngle.png"
                isSelected: leftToolColumn.selectedToolIndex === 4
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 4) ? -1 : 4
                    if(measureAngleToolBtn.isSelected){
                        $DicomDataModel.setToolMode(6)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }else{
                        $DicomDataModel.setToolMode(1)
                    }
                }
            }
            LeftToolButton {
                id: lineToolBtn
                iconSource: "qrc:/image/toolLine.png"
                isSelected: leftToolColumn.selectedToolIndex === 5
                onClicked: {
                    leftToolColumn.selectedToolIndex = (leftToolColumn.selectedToolIndex === 5) ? -1 : 5
                    if(lineToolBtn.isSelected){
                        $DicomDataModel.setToolMode(1)
                        $DicomDataModel.setCrosshairEnabled(true)
                    }else{
                        $DicomDataModel.setToolMode(1)
                        $DicomDataModel.setCrosshairEnabled(false)
                    }
                }
            }
            LeftToolButton {
                id: resetToolBtn
                iconSource: "qrc:/image/toolReset.png"
                onClicked: {
                    leftToolColumn.selectedToolIndex = -1
                    $DicomDataModel.resetAllInteractions()
                }
            }
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
    }
}
