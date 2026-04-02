import QtQuick 2.15

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
        enabled: root.enabledButton
        hoverEnabled: true
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: root.scale = 0.97
        onReleased: root.scale = containsMouse ? 1.02 : 1.0
        onExited: root.scale = 1.0
        onClicked: root.clicked()
    }
}
