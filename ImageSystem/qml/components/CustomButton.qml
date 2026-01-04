import QtQuick 2.15
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.0

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
    // 渐变相关属性
    property bool useGradient: false  // 是否使用渐变背景
    property color gradientStartColor: "#3C7EFF"
    property color gradientEndColor: "#572499"
    property bool animateGradient: false  // 是否动画渐变
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
    
    // 渐变背景层
    Item {
        id: gradientLayer
        anchors.fill: parent
        visible: useGradient
        
        Rectangle {
            id: gradientRect
            anchors.fill: parent
            radius: buttonRadius
            clip: true
            
            gradient: Gradient {
                orientation: Gradient.Horizontal  // 水平渐变（左到右）
                GradientStop { 
                    position: 0.0
                    color: gradientStartColor 
                }
                GradientStop { 
                    position: 1.0
                    color: gradientEndColor 
                }
            }
            
            // 动画光效层（水平移动）
            Rectangle {
                id: shineLayer
                width: parent.width * 2
                height: parent.height
                x: animateGradient ? shineAnimation.xPosition : -parent.width
                gradient: Gradient {
                    orientation: Gradient.Horizontal  // 水平渐变
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.4; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.2) }
                    GradientStop { position: 0.6; color: "transparent" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                
                SequentialAnimation {
                    id: shineAnimation
                    running: animateGradient
                    loops: Animation.Infinite
                    property real xPosition: -gradientRect.width
                    
                    PauseAnimation { duration: 300 }
                    NumberAnimation {
                        target: shineAnimation
                        property: "xPosition"
                        from: -gradientRect.width
                        to: gradientRect.width
                        duration: 1500
                        easing.type: Easing.InOutQuad
                    }
                    PauseAnimation { duration: 300 }
                }
            }
        }
    }
    
    // 按钮内容（图标+文字）
    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: iconSource !== "" ? 8 : 0
        z: 10  // 确保内容在渐变层上方
        
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
