import QtQuick 2.9
import QtQuick.Controls 2.2

Rectangle {
    id: singleLineTextInput
    
    // 可配置属性
    property int inputWidth: 300
    property int inputHeight: 30
    property color borderColor: "transparent"
    property color focusedBorderColor: "#006BFF"
    property color backgroundColor: "#383838"
    property color textColor: "#ffffff"
    property color placeholderColor: "#c3c3c3"
    property int fontSize: 16
    property string placeholderText: qsTr("请输入...")
    property int borderWidth: 1
    property int inputRadius: 8
    
    // 文本属性
    property alias text: textField.text
    property alias readOnly: textField.readOnly
    property alias echoMode: textField.echoMode
    property alias maximumLength: textField.maximumLength
    property alias validator: textField.validator
    property alias inputMethodHints: textField.inputMethodHints
    
    // 信号
    signal accepted()
    signal editingFinished()
    signal focusChanged(bool hasFocus)
    
    // 设置尺寸
    width: inputWidth
    height: inputHeight
    
    // 背景样式
    color: backgroundColor
    border.color: textField.activeFocus && !readOnly ? focusedBorderColor : borderColor
    border.width: borderWidth
    radius: inputRadius
    
    TextField {
        id: textField
        anchors.fill: parent
        font.pixelSize: fontSize
        font.family: "Alibaba PuHuiTi 3.0"
        color: textColor
        selectByMouse: !readOnly
        padding: 4
        placeholderText: !readOnly ? singleLineTextInput.placeholderText : ""
        placeholderTextColor: placeholderColor
        
        background: Rectangle {
            color: "transparent"
        }
        
        // 信号连接
        onAccepted: singleLineTextInput.accepted()
        onEditingFinished: singleLineTextInput.editingFinished()
        onActiveFocusChanged: singleLineTextInput.focusChanged(activeFocus)
    }
    
    // 边框颜色动画
    Behavior on border.color {
        ColorAnimation { duration: 200 }
    }
    
    // 提供焦点控制方法
    function forceActiveFocus() {
        textField.forceActiveFocus()
    }
    
    function clear() {
        textField.clear()
    }
}
