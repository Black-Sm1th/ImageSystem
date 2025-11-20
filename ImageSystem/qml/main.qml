import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.5
import com.vtk.dicom 1.0

Window {
    id: win
    visible: true
    width: 1600
    height: 1000
    title: qsTr("DICOM 医学影像查看器 - 专业版")
    color: "#2b2b2b"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部工具栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#1e1e1e"
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 15

                // 文件选择按钮
                Button {
                    text: "打开 DICOM 文件夹"
                    Layout.preferredHeight: 40
                    Layout.preferredWidth: 160

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
                        var folder = dicomManager.openFolderDialog()
                        if (folder) {
                            dicomManager.loadDicomDirectory(folder)
                        }
                    }
                }

                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    color: "#404040"
                }

                // DICOM信息显示
                Text {
                    id: infoText
                    text: dicomManager.hasData ? dicomManager.dicomInfo : "未加载 DICOM 数据"
                    color: "#cccccc"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                // 预设窗宽窗位按钮
                Button {
                    text: "肺窗"
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 70
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
                    text: "骨窗"
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 70
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
                    text: "脑窗"
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 70
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
                    text: "腹窗"
                    Layout.preferredHeight: 35
                    Layout.preferredWidth: 70
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
        }

        // 主显示区域
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 左侧：四视图显示
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 5
                    rows: 2
                    columns: 2
                    rowSpacing: 5
                    columnSpacing: 5

                    // 左上：轴向视图
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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

            // 右侧：控制面板
            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: "#1e1e1e"

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    ColumnLayout {
                        width: parent.width - 20
                        spacing: 20

                        // 切片控制区域
                        GroupBox {
                            Layout.fillWidth: true
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

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 15

                                // 轴向切片
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Label {
                                        text: "轴向切片: " + dicomManager.axialSlice + " / " + dicomManager.maxAxialSlice
                                        color: "#ffff00"
                                        font.pixelSize: 12
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: dicomManager.maxAxialSlice
                                        value: dicomManager.axialSlice
                                        stepSize: 1
                                        enabled: dicomManager.hasData
                                        
                                        onValueChanged: {
                                            if (pressed) {
                                                dicomManager.axialSlice = value
                                            }
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
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Label {
                                        text: "矢状切片: " + dicomManager.sagittalSlice + " / " + dicomManager.maxSagittalSlice
                                        color: "#ffff00"
                                        font.pixelSize: 12
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: dicomManager.maxSagittalSlice
                                        value: dicomManager.sagittalSlice
                                        stepSize: 1
                                        enabled: dicomManager.hasData
                                        
                                        onValueChanged: {
                                            if (pressed) {
                                                dicomManager.sagittalSlice = value
                                            }
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
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Label {
                                        text: "冠状切片: " + dicomManager.coronalSlice + " / " + dicomManager.maxCoronalSlice
                                        color: "#ffff00"
                                        font.pixelSize: 12
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: dicomManager.maxCoronalSlice
                                        value: dicomManager.coronalSlice
                                        stepSize: 1
                                        enabled: dicomManager.hasData
                                        
                                        onValueChanged: {
                                            if (pressed) {
                                                dicomManager.coronalSlice = value
                                            }
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
                            Layout.fillWidth: true
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

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 15

                                // 窗宽
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Label {
                                        text: "窗宽 (Width): " + Math.round(dicomManager.windowWidth)
                                        color: "#00ff00"
                                        font.pixelSize: 12
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 1
                                        to: 4000
                                        value: dicomManager.windowWidth
                                        stepSize: 10
                                        enabled: dicomManager.hasData
                                        
                                        onValueChanged: {
                                            if (pressed) {
                                                dicomManager.windowWidth = value
                                            }
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
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Label {
                                        text: "窗位 (Level): " + Math.round(dicomManager.windowLevel)
                                        color: "#ff6600"
                                        font.pixelSize: 12
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: -1024
                                        to: 3071
                                        value: dicomManager.windowLevel
                                        stepSize: 10
                                        enabled: dicomManager.hasData
                                        
                                        onValueChanged: {
                                            if (pressed) {
                                                dicomManager.windowLevel = value
                                            }
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
                            Layout.fillWidth: true
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
        }

        // 底部状态栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#1e1e1e"
            
            Text {
                anchors.centerIn: parent
                text: "DICOM 医学影像查看器 v1.0 | 支持三视图 + 3D体渲染"
                color: "#888888"
                font.pixelSize: 11
            }
        }
    }
}

