import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.0

Rectangle {
    id: root
    visible: false
    color: "#E6111217"
    radius: 12
    border.width: 1
    border.color: "#30FFFFFF"
    clip: true

    property int historyIndex: -1

    function toggle() {
        if (visible) {
            hideAnim.start()
        } else {
            visible = true
            showAnim.start()
            commandInput.forceActiveFocus()
        }
    }

    function appendOutput(text) {
        if (text === "\x1B[CLEAR]") {
            outputText.text = ""
            return
        }
        outputText.text += text
        scrollToBottom()
    }

    function scrollToBottom() {
        Qt.callLater(function() {
            if (outputFlick.contentHeight > outputFlick.height) {
                outputFlick.contentY = outputFlick.contentHeight - outputFlick.height
            }
        })
    }

    NumberAnimation {
        id: showAnim
        target: root
        property: "opacity"
        from: 0; to: 1
        duration: 200
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: hideAnim
        target: root
        property: "opacity"
        from: 1; to: 0
        duration: 150
        easing.type: Easing.InCubic
        onFinished: root.visible = false
    }

    Connections {
        target: $PythonConsole
        function onOutputAppended(text) {
            root.appendOutput(text)
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: commandInput.forceActiveFocus()
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: titleBar
            width: parent.width
            height: 40
            color: "#1A1B23"
            radius: 12

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 12
                color: parent.color
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: ">"
                    color: "#3C7EFF"
                    font.pixelSize: 16
                    font.bold: true
                    font.family: "Consolas"
                }
                Text {
                    text: "Python Console"
                    color: "#E5FFFFFF"
                    font.pixelSize: 14
                    font.family: "Alibaba PuHuiTi 3.0"
                    font.weight: Font.Medium
                }
                Text {
                    text: $PythonConsole.isExecuting ? "(running...)" : ""
                    color: "#FFD700"
                    font.pixelSize: 12
                    font.family: "Consolas"
                    visible: $PythonConsole.isExecuting
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: clearBtnArea.containsMouse ? "#33FFFFFF" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "C"
                        color: "#80FFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                        font.family: "Consolas"
                    }
                    MouseArea {
                        id: clearBtnArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            $PythonConsole.clearOutput()
                        }
                    }
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: closeBtnArea.containsMouse ? "#44E81123" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: "#80FFFFFF"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: closeBtnArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggle()
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: parent.height - titleBar.height - inputBar.height
            color: "transparent"

            Flickable {
                id: outputFlick
                anchors.fill: parent
                anchors.margins: 12
                contentWidth: width
                contentHeight: outputText.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: "#40FFFFFF"
                    }
                }

                TextEdit {
                    id: outputText
                    width: parent.width - 8
                    text: ""
                    color: "#D4D4D4"
                    font.family: "Consolas"
                    font.pixelSize: 13
                    readOnly: true
                    selectByMouse: true
                    selectionColor: "#264F78"
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                }
            }
        }

        Rectangle {
            id: inputBar
            width: parent.width
            height: 40
            color: "#1A1B23"
            radius: 12

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 12
                color: parent.color
            }

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: "#20FFFFFF"
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Text {
                    text: ">>>"
                    color: "#3C7EFF"
                    font.family: "Consolas"
                    font.pixelSize: 14
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }

                TextInput {
                    id: commandInput
                    width: parent.width - 50
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#E5FFFFFF"
                    selectionColor: "#264F78"
                    selectedTextColor: "#FFFFFF"
                    font.family: "Consolas"
                    font.pixelSize: 13
                    clip: true
                    enabled: !$PythonConsole.isExecuting

                    property string placeholderText: "输入 Python 命令..."

                    Text {
                        anchors.fill: parent
                        text: commandInput.placeholderText
                        color: "#40FFFFFF"
                        font: commandInput.font
                        visible: !commandInput.text && !commandInput.activeFocus
                        verticalAlignment: Text.AlignVCenter
                    }

                    Keys.onPressed: {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (commandInput.text.trim().length > 0) {
                                $PythonConsole.executeCommand(commandInput.text)
                                root.historyIndex = -1
                                commandInput.text = ""
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            var sz = $PythonConsole.historySize()
                            if (sz > 0) {
                                if (root.historyIndex < 0) {
                                    root.historyIndex = sz - 1
                                } else if (root.historyIndex > 0) {
                                    root.historyIndex--
                                }
                                commandInput.text = $PythonConsole.getHistory(root.historyIndex)
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            var sz2 = $PythonConsole.historySize()
                            if (root.historyIndex >= 0) {
                                if (root.historyIndex < sz2 - 1) {
                                    root.historyIndex++
                                    commandInput.text = $PythonConsole.getHistory(root.historyIndex)
                                } else {
                                    root.historyIndex = -1
                                    commandInput.text = ""
                                }
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_L && (event.modifiers & Qt.ControlModifier)) {
                            $PythonConsole.clearOutput()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            root.toggle()
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
