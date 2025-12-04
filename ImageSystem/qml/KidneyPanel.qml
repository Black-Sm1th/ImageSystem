import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.3
import "./components"
Rectangle {
    id: rootPanel
    anchors.fill: parent
    color: "transparent"
    property int colWidth: 80
    Column{
        width: parent.width
        padding: 10
        spacing: 10
        Label {
            id: buttonText
            text: qsTr("特征输入")
            color: "#ffffff"
            font.pixelSize: 16
        }
        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("T2信号：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: t2ComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["低信号", "中信号", "高信号"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.t2 = selectedIndices[0]
                    }
                }
            }
        }

        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("皮髓质期：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: skinComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["轻度强化", "中度强化", "明显强化"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.skin = selectedIndices[0]
                    }
                }
            }
        }

        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("微观脂肪：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: microComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.micro = selectedIndices[0]
                    }
                }
            }
        }

        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("SEI：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: seiComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.sei = selectedIndices[0]
                    }
                }
            }
        }

        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("ADER≥1.5：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: aderComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.ader = selectedIndices[0]
                    }
                }
            }
        }

        Row {
            height: 40
            spacing: 8
            Label {
                text: qsTr("弥散受限：")
                font.pixelSize: 14
                color: "#ffffff"
                width: colWidth
                anchors.verticalCenter: parent.verticalCenter
            }
            CustomComboBox {
                id: dispComboBox
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
                onSelectionChanged: function(selectedIndices, selectedItems) {
                    if (selectedIndices.length > 0) {
                        $MainViewController.disp = selectedIndices[0]
                    }
                }
            }
        }
        CustomButton {
            id: calculateButton
            buttonWidth: parent.width - 20
            text: "计算"
            backgroundColor: "#004578"
            onClicked: {
                $MainViewController.calculateKidney()
            }
        }
        Label {
            text: qsTr("CCLS：") + $MainViewController.cclsResult.toFixed(2)
            font.pixelSize: 14
            color: "#00ff00"
            font.bold: true
        }
        Label {
            text: qsTr("CCRCC：") + $MainViewController.ccrccResult.toFixed(4)
            font.pixelSize: 14
            color: "#00ff00"
            font.bold: true
        }
    }
}
