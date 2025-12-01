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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["低信号", "中信号", "高信号"]
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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["轻度强化", "中度强化", "明显强化"]
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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
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
                width: rootPanel.width - 20 - 8 - colWidth
                model: ["无", "有"]
            }
        }
        CustomButton {
            id: openButton
            buttonWidth: parent.width - 20
            text: "计算"
            backgroundColor: "#004578"
            onClicked: {

            }
        }
        Label{
            text: qsTr("CCLS：")
            font.pixelSize: 14
            color: "#ffffff"
        }
        Label{
            text: qsTr("CCRCC：")
            font.pixelSize: 14
            color: "#ffffff"
        }
    }
}
