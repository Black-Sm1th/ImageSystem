import QtQuick 2.15
import "./"

Rectangle {
    id: root

    // 任务列表单行组件，后续新增/删除数据时只需要操作模型
    property int rowIndex: 0
    property bool checked: false
    property string serialNumber: ""
    property string patientName: ""
    property string recordNumber: ""
    property string ageText: ""
    property string genderText: ""
    property string inspectTime: ""
    property string statusText: ""
    property color statusColor: "#32D26B"
    property bool detailEnabled: true
    property bool deleteEnabled: true
    property real selectColumnWidth: 46
    property var columnWidths: [0.12, 0.12, 0.12, 0.08, 0.08, 0.14, 0.12, 0.22]

    signal rowClicked()
    signal checkClicked()
    signal detailClicked()
    signal deleteClicked()

    height: 54
    radius: 0
    color: checked ? "#2A3B67" : rowHoverArea.containsMouse ? "#1D2536" : "#171D2B"

    Behavior on color {
        ColorAnimation { duration: 120 }
    }

    function bodyWidth(rate) {
        return (width - selectColumnWidth) * rate
    }

    Row {
        anchors.fill: parent

        Rectangle {
            width: root.selectColumnWidth
            height: parent.height
            color: "transparent"

            Rectangle {
                width: 18
                height: 18
                radius: 4
                anchors.centerIn: parent
                color: root.checked ? "#3C7EFF" : "#313848"
                border.color: root.checked ? "#3C7EFF" : "#4D566A"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.checked ? "✓" : ""
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.family: "Alibaba PuHuiTi 3.0"
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.checkClicked()
                }
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[0])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.serialNumber
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[1])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.patientName
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[2])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.recordNumber
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[3])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.ageText
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[4])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.genderText
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[5])
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: root.inspectTime
                color: "#E6E6E6"
                font.pixelSize: 14
                font.family: "Alibaba PuHuiTi 3.0"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[6])
            height: parent.height

            Row {
                anchors.centerIn: parent
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: root.statusColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.statusText
                    color: root.statusColor
                    font.pixelSize: 14
                    font.family: "Alibaba PuHuiTi 3.0"
                }
            }
        }

        Item {
            width: root.bodyWidth(root.columnWidths[7])
            height: parent.height

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                DataStoreButton {
                    width: 96
                    height: 28
                    text: qsTr("查看分析详情")
                    normalColor: root.detailEnabled ? "#2B4D97" : "#30343D"
                    hoverColor: root.detailEnabled ? "#3862BE" : "#30343D"
                    pressedColor: root.detailEnabled ? "#203F7D" : "#30343D"
                    textColor: root.detailEnabled ? "#E6FFFFFF" : "#7E8796"
                    fontSize: 12
                    enabledButton: root.detailEnabled
                    onClicked: root.detailClicked()
                }

DataStoreButton {
                    width: 48
                    height: 28
                    text: qsTr("删除")
                    normalColor: root.deleteEnabled ? "#5A3438" : "#30343D"
                    hoverColor: root.deleteEnabled ? "#78444B" : "#30343D"
                    pressedColor: root.deleteEnabled ? "#43262A" : "#30343D"
                    textColor: root.deleteEnabled ? "#FFABB2" : "#7E8796"
                    fontSize: 12
                    enabledButton: root.deleteEnabled
                    onClicked: root.deleteClicked()
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#2A3348"
    }

    MouseArea {
        id: rowHoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        z: -1
        onClicked: root.rowClicked()
    }
}
