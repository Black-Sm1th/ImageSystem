import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: toolButton
    width: 36
    height: 36
    color: "transparent"
    
    // 公共属性
    property string iconSource: ""
    property bool isSelected: false
    
    // 信号
    signal clicked()
    
    Image {
        source: toolButton.isSelected ? "qrc:/image/leftToolBtnSelected.png" : "qrc:/image/leftToolBtn.png"
        anchors.fill: parent
        z: 1
        opacity: toolButton.isSelected ? 1.0 : (mouseArea.containsMouse ? 0.8 : 1.0)
        
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
    
    Image {
        source: toolButton.iconSource
        anchors.centerIn: parent
        z: 2
    }
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: {
            toolButton.clicked()
        }
    }
}

