import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import com.vtk.dicom 1.0
import "./components"
ApplicationWindow {
    id: win
    visible: true
    width: 1600
    height: 1000
    title: qsTr("DICOM 医学影像查看器")
    color: "#18191C"
    property int analysisPanelIndex: 0
    font.family: "Alibaba PuHuiTi 3.0"
    font.pixelSize: 14
    // 顶部工具栏
    Rectangle {
        id: topToolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "transparent"
        
        // 文件选择按钮
        CustomButton {
            id: openButton
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 10
            width: 200
            height: 40
            text: "打开 DICOM/NIfTI 数据"
            backgroundColor: "#004578"

            onClicked: {
                openMenu.open()
            }
        }
        Menu {
            id: openMenu
            y: openButton.height
            MenuItem {
                text: "选择 DICOM 文件夹"
                onTriggered: {
                    fileDialog.requestType = "dicom"
                    fileDialog.title = qsTr("选择 DICOM 文件夹")
                    fileDialog.selectFolder = true
                    fileDialog.nameFilters = []
                    fileDialog.open()
                }
            }
            MenuItem {
                text: "选择 NIfTI 文件 (.nii/.nii.gz)"
                onTriggered: {
                    fileDialog.requestType = "niftiPrimary"
                    fileDialog.title = qsTr("选择 NIfTI 文件")
                    fileDialog.selectFolder = false
                    fileDialog.nameFilters = ["NIfTI Files (*.nii *.nii.gz)", qsTr("All Files (*)")]
                    fileDialog.open()
                }
            }
        }
        FileDialog {
            id: fileDialog
            property string requestType: ""
            selectExisting: true
            selectMultiple: false
            onAccepted: {
                var target = fileDialog.fileUrls[0]
                if (fileDialog.requestType === "dicom" || fileDialog.requestType === "niftiPrimary") {
                    $DicomDataModel.loadDicomDirectory(target)
                }
                fileDialog.requestType = ""
            }
        }
        Rectangle {
            anchors.left: openButton.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            width: 1
            height: parent.height - 20
            color: "#404040"
        }

        // DICOM信息显示
        Label {
            id: infoText
            anchors.left: openButton.right
            anchors.right: lungWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            anchors.rightMargin: 15
            text: $DicomDataModel.hasData ? $DicomDataModel.dicomInfo : "未加载 DICOM 数据"
            color: "#cccccc"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        // 预设窗宽窗位按钮
        CustomButton {
            id: kidneyButton
            anchors.right: brainButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "肾功能"
            backgroundColor: analysisPanelIndex === 1 ? "green" : "#383838"
            onClicked: {
                if(analysisPanelIndex === 1){
                    analysisPanelIndex = 0
                }else{
                    analysisPanelIndex = 1
                }
            }
        }

        // 预设窗宽窗位按钮
        CustomButton {
            id: brainButton
            anchors.right: split1.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "脑功能"
            backgroundColor: analysisPanelIndex === 2 ? "green" : "#383838"
            onClicked: {
                if(analysisPanelIndex === 2){
                    analysisPanelIndex = 0
                }else{
                    analysisPanelIndex = 2
                }
            }
        }

        Rectangle {
            id: split1
            anchors.right: lungWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 1
            height: parent.height - 20
            color: "#404040"
        }

        // 预设窗宽窗位按钮
        CustomButton {
            id: lungWindowButton
            anchors.right: boneWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "肺窗"
            backgroundColor: "#383838"
            onClicked: {
                $DicomDataModel.windowWidth = 1500
                $DicomDataModel.windowLevel = -600
            }
        }

        CustomButton {
            id: boneWindowButton
            anchors.right: brainWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "骨窗"
            backgroundColor: "#383838"
            onClicked: {
                $DicomDataModel.windowWidth = 2000
                $DicomDataModel.windowLevel = 300
            }
        }

        CustomButton {
            id: brainWindowButton
            anchors.right: abdomenWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "脑窗"
            backgroundColor: "#383838"
            onClicked: {
                $DicomDataModel.windowWidth = 80
                $DicomDataModel.windowLevel = 40
            }
        }

        CustomButton {
            id: abdomenWindowButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "腹窗"
            backgroundColor: "#383838"
            onClicked: {
                $DicomDataModel.windowWidth = 400
                $DicomDataModel.windowLevel = 40
            }
        }
    }

    // 底部状态栏
    Rectangle {
        id: bottomStatusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: "transparent"
        
        Label {
            anchors.centerIn: parent
            text: "DICOM 医学影像查看器 v1.0"
            color: "#888888"
        }
    }

    // 右侧控制面板
    Rectangle {
        id: rightPanel
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
        anchors.right: parent.right
        width: 280
        visible: analysisPanelIndex !== 2
        color: "transparent"

        ScrollView {
            anchors.fill: parent
            anchors.margins: 10
            clip: true

            Column {
                width: rightPanel.width - 20
                spacing: 20

                // 切片控制区域
                GroupBox {
                    width: parent.width
                    title: "切片控制"
                    
                    background: Rectangle {
                        color: "#252525"
                        border.color: "#404040"
                        radius: 5
                    }
                    
                    label: Label {
                        text: parent.title
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 14
                    }

                    Column {
                        width: parent.width
                        spacing: 15

                        // 轴向切片
                        Column {
                            width: parent.width
                            spacing: 5

                            Label {
                                text: "轴向切片: " + $DicomDataModel.axialSlice + " / " + $DicomDataModel.maxAxialSlice
                                color: "#ffff00"
                                font.pixelSize: 12
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
                                    radius: 2
                                    color: "#404040"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        color: "#0078d4"
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: parent.pressed ? "#005a9e" : "#0078d4"
                                    border.color: "#ffffff"
                                    border.width: 2
                                }
                            }
                        }

                        // 矢状切片
                        Column {
                            width: parent.width
                            spacing: 5

                            Label {
                                text: "矢状切片: " + $DicomDataModel.sagittalSlice + " / " + $DicomDataModel.maxSagittalSlice
                                color: "#ffff00"
                                font.pixelSize: 12
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
                                    radius: 2
                                    color: "#404040"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        color: "#0078d4"
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: parent.pressed ? "#005a9e" : "#0078d4"
                                    border.color: "#ffffff"
                                    border.width: 2
                                }
                            }
                        }

                        // 冠状切片
                        Column {
                            width: parent.width
                            spacing: 5

                            Label {
                                text: "冠状切片: " + $DicomDataModel.coronalSlice + " / " + $DicomDataModel.maxCoronalSlice
                                color: "#ffff00"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: coronalSlider
                                width: parent.width
                                from: 0
                                to: $DicomDataModel.maxCoronalSlice
                                stepSize: 1
                                enabled: $DicomDataModel.hasData
                                
                                value: $DicomDataModel.coronalSlice
                                
                                onMoved: {
                                    $DicomDataModel.coronalSlice = value
                                }
                                
                                background: Rectangle {
                                    x: parent.leftPadding
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: parent.availableWidth
                                    height: 4
                                    radius: 2
                                    color: "#404040"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        color: "#0078d4"
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: parent.pressed ? "#005a9e" : "#0078d4"
                                    border.color: "#ffffff"
                                    border.width: 2
                                }
                            }
                        }
                    }
                }

                // 窗宽窗位控制
                GroupBox {
                    width: parent.width
                    title: "窗宽窗位"
                    
                    background: Rectangle {
                        color: "#252525"
                        border.color: "#404040"
                        radius: 5
                    }
                    
                    label: Label {
                        text: parent.title
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 14
                    }

                    Column {
                        width: parent.width
                        spacing: 15

                        // 窗宽
                        Column {
                            width: parent.width
                            spacing: 5

                            Label {
                                text: "窗宽 (Width): " + Math.round($DicomDataModel.windowWidth)
                                color: "#00ff00"
                                font.pixelSize: 12
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
                                    radius: 2
                                    color: "#404040"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        color: "#00ff00"
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: parent.pressed ? "#00aa00" : "#00ff00"
                                    border.color: "#ffffff"
                                    border.width: 2
                                }
                            }
                        }

                        // 窗位
                        Column {
                            width: parent.width
                            spacing: 5

                            Label {
                                text: "窗位 (Level): " + Math.round($DicomDataModel.windowLevel)
                                color: "#ff6600"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: windowLevelSlider
                                width: parent.width
                                from: -1024
                                to: 3071
                                stepSize: 10
                                enabled: $DicomDataModel.hasData
                                
                                Component.onCompleted: {
                                    value = $DicomDataModel.windowLevel
                                }
                                
                                Connections {
                                    target: $DicomDataModel
                                    function onWindowLevelChanged(level) {
                                        if (!windowLevelSlider.pressed) {
                                            windowLevelSlider.value = level
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
                                    radius: 2
                                    color: "#404040"

                                    Rectangle {
                                        width: parent.parent.visualPosition * parent.width
                                        height: parent.height
                                        color: "#ff6600"
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16
                                    height: 16
                                    radius: 8
                                    color: parent.pressed ? "#cc5200" : "#ff6600"
                                    border.color: "#ffffff"
                                    border.width: 2
                                }
                            }
                        }
                    }
                }

                // 使用说明
                GroupBox {
                    width: parent.width
                    title: "使用说明"
                    
                    background: Rectangle {
                        color: "#252525"
                        border.color: "#404040"
                        radius: 5
                    }
                    
                    label: Label {
                        text: parent.title
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 14
                    }

                    Label {
                        width: parent.width
                        text: "• 三视图左键拖动：调整窗宽窗位\n" +
                              "  (横向-窗宽, 纵向-窗位)\n" +
                              "• 鼠标滚轮：切片浏览\n" +
                              "• 3D视图：旋转查看\n" +
                              "• 预设窗口：快速切换\n" +
                              "• 右下角实时显示窗宽窗位"
                        color: "#aaaaaa"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.5
                    }
                }
            }
        }
    }

    //左侧功能区域
    Rectangle {
        id: leftAnalysisPanel
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
        anchors.left: parent.left
        width: 300
        color: "transparent"
        visible: analysisPanelIndex !== 0 && analysisPanelIndex !== 2
        KidneyPanel {
            id: kidneypanel
            visible: analysisPanelIndex === 1
        }
    }

    BrainPanel{
        id: brainpanel
        visible: analysisPanelIndex === 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
    }

    // 左侧：四视图显示区域
    Item {
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
        anchors.left: analysisPanelIndex !== 0 ? leftAnalysisPanel.right : parent.left
        anchors.right: rightPanel.left
        visible: analysisPanelIndex !== 2
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
}
