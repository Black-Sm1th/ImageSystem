import QtQuick 2.9
import QtQuick.Controls 2.2

Rectangle {
    id: customButton
    
    // 可配置属性
    property int buttonWidth: contentRow.width + 20
    property int buttonHeight: 40
    property color borderColor: "#E0E0E0"
    property color textColor: "#E5FFFFFF"
    property color backgroundColor: "#3C7EFF"
    property int fontSize: 16
    property string text: qsTr("按钮")
    property string iconSource: ""  // 图标路径，为空则不显示图标
    property int borderWidth: 0
    property int buttonRadius: 8
    property bool containsMouse: mouseArea.containsMouse
    // hover和pressed状态的颜色
    property color hoverBackgroundColor: Qt.lighter(backgroundColor, 1.1)
    property color pressedBackgroundColor: Qt.darker(backgroundColor, 1.1)
    property color hoverBorderColor: Qt.darker(borderColor, 1.2)
    property color pressedBorderColor: Qt.darker(borderColor, 1.4)
    property color hoverTextColor: textColor
    property color pressedTextColor: textColor
    
    // 点击信号
    signal clicked()
    signal pressed()
    signal released()
    
    // 设置尺寸
    width: buttonWidth
    height: buttonHeight
    
    // 背景样式
    color: {
        if (mouseArea.pressed) {
            return pressedBackgroundColor
        } else if (mouseArea.containsMouse) {
            return hoverBackgroundColor
        } else {
            return backgroundColor
        }
    }
    
    border.color: {
        if (mouseArea.pressed) {
            return pressedBorderColor
        } else if (mouseArea.containsMouse) {
            return hoverBorderColor
        } else {
            return borderColor
        }
    }
    
    border.width: borderWidth
    radius: buttonRadius
    
    // 按钮内容（图标+文字）
    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: iconSource !== "" ? 8 : 0
        
        Image {
            id: buttonIcon
            source: iconSource
            visible: iconSource !== ""
            anchors.verticalCenter: parent.verticalCenter
        }
        
        Text {
            id: buttonText
            text: customButton.text
            font.pixelSize: fontSize
            color: {
                if (mouseArea.pressed) {
                    return pressedTextColor
                } else if (mouseArea.containsMouse) {
                    return hoverTextColor
                } else {
                    return textColor
                }
            }
            anchors.verticalCenter: parent.verticalCenter
            font.family: "Alibaba PuHuiTi 3.0"
        }
    }
    
    // 鼠标交互区域
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        
        onClicked: {
            customButton.clicked()
        }
        
        onPressed: {
            customButton.pressed()
        }
        
        onReleased: {
            customButton.released()
        }
    }
    
    // 动画效果
    Behavior on color {
        ColorAnimation {
            duration: 150
            easing.type: Easing.OutQuad
        }
    }
    
    Behavior on border.color {
        ColorAnimation {
            duration: 150
            easing.type: Easing.OutQuad
        }
    }
    
    Behavior on scale {
        NumberAnimation {
            duration: 100
            easing.type: Easing.OutQuad
        }
    }
    
    // 按下时的缩放效果
    states: [
        State {
            name: "pressed"
            when: mouseArea.pressed
            PropertyChanges {
                target: customButton
                scale: 0.98
            }
        }
    ]
    
    transitions: [
        Transition {
            to: "pressed"
            NumberAnimation {
                property: "scale"
                duration: 50
                easing.type: Easing.OutQuad
            }
        },
        Transition {
            from: "pressed"
            NumberAnimation {
                property: "scale"
                duration: 100
                easing.type: Easing.OutBack
            }
        }
    ]
} 
