import QtQuick 2.9
import QtQuick.Window 2.2
import QtQuick.Layouts 1.3
import com.vtk.dicom 1.0

Window {
    id: win
    visible: true
    width: 1280
    height: 960
    title: qsTr("DICOM Viewer - 三视图 + 3D")

    GridLayout {
        anchors.fill: parent
        anchors.margins: 5
        rows: 2
        columns: 2
        rowSpacing: 5
        columnSpacing: 5

        // 左上：轴向视图
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            AxialView {
                id: axialView
                anchors.fill: parent
                anchors.margins: 2
            }
        }

        // 右上：矢状视图
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            SagittalView {
                id: sagittalView
                anchors.fill: parent
                anchors.margins: 2
            }
        }

        // 左下：冠状视图
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            CoronalView {
                id: coronalView
                anchors.fill: parent
                anchors.margins: 2
            }
        }

        // 右下：3D体渲染视图
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            VolumeView {
                id: volumeView
                anchors.fill: parent
                anchors.margins: 2
            }
        }
    }
}

