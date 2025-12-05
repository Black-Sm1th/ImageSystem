import QtQuick 2.15
import QtQuick.Controls 2.15
import com.vtk.dicom 1.0

// 可复用的四视图显示面板
Item {
    id: root
    
    // 左上：轴向视图
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 5
        anchors.leftMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#000000"
        border.color: "#404040"
        border.width: 1

        AxialView {
            id: axialView
            anchors.fill: parent
            anchors.margins: 2
        }
    }

    // 右上：矢状视图
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.rightMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        SagittalView {
            id: sagittalView
            anchors.fill: parent
            anchors.margins: 2
        }
    }

    // 左下：冠状视图
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.bottomMargin: 5
        anchors.leftMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        CoronalView {
            id: coronalView
            anchors.fill: parent
            anchors.margins: 2
        }
    }

    // 右下：3D体渲染视图
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 5
        anchors.rightMargin: 5
        width: (parent.width - 15) / 2
        height: (parent.height - 15) / 2
        color: "#1a1a1a"
        border.color: "#404040"
        border.width: 1

        VolumeView {
            id: volumeView
            anchors.fill: parent
            anchors.margins: 2
        }
    }
}

