import QtQuick 2.9
import QtQuick.Controls 2.2

Item {
    id: root
    
    // 可配置属性
    property int comboWidth: 200
    property int comboHeight: 40
    property color borderColor: "#E0E0E0"
    property color textColor: "#FFFFFF"
    property color backgroundColor: "#383838"
    property color dropdownBackgroundColor: "#2b2b2b"
    property color itemHoverColor: "#404040"
    property color itemSelectedColor: "#065B87"
    property int fontSize: 14
    property string placeholderText: "请选择"
    property int borderWidth: 1
    property int radius: 8
    property int maxDropdownHeight: 300
    
    // 数据模型
    property var model: []  // 数据源，格式: ["选项1", "选项2", "选项3"]
    
    // 选择模式
    property bool multiSelect: false  // false=单选, true=多选
    
    // 选中的项
    property var selectedIndices: []  // 选中项的索引数组
    property string selectedText: ""   // 显示的文本
    
    // 信号
    signal selectionChanged(var selectedIndices, var selectedItems)
    
    width: comboWidth
    height: comboHeight
    
    // 主按钮
    Rectangle {
        id: mainButton
        anchors.fill: parent
        color: mouseArea.containsMouse ? Qt.lighter(backgroundColor, 1.1) : backgroundColor
        border.color: dropdown.visible ? itemSelectedColor : borderColor
        border.width: borderWidth
        radius: root.radius
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        
        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
        
        // 显示文本
        Text {
            id: displayText
            anchors.left: parent.left
            anchors.right: arrowIcon.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            text: root.selectedText || root.placeholderText
            color: root.selectedText ? textColor : Qt.darker(textColor, 1.5)
            font.pixelSize: fontSize
            font.family: "Alibaba PuHuiTi 3.0"
            elide: Text.ElideRight
        }
        
        // 箭头图标
        Text {
            id: arrowIcon
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: dropdown.visible ? "▲" : "▼"
            color: textColor
            font.pixelSize: 10
            
            Behavior on rotation {
                NumberAnimation { duration: 200 }
            }
        }
        
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            
            onClicked: {
                dropdown.visible = !dropdown.visible
            }
        }
    }
    
    // 下拉列表
    Rectangle {
        id: dropdown
        visible: false
        
        // 挂载到最顶层的父容器
        parent: {
            var item = root
            while (item.parent) {
                item = item.parent
            }
            return item
        }
        
        // 使用绝对定位
        x: {
            if (visible && parent) {
                var pos = root.mapToItem(parent, 0, 0)
                return pos.x
            }
            return 0
        }
        y: {
            if (visible && parent) {
                var pos = root.mapToItem(parent, 0, 0)
                return pos.y + root.height + 4
            }
            return 0
        }
        
        width: root.width
        height: Math.min(listView.contentHeight + 8, maxDropdownHeight)
        color: dropdownBackgroundColor
        border.color: borderColor
        border.width: 1
        radius: root.radius
        z: 10000
        
        // 进入/退出动画
        opacity: 0
        scale: 0.95
        transformOrigin: Item.Top
        
        states: [
            State {
                name: "visible"
                when: dropdown.visible
                PropertyChanges {
                    target: dropdown
                    opacity: 1
                    scale: 1
                }
            }
        ]
        
        transitions: [
            Transition {
                NumberAnimation {
                    properties: "opacity,scale"
                    duration: 200
                    easing.type: Easing.OutQuad
                }
            }
        ]
        
        // 列表视图
        ListView {
            id: listView
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            model: root.model
            
            ScrollBar.vertical: ScrollBar {
                width: 8
                policy: ScrollBar.AsNeeded
                visible: listView.contentHeight > listView.height
                
                contentItem: Rectangle {
                    implicitWidth: 8
                    color: "#606060"
                    radius: 4
                    opacity: parent.active ? 1.0 : 0.5
                    
                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }
                
                background: Rectangle {
                    implicitWidth: 8
                    color: "transparent"
                }
            }
            
            delegate: Rectangle {
                id: itemDelegate
                width: listView.width - (listView.contentHeight > listView.height ? 12 : 0)
                height: 36
                color: {
                    if (itemMouseArea.containsMouse) {
                        return itemHoverColor
                    } else if (isSelected) {
                        return Qt.rgba(itemSelectedColor.r, itemSelectedColor.g, itemSelectedColor.b, 0.3)
                    } else {
                        return "transparent"
                    }
                }
                radius: 4
                
                property bool isSelected: root.selectedIndices.indexOf(index) !== -1
                
                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
                
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8
                    
                    // 多选模式下的复选框
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        visible: root.multiSelect
                        color: itemDelegate.isSelected ? itemSelectedColor : "transparent"
                        border.color: itemDelegate.isSelected ? itemSelectedColor : "#808080"
                        border.width: 2
                        radius: 3
                        
                        // 勾选标记
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                            visible: itemDelegate.isSelected
                        }
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    // 单选模式下的圆点
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        visible: !root.multiSelect
                        color: "transparent"
                        border.color: itemDelegate.isSelected ? itemSelectedColor : "#808080"
                        border.width: 2
                        radius: 8
                        
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            color: itemSelectedColor
                            radius: 4
                            visible: itemDelegate.isSelected
                        }
                    }
                    
                    // 选项文本
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - (root.multiSelect ? 32 : 32)
                        text: modelData
                        color: textColor
                        font.pixelSize: fontSize
                        font.family: "Alibaba PuHuiTi 3.0"
                        elide: Text.ElideRight
                    }
                }
                
                MouseArea {
                    id: itemMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: {
                        if (root.multiSelect) {
                            // 多选模式
                            var newSelectedIndices = root.selectedIndices.slice()
                            var idx = newSelectedIndices.indexOf(index)
                            
                            if (idx !== -1) {
                                // 取消选中
                                newSelectedIndices.splice(idx, 1)
                            } else {
                                // 选中
                                newSelectedIndices.push(index)
                            }
                            
                            root.selectedIndices = newSelectedIndices
                            updateSelectedText()
                        } else {
                            // 单选模式
                            root.selectedIndices = [index]
                            root.selectedText = modelData
                            dropdown.visible = false
                            
                            // 触发选择改变信号
                            var selectedItems = [modelData]
                            root.selectionChanged(root.selectedIndices, selectedItems)
                        }
                    }
                }
            }
        }
    }
    
    // 全屏遮罩层 - 点击外部关闭下拉框
    Item {
        id: outsideClickHandler
        visible: dropdown.visible
        z: 9999
        
        // 尝试找到根窗口或最顶层的父项
        parent: {
            var item = root
            while (item.parent) {
                item = item.parent
            }
            return item
        }
        
        // 覆盖整个父容器
        anchors.fill: parent
        
        // 透明背景，捕获点击事件
        MouseArea {
            anchors.fill: parent
            
            onPressed: {
                // 检查点击位置是否在组件区域内
                var clickInComponent = false
                
                // 检查是否点击在主按钮区域
                var posInRoot = mapToItem(root, mouse.x, mouse.y)
                if (posInRoot.x >= 0 && posInRoot.x <= root.width &&
                    posInRoot.y >= 0 && posInRoot.y <= root.height) {
                    clickInComponent = true
                }
                
                // 检查是否点击在下拉框区域
                if (!clickInComponent && dropdown.visible) {
                    var posInDropdown = mapToItem(dropdown, mouse.x, mouse.y)
                    if (posInDropdown.x >= 0 && posInDropdown.x <= dropdown.width &&
                        posInDropdown.y >= 0 && posInDropdown.y <= dropdown.height) {
                        clickInComponent = true
                    }
                }
                
                // 如果点击在外部，关闭下拉框
                if (!clickInComponent) {
                    dropdown.visible = false
                    if (root.multiSelect) {
                        updateSelectedText()
                    }
                    mouse.accepted = true
                } else {
                    // 如果点击在组件内，让事件继续传播
                    mouse.accepted = false
                }
            }
        }
    }
    
    // 更新多选模式下的显示文本
    function updateSelectedText() {
        if (root.multiSelect) {
            var selectedItems = []
            for (var i = 0; i < root.selectedIndices.length; i++) {
                var idx = root.selectedIndices[i]
                if (idx >= 0 && idx < root.model.length) {
                    selectedItems.push(root.model[idx])
                }
            }
            
            if (selectedItems.length === 0) {
                root.selectedText = ""
            } else if (selectedItems.length === 1) {
                root.selectedText = selectedItems[0]
            } else {
                root.selectedText = selectedItems[0] + " 等 " + selectedItems.length + " 项"
            }
            
            // 触发选择改变信号
            root.selectionChanged(root.selectedIndices, selectedItems)
        }
    }
    
    // 清空选择
    function clearSelection() {
        root.selectedIndices = []
        root.selectedText = ""
    }
    
    // 全选（仅多选模式）
    function selectAll() {
        if (root.multiSelect) {
            var allIndices = []
            for (var i = 0; i < root.model.length; i++) {
                allIndices.push(i)
            }
            root.selectedIndices = allIndices
            updateSelectedText()
        }
    }
    
    // 设置选中项（通过索引）
    function setSelectedIndices(indices) {
        root.selectedIndices = indices
        if (root.multiSelect) {
            updateSelectedText()
        } else if (indices.length > 0) {
            var idx = indices[0]
            if (idx >= 0 && idx < root.model.length) {
                root.selectedText = root.model[idx]
            }
        }
    }
    
    // 获取选中的项
    function getSelectedItems() {
        var selectedItems = []
        for (var i = 0; i < root.selectedIndices.length; i++) {
            var idx = root.selectedIndices[i]
            if (idx >= 0 && idx < root.model.length) {
                selectedItems.push(root.model[idx])
            }
        }
        return selectedItems
    }
}


