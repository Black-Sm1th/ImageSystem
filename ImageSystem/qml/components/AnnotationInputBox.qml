import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15
// 标注输入框组件
Item {
    id: root
    
    // 公开属性
    property int annotationIndex: -1
    property int orientation: 0  // 0=Axial, 1=Sagittal, 2=Coronal
    property alias text: textInput.text
    
    // 信号
    signal textConfirmed(string text)
    signal cancelled()
    
    width: 200
    height: annotationCol.height
    visible: false
    z: 1000  // 确保在最上层
    
    // 背景
    Rectangle {
        anchors.fill: parent
        color: "#CC303338"
        radius: 8
        
        // 阴影效果
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 2
            verticalOffset: 2
            radius: 8
            samples: 16
            color: "#80000000"
        }
    }
    
    Column {
        id: annotationCol
        width: parent.width
        padding: 8
        spacing: 12
        // 标题
        Text {
            width: parent.width
            text: qsTr("标注文字")
            color: "#00FFFF"
            font.pixelSize: 14
            font.bold: true
        }
        
        // 输入框
        SingleLineTextInput {
            id: textInput
            width: parent.width - 16
            height: 28
            inputRadius: 4
            backgroundColor: "#14FFFFFF"
        }
        
        // 按钮行
        Row {
            width: parent.width - 16
            spacing: 8
            

            CustomButton{
                width: (parent.width - 8) / 2
                height: 36
                backgroundColor: "#27C346"
                text: qsTr("确定")
                onClicked: root.confirm()
            }
            CustomButton{
                width: (parent.width - 8) / 2
                height: 36
                backgroundColor: "#F76965"
                text: qsTr("取消")
                onClicked: root.cancel()
            }
        }
    }
    
    // 公开方法
    function show(screenX, screenY, annIndex, orient, targetContainer) {
        annotationIndex = annIndex
        orientation = orient
        
        // 每次都重新设置parent为对应的视图容器
        if (targetContainer) {
            parent = targetContainer
        }
        
        // VTK的显示坐标系统：
        // - 原点在左下角
        // - X轴向右，Y轴向上
        // QML坐标系统：
        // - 原点在左上角
        // - X轴向右，Y轴向下
        
        // 调试输出
        console.log("VTK坐标: (" + screenX + ", " + screenY + "), 视图: " + orient + ", 容器大小: " + parent.width + "x" + parent.height)
        
        // 转换坐标：Y轴需要翻转
        var localX = screenX
        var localY = parent.height - screenY
        
        console.log("转换后坐标: (" + localX + ", " + localY + ")")
        
        // 输入框放在标注正上方中间
        // X坐标居中（减去输入框宽度的一半）
        // Y坐标在上方（减去高度和间距）
        var newX = localX - width / 2
        var newY = localY - height - 10  // 恢复原来的偏移
        
        // 边界检查和调整
        // 检查右边界
        if (newX + width > parent.width - 10) {
            newX = parent.width - width - 10
        }
        
        // 检查左边界
        if (newX < 10) {
            newX = 10
        }
        
        // 检查上边界（QML坐标，小值在上）
        if (newY < 10) {
            // 如果上方空间不够，放在标注点下方
            newY = localY + 10
        }
        
        // 检查下边界
        if (newY + height > parent.height - 10) {
            newY = parent.height - height - 10
        }
        
        console.log("最终位置: (" + newX + ", " + newY + ")")
        
        x = newX
        y = newY
        
        textInput.text = ""
        textInput.forceActiveFocus()
        visible = true
    }
    
    function confirm() {
        textConfirmed(textInput.text)
        visible = false
    }
    
    function cancel() {
        cancelled()
        visible = false
    }
}

