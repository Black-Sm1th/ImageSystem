import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.2

Window {
    id: logWindow
    width: 900
    height: 520
    title: qsTr("AetherDesk - 流程日志")
    color: "#1A1D24"
    visibility: Window.Minimized
    visible: true

    Rectangle {
        anchors.fill: parent
        color: "#1A1D24"

        Column {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: titleBar
                width: parent.width
                height: 36
                color: "#24272E"

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        text: "流程日志"
                        color: "#E6FFFFFF"
                        font.pixelSize: 14
                        font.family: "Alibaba PuHuiTi 3.0"
                        font.bold: true
                    }

                    Text {
                        id: lineCountLabel
                        text: ""
                        color: "#80FFFFFF"
                        font.pixelSize: 12
                        font.family: "Alibaba PuHuiTi 3.0"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Rectangle {
                        width: 60
                        height: 24
                        radius: 4
                        color: autoScrollToggle.containsMouse ? "#3C4450" : (autoScroll ? "#2A5A2A" : "#3A3D44")
                        border.color: autoScroll ? "#4A8A4A" : "#4A4D54"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: autoScroll ? "自动滚" : "手动"
                            color: autoScroll ? "#80FF80" : "#A0A0A0"
                            font.pixelSize: 11
                            font.family: "Alibaba PuHuiTi 3.0"
                        }

                        MouseArea {
                            id: autoScrollToggle
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: autoScroll = !autoScroll
                        }
                    }

                    Rectangle {
                        width: 50
                        height: 24
                        radius: 4
                        color: clearBtn.containsMouse ? "#5A3438" : "#3A3D44"
                        border.color: "#4A4D54"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "清空"
                            color: "#FFABB2"
                            font.pixelSize: 11
                            font.family: "Alibaba PuHuiTi 3.0"
                        }

                        MouseArea {
                            id: clearBtn
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: $LogManager.clear()
                        }
                    }
                }
            }

            ScrollView {
                id: scrollView
                width: parent.width
                height: parent.height - titleBar.height
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                TextArea {
                    id: logTextArea
                    readOnly: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: "#D0D4DC"
                    selectionColor: "#3C7EFF"
                    selectedTextColor: "#FFFFFF"
                    font.family: "Consolas"
                    font.pixelSize: 12
                    background: Rectangle { color: "#1A1D24" }
                    leftPadding: 10
                    rightPadding: 10
                    topPadding: 6
                    bottomPadding: 6
                    text: $LogManager.logText
                    selectByMouse: true

                    onTextChanged: {
                        if (autoScroll) {
                            scrollToBottomTimer.restart()
                        }
                        var count = text.split("\n").length
                        lineCountLabel.text = "(" + count + " 行)"
                    }
                }
            }
        }
    }

    property bool autoScroll: true

    Timer {
        id: scrollToBottomTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (autoScroll) {
                logTextArea.cursorPosition = logTextArea.text.length
            }
        }
    }
}
