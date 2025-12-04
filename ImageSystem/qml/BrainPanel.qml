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
    FileDialog {
        id: fileDialog
        title: qsTr("选择要上传的文件")
        selectFolder: true
        onAccepted: {
            $MainViewController.importBrainData(fileDialog.fileUrls[0])
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
            height: rootPanel.height - 10 - 60
            width: parent.width
            visible: currentIndex === 1
        }
        Rectangle {
            height: rootPanel.height - 10 - 60
            width: parent.width
            visible: currentIndex === 2
        }
        Rectangle {
            height: rootPanel.height - 10 - 60
            width: parent.width
            visible: currentIndex === 3
            color: "transparent"
            Rectangle{
                id: networkMain
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: networkAnalysis.left
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
                id: networkAnalysis
                width: 340
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: "transparent"
                Column {
                    padding: 10
                    width: parent.width
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
                                fileDialog.open()
                            }
                        }
                    }
                    Rectangle{
                        width: parent.width - 10
                        height: infoList.height
                        color: "#000000"
                        Column{
                            id:infoList
                            width: parent.width
                            spacing: 5
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
                    ///表格
                }
            }
        }
        Rectangle {
            height: rootPanel.height - 10 - 60
            width: parent.width
            visible: currentIndex === 4
        }
        Rectangle {
            height: rootPanel.height - 10 - 60
            width: parent.width
            visible: currentIndex === 5
        }
    }
}
