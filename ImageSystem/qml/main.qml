import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQml 2.15
import QtQuick.Dialogs 1.3
import com.vtk.dicom 1.0

Window {
    id: win
    visible: true
    width: 1600
    height: 1000
    title: qsTr("DICOM 医学影像查看器")
    color: "#2b2b2b"

    // 顶部工具栏
    Rectangle {
        id: topToolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#1e1e1e"
        
        // 文件选择按钮
        Button {
            id: openButton
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 10
            width: 160
            height: 40
            text: "打开 DICOM 文件夹"

            background: Rectangle {
                color: parent.pressed ? "#0078d4" : (parent.hovered ? "#005a9e" : "#004578")
                radius: 4
            }

            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 13
                font.bold: true
            }

            onClicked: {
                fileDialog.open()
            }
        }
        FileDialog {
            id: fileDialog
            title: qsTr("选择要上传的文件")
            selectFolder: true
            onAccepted: {
                dicomManager.loadDicomDirectory(fileDialog.fileUrls[0])
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
        Text {
            id: infoText
            anchors.left: openButton.right
            anchors.right: lungWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            anchors.rightMargin: 15
            text: dicomManager.hasData ? dicomManager.dicomInfo : "未加载 DICOM 数据"
            color: "#cccccc"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        // 预设窗宽窗位按钮
        Button {
            id: lungWindowButton
            anchors.right: boneWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "肺窗"
            
            background: Rectangle {
                color: parent.pressed ? "#3d3d3d" : (parent.hovered ? "#505050" : "#383838")
                radius: 3
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 11
            }
            onClicked: {
                dicomManager.windowWidth = 1500
                dicomManager.windowLevel = -600
            }
        }

        Button {
            id: boneWindowButton
            anchors.right: brainWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "骨窗"
            
            background: Rectangle {
                color: parent.pressed ? "#3d3d3d" : (parent.hovered ? "#505050" : "#383838")
                radius: 3
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 11
            }
            onClicked: {
                dicomManager.windowWidth = 2000
                dicomManager.windowLevel = 300
            }
        }

        Button {
            id: brainWindowButton
            anchors.right: abdomenWindowButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "脑窗"
            
            background: Rectangle {
                color: parent.pressed ? "#3d3d3d" : (parent.hovered ? "#505050" : "#383838")
                radius: 3
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 11
            }
            onClicked: {
                dicomManager.windowWidth = 80
                dicomManager.windowLevel = 40
            }
        }

        Button {
            id: abdomenWindowButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 10
            width: 70
            height: 35
            text: "腹窗"
            
            background: Rectangle {
                color: parent.pressed ? "#3d3d3d" : (parent.hovered ? "#505050" : "#383838")
                radius: 3
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 11
            }
            onClicked: {
                dicomManager.windowWidth = 400
                dicomManager.windowLevel = 40
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
        color: "#1e1e1e"
        
        Text {
            anchors.centerIn: parent
            text: "DICOM 医学影像查看器 v1.0"
            color: "#888888"
            font.pixelSize: 11
        }
    }

    // 右侧控制面板
    Rectangle {
        id: rightPanel
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
        anchors.right: parent.right
        width: 280
        color: "#1e1e1e"

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
                                text: "轴向切片: " + dicomManager.axialSlice + " / " + dicomManager.maxAxialSlice
                                color: "#ffff00"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: axialSlider
                                width: parent.width
                                from: 0
                                to: dicomManager.maxAxialSlice
                                stepSize: 1
                                enabled: dicomManager.hasData
                                
                                Component.onCompleted: {
                                    value = dicomManager.axialSlice
                                }
                                
                                Connections {
                                    target: dicomManager
                                    function onAxialSliceChanged(slice) {
                                        if (!axialSlider.pressed) {
                                            axialSlider.value = slice
                                        }
                                    }
                                }
                                
                                onMoved: {
                                    dicomManager.axialSlice = value
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
                                text: "矢状切片: " + dicomManager.sagittalSlice + " / " + dicomManager.maxSagittalSlice
                                color: "#ffff00"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: sagittalSlider
                                width: parent.width
                                from: 0
                                to: dicomManager.maxSagittalSlice
                                stepSize: 1
                                enabled: dicomManager.hasData
                                
                                Component.onCompleted: {
                                    value = dicomManager.sagittalSlice
                                }
                                
                                Connections {
                                    target: dicomManager
                                    function onSagittalSliceChanged(slice) {
                                        if (!sagittalSlider.pressed) {
                                            sagittalSlider.value = slice
                                        }
                                    }
                                }
                                
                                onMoved: {
                                    dicomManager.sagittalSlice = value
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
                                text: "冠状切片: " + dicomManager.coronalSlice + " / " + dicomManager.maxCoronalSlice
                                color: "#ffff00"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: coronalSlider
                                width: parent.width
                                from: 0
                                to: dicomManager.maxCoronalSlice
                                stepSize: 1
                                enabled: dicomManager.hasData
                                
                                value: dicomManager.coronalSlice
                                
                                onMoved: {
                                    dicomManager.coronalSlice = value
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
                                text: "窗宽 (Width): " + Math.round(dicomManager.windowWidth)
                                color: "#00ff00"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: windowWidthSlider
                                width: parent.width
                                from: 1
                                to: 4000
                                stepSize: 10
                                enabled: dicomManager.hasData
                                
                                Component.onCompleted: {
                                    value = dicomManager.windowWidth
                                }
                                
                                Connections {
                                    target: dicomManager
                                    function onWindowWidthChanged(width) {
                                        if (!windowWidthSlider.pressed) {
                                            windowWidthSlider.value = width
                                        }
                                    }
                                }
                                
                                onMoved: {
                                    dicomManager.windowWidth = value
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
                                text: "窗位 (Level): " + Math.round(dicomManager.windowLevel)
                                color: "#ff6600"
                                font.pixelSize: 12
                            }

                            Slider {
                                id: windowLevelSlider
                                width: parent.width
                                from: -1024
                                to: 3071
                                stepSize: 10
                                enabled: dicomManager.hasData
                                
                                Component.onCompleted: {
                                    value = dicomManager.windowLevel
                                }
                                
                                Connections {
                                    target: dicomManager
                                    function onWindowLevelChanged(level) {
                                        if (!windowLevelSlider.pressed) {
                                            windowLevelSlider.value = level
                                        }
                                    }
                                }
                                
                                onMoved: {
                                    dicomManager.windowLevel = value
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

                    Text {
                        width: parent.width
                        text: "• 鼠标左键：平移图像\n" +
                              "• 鼠标右键：缩放图像\n" +
                              "• 鼠标滚轮：切片浏览\n" +
                              "• 3D视图：旋转查看\n" +
                              "• 预设窗口：快速切换"
                        color: "#aaaaaa"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.5
                    }
                }
            }
        }
    }

    // 左侧：四视图显示区域
    Item {
        anchors.top: topToolbar.bottom
        anchors.bottom: bottomStatusBar.top
        anchors.left: parent.left
        anchors.right: rightPanel.left
        
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
