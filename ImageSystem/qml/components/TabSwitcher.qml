import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    width: tabTitles.length * 68 + (tabTitles.length - 1) * 4 + 8
    height: 32
    color: "#383838"
    radius: 8
    
    property int currentIndex: 0
    property var tabTitles: []
    
    signal tabChanged(int index)
    
    // 选中状态的滑动指示器
    Rectangle {
        id: indicator
        width: 68
        height: 24
        radius: 6
        color: "grey"
        x: 4 + root.currentIndex * (68 + 4)
        y: 4
        z: 0
        
        // 滑动动画
        Behavior on x {
            NumberAnimation {
                duration: 250
                easing.type: Easing.OutCubic
            }
        }
    }
    
    // Tab选项
    Row {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 4
        spacing: 4
        
        Repeater {
            model: root.tabTitles
            
            Rectangle {
                id: tabItem
                width: 68
                height: 24
                radius: 6
                color: "transparent"
                
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    font.family: "Alibaba PuHuiTi 3.0"
                    font.pixelSize: 14
                    color: root.currentIndex === index ? "#ffffff" : "#c3c3c3"
                }
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (root.currentIndex !== index) {
                            root.currentIndex = index
                            root.tabChanged(index)
                        }
                    }
                }
            }
        }
    }
} 
