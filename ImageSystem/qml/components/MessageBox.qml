import QtQuick 2.9
import QtQuick.Controls 2.2
import QtGraphicalEffects 1.0

// 统一的消息组件
Item {
    id: messageBox
    anchors.fill: parent
    z: 9999  // 确保消息显示在最上层
    
    // 消息列表模型
    ListModel {
        id: messageModel
    }
    
    // 消息容器
    Column {
        id: messageContainer
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 20
        spacing: 10
        
        // 消息列表
        Repeater {
            model: messageModel
            
            // 单个消息组件（内联定义）
            Rectangle {
                id: messageRoot
                
                property string messageType: model.type
                property string messageText: model.text
                
                width: Math.max(320, messageContent.width + 60)
                height: 56
                radius: 8
                
                // 根据类型设置颜色
                color: {
                    switch(messageType) {
                        case "success": return "#F0F9FF"
                        case "error": return "#FEF2F2" 
                        case "warning": return "#FFFBEB"
                        case "info": 
                        default: return "#F8F9FA"
                    }
                }
                
                // border.width: 1
                // border.color: {
                //     switch(messageType) {
                //         case "success": return "#D1FAE5"
                //         case "error": return "#FECACA"
                //         case "warning": return "#FDE68A"
                //         case "info":
                //         default: return "#E5E7EB"
                //     }
                // }
                
                // 出现动画
                opacity: 0
                scale: 0.8
                y: -20
                
                Component.onCompleted: {
                    appearAnimation.start()
                    // 自动消失定时器
                    var timer = Qt.createQmlObject(
                        'import QtQuick 2.9; Timer { interval: ' + (model.duration || 3000) + '; running: true; repeat: false }',
                        messageRoot
                    )
                    timer.triggered.connect(function() {
                        disappearAnimation.start()
                        timer.destroy()
                    })
                }
                
                // 出现动画
                ParallelAnimation {
                    id: appearAnimation
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "scale"
                        from: 0.8
                        to: 1
                        duration: 300
                        easing.type: Easing.OutBack
                    }
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "y"
                        from: -20
                        to: 0
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }
                
                // 消失动画
                ParallelAnimation {
                    id: disappearAnimation
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: 200
                        easing.type: Easing.InCubic
                    }
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "scale"
                        from: 1
                        to: 0.8
                        duration: 200
                        easing.type: Easing.InBack
                    }
                    
                    NumberAnimation {
                        target: messageRoot
                        property: "y"
                        from: 0
                        to: -20
                        duration: 200
                        easing.type: Easing.InCubic
                    }
                    
                    onStopped: {
                        removeMessage(index)
                    }
                }
                
                Row {
                    id: messageContent
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    spacing: 12
                    
                    // 图标
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        anchors.verticalCenter: parent.verticalCenter
                        
                        color: {
                            switch(messageType) {
                                case "success": return "#22C55E"
                                case "error": return "#EF4444"
                                case "warning": return "#F59E0B"
                                case "info":
                                default: return "#3B82F6"
                            }
                        }
                        
                        // 图标符号
                        Text {
                            anchors.centerIn: parent
                            color: "white"
                            font.pixelSize: 12
                            font.bold: true
                            text: {
                                switch(messageType) {
                                    case "success": return "✓"
                                    case "error": return "✕"
                                    case "warning": return "!"
                                    case "info":
                                    default: return "i"
                                }
                            }
                        }
                    }
                    
                    // 消息文本
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: messageText
                        font.pixelSize: 14
                        color: "#D9000000"
                        font.family: "Alibaba PuHuiTi 3.0"
                        wrapMode: Text.WordWrap
                        
                        // 限制最大宽度
                        Component.onCompleted: {
                            if (contentWidth > 300) {
                                width = 300
                            }
                        }
                    }
                }
                
                // 鼠标悬停效果
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    
                    onEntered: {
                        messageRoot.scale = 1.02
                    }
                    
                    onExited: {
                        messageRoot.scale = 1.0
                    }
                    
                    Behavior on scale {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutQuad
                        }
                    }
                }
                
                // // 阴影效果
                // DropShadow {
                //     anchors.fill: messageRoot
                //     source: messageRoot
                //     radius: 8
                //     samples: 16
                //     color: "#0A000000"
                //     horizontalOffset: 0
                //     verticalOffset: 2
                //     z: -1
                // }
            }
        }
    }
    
    // 添加消息的函数
    function showMessage(type, text, duration) {
        messageModel.append({
            type: type || "info",
            text: text || "",
            duration: duration || 1000
        })
    }
    
    // 移除消息的函数
    function removeMessage(index) {
        if (index >= 0 && index < messageModel.count) {
            messageModel.remove(index)
        }
    }
    
    // 便捷方法
    function success(text, duration) {
        showMessage("success", text, duration)
    }
    
    function error(text, duration) {
        showMessage("error", text, duration)
    }
    
    function warning(text, duration) {
        showMessage("warning", text, duration)
    }
    
    function info(text, duration) {
        showMessage("info", text, duration)
    }
    
    // 清除所有消息
    function clear() {
        messageModel.clear()
    }
} 
