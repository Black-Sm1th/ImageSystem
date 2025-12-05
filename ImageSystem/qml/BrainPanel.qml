import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import QtWebEngine 1.10
import "./components"

Rectangle {
    id: rootPanel
    color: "transparent"
    property int currentIndex: 1
    
    // 接收从外部传入的 FourViewPanel 实例
    property var fourViewPanel: null
    
    // 四视图容器（当在脑分割面板时使用）
    property alias fourViewContainer: brainSegmentationContainer
    
    FileDialog {
        id: fileDialog
        title: qsTr("选择要上传的文件")
        selectFolder: true
        onAccepted: {
            $MainViewController.importBrainData(fileDialog.fileUrls[0])
        }
    }

    FileDialog {
        id: segFileDialog
        title: qsTr("选择要上传的文件")
        selectFolder: true
        onAccepted: {
            $MainViewController.importBrainSegData(fileDialog.fileUrls[0])
        }
    }
    
    // 脑网络分析进度对话框
    Rectangle {
        id: analysisProgressDialog
        anchors.fill: parent
        color: "#80000000"
        visible: false
        z: 1000
        
        Rectangle {
            anchors.centerIn: parent
            width: 400
            height: 200
            color: "#2a2a2a"
            border.color: "#0078d4"
            border.width: 2
            radius: 10
            
            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15
                
                Label {
                    width: parent.width
                    text: qsTr("脑网络分析进行中...")
                    color: "#ffffff"
                    font.pixelSize: 18
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: analysisProgressDialog.visible
                    width: 60
                    height: 60
                }
                
                Label {
                    id: progressText
                    width: parent.width
                    text: qsTr("正在初始化...")
                    color: "#cccccc"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    
    // 连接信号
    Connections {
        target: $MainViewController
        
        function onBrainAnalysisStarted() {
            analysisProgressDialog.visible = true
            progressText.text = qsTr("开始脑网络分析...")
        }
        
        function onBrainAnalysisFinished(success) {
            analysisProgressDialog.visible = false
            if (success) {
                progressText.text = qsTr("分析完成！")
            }
        }

        function onNetworkTableIndexChanged(index){
            regionTableView.currentIndex = index
        }
    }
    Column{
        spacing: 10
        width: parent.width
        Rectangle {
            height: 60
            width: parent.width
            color: "transparent"
            Row{
                height: parent.height
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter
                CustomButton{
                    text: qsTr("数据预处理")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 1 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 1
                    }
                }
                CustomButton{
                    text: qsTr("脑区分割")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 2 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 2
                    }
                }
                CustomButton{
                    text: qsTr("脑网络")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 3 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 3
                    }
                }
                CustomButton{
                    text: qsTr("AI分析")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 4 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 4
                    }
                }
                CustomButton{
                    text: qsTr("生成报告")
                    height: parent.height
                    width: 150
                    backgroundColor: currentIndex === 5 ? "#619e9f" : "#000000"
                    textColor: "#ffffff"
                    onClicked: {
                        currentIndex = 5
                    }
                }
            }
        }
        Rectangle {
            height: rootPanel.height - 60 - 10
            width: parent.width
            color: "transparent"
            Item {
                id: brainSegmentationContainer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: parent.width - 380
                visible: currentIndex === 1 || currentIndex === 2 || currentIndex === 4 || currentIndex === 5
            }
            Rectangle{
                id: networkMain
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - 380
                visible: currentIndex === 3
                color: "transparent"
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.topMargin: 5
                    anchors.leftMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    Image{
                        anchors.fill: parent
                        source: $MainViewController.currentAlffUrl
                    }
                }
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 5
                    anchors.rightMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    Image{
                        anchors.fill: parent
                        source: $MainViewController.currentCovarianceUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.bottomMargin: 5
                    anchors.leftMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    Image{
                        anchors.fill: parent
                        source: $MainViewController.currentRegionplotsUrl
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: 5
                    anchors.rightMargin: 5
                    width: (parent.width - 15) / 2
                    height: (parent.height - 15) / 2
                    WebEngineView{
                        anchors.fill: parent
                        url: $MainViewController.currentViewConnectomeUrl
                    }
                }
            }
            Rectangle{
                id: preAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 1
                Column{
                    spacing: 10
                }
            }
            Rectangle{
                id: segmentationAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 2
                Row {
                    spacing: 5
                    height: 40
                    Label{
                        text: qsTr("导入数据：")
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#ffffff"
                        font.pixelSize: 16
                    }
                    // 文件选择按钮
                    CustomButton {
                        id: openButton
                        width: 160
                        height: 40
                        text: "选择预处理后数据"
                        backgroundColor: "#004578"
                        onClicked: {
                            segFileDialog.open()
                        }
                    }
                }
            }
            Rectangle{
                id: networkAnalysis
                width: 380
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                visible: currentIndex === 3
                Column {
                    padding: 10
                    width: parent.width
                    spacing: 10
                    Row {
                        spacing: 5
                        height: 40
                        Label{
                            text: qsTr("导入数据：")
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#ffffff"
                            font.pixelSize: 16
                        }
                        // 文件选择按钮
                        CustomButton {
                            width: 160
                            height: 40
                            text: "选择预处理后数据"
                            backgroundColor: "#004578"
                            onClicked: {
                                fileDialog.open()
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width - 20
                        height: infoList.height
                        color: "#000000"
                        Column{
                            id:infoList
                            width: parent.width
                            spacing: 5
                            padding: 5
                            Label{
                                text: qsTr("全局效率：" + $MainViewController.globalEfficiency)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("平均局部效率：" + $MainViewController.averageLocalEfficiency)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("平均聚类系数：" + $MainViewController.averageClusteringCoefficient)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("富俱乐部系数")
                                color: "#ffffff"
                                font.weight: Font.Bold
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("富俱乐部连接：" + $MainViewController.richClubConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("桥接连接：" + $MainViewController.bridgeConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                            Label{
                                text: qsTr("局部连接：" + $MainViewController.localConnections)
                                color: "#ffffff"
                                font.pixelSize: 16
                            }
                        }
                    }

                    // 脑区数据表格
                    Rectangle {
                        width: parent.width - 20
                        height: rootPanel.height - 500
                        color: "#1a1a1a"
                        border.color: "#404040"
                        border.width: 1

                        Column {
                            id: tableColumn
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 0

                            // 定义列宽常量
                            readonly property int colIndexWidth: 40
                            readonly property int colChineseWidth: 80
                            readonly property int colEnglishWidth: 80
                            readonly property int colDegreeWidth: 40
                            readonly property int colClusteringWidth: 45

                            // 表格标题
                            Rectangle {
                                id: tableHeader
                                width: parent.width
                                height: 35
                                color: "#2a2a2a"

                                Row {
                                    anchors.fill: parent

                                    // 序号列
                                    Rectangle {
                                        width: tableColumn.colIndexWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "#"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 中文名称
                                    Rectangle {
                                        width: tableColumn.colChineseWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "中文名称"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // Mricro命名
                                    Rectangle {
                                        width: tableColumn.colEnglishWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "Mricro命名"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 度
                                    Rectangle {
                                        width: tableColumn.colDegreeWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "度"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 聚类
                                    Rectangle {
                                        width: tableColumn.colClusteringWidth
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "聚类"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // 局部效率
                                    Rectangle {
                                        width: tableColumn.width - tableColumn.colIndexWidth - tableColumn.colChineseWidth - tableColumn.colEnglishWidth - tableColumn.colDegreeWidth - tableColumn.colClusteringWidth - 10
                                        height: parent.height
                                        color: "transparent"
                                        border.color: "#404040"
                                        border.width: 1
                                        Label {
                                            anchors.centerIn: parent
                                            text: "局部效率"
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }
                                }
                            }

                            // 表格内容
                            ListView {
                                id: regionTableView
                                width: parent.width
                                height: parent.height - 35
                                clip: true
                                model: $BrainRegionTableModel
                                currentIndex: 0

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AlwaysOn
                                    background: Rectangle {
                                        color: "#1a1a1a"
                                    }
                                    contentItem: Rectangle {
                                        implicitWidth: 8
                                        radius: 4
                                        color: "#404040"
                                    }
                                }

                                delegate: Rectangle {
                                    width: regionTableView.width - 10
                                    height: 30
                                    color: regionTableView.currentIndex === index ? "#0078d4" : (index % 2 === 0 ? "#1a1a1a" : "#252525")

                                    Row {
                                        anchors.fill: parent

                                        // 序号
                                        Rectangle {
                                            width: tableColumn.colIndexWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: (index + 1)
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                            }
                                        }

                                        // 中文名称
                                        Rectangle {
                                            width: tableColumn.colChineseWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.chineseName
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                                width: parent.width - 4
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }

                                        // Mricro命名
                                        Rectangle {
                                            width: tableColumn.colEnglishWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.englishName
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                                width: parent.width - 4
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }

                                        // 度
                                        Rectangle {
                                            width: tableColumn.colDegreeWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.degree
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                            }
                                        }

                                        // 聚类
                                        Rectangle {
                                            width: tableColumn.colClusteringWidth
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.clustering
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                            }
                                        }

                                        // 局部效率
                                        Rectangle {
                                            width: tableColumn.width - tableColumn.colIndexWidth - tableColumn.colChineseWidth - tableColumn.colEnglishWidth - tableColumn.colDegreeWidth - tableColumn.colClusteringWidth - 10
                                            height: parent.height
                                            color: "transparent"
                                            Label {
                                                anchors.centerIn: parent
                                                text: model.localEfficiency
                                                color: regionTableView.currentIndex === index ? "#ffffff" : "#cccccc"
                                                font.pixelSize: 11
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            regionTableView.currentIndex = index
                                            $MainViewController.selectBrainRegion(index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
