import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
// import the VTK module
import VTK 9.2



ApplicationWindow {
    // title of the application
    title: qsTr("VTK QtQuick App")
    width: 800
    height: 800
    color: palette.window

    Component.onCompleted: {
        console.log("ApplicationWindow has been loaded successfully.")
    }

    SystemPalette {
        id: palette
        colorGroup: SystemPalette.Active
    }

    VTKRenderWindow {
        id: vtkwindow
        anchors.fill: parent
    }

    VTKRenderItem {
        id: coneView
        objectName: "ConeView"
        anchors.fill: vtkwindow
        renderWindow: vtkwindow
        focus: true
    }
}
