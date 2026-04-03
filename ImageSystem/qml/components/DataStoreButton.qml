import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root

    // 通用按钮组件，统一处理悬停、按压和点击态
    property string text: qsTr("")
    property color normalColor: "#3C7EFF"
    property color hoverColor: "#5B90FF"
    property color pressedColor: "#2A63DA"
    property color borderColor: "transparent"
    property color textColor: "#FFFFFF"
    property int radiusSize: 6
    property int fontSize: 14
    property bool enabledButton: true
    property string tooltipText: ""

    signal clicked()

    radius: radiusSize
    border.color: root.borderColor
    border.width: root.borderColor === "transparent" ? 0 : 1
    color: {
        if (!enabledButton)
            return "#3A4256"
        if (buttonMouseArea.pressed)
            return pressedColor
        if (buttonMouseArea.containsMouse)
            return hoverColor
        return normalColor
    }
    opacity: enabledButton ? 1.0 : 0.6

    Behavior on color {
        ColorAnimation { duration: 120 }
    }

    Behavior on scale {
        NumberAnimation { duration: 100 }
    }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.pixelSize: root.fontSize
        font.family: "Alibaba PuHuiTi 3.0"
    }

    MouseArea {
        id: buttonMouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: root.enabledButton ? Qt.LeftButton : Qt.NoButton
        cursorShape: root.enabledButton ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: if (root.enabledButton) root.scale = 0.97
        onReleased: root.scale = (root.enabledButton && containsMouse) ? 1.02 : 1.0
        onExited: root.scale = 1.0
        onClicked: if (root.enabledButton) root.clicked()

        ToolTip {
            id: buttonToolTip
            parent: buttonMouseArea
            visible: root.tooltipText !== "" && buttonMouseArea.containsMouse
            text: root.tooltipText
            delay: 500
            x: (buttonMouseArea.width - width) / 2
            y: -height - 8
            padding: 10

            background: Rectangle {
                radius: 6
                color: "#2B4D97"
                border.width: 1
                border.color: "#3862BE"
            }

            contentItem: Text {
                text: buttonToolTip.text
                color: "#FFFFFF"
                font.pixelSize: 12
                font.family: "Alibaba PuHuiTi 3.0"
                wrapMode: Text.Wrap
            }
        }
    }
}
