import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: mainWindow
    width: Screen.width
    height: Screen.height
    visible: true
    visibility: Window.FullScreen
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "white"
    title: "F1sh Camera RX"

    Loader {
        id: startLoader
        anchors.fill: parent
        source: "content/Start.qml"
        asynchronous: false
    }
    
    // Settings popup
    Popup {
        id: settingsPopup
        anchors.centerIn: parent
        width: parent.width * 0.8
        height: parent.height * 0.8
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle {
            color: "white"
            border.color: "#0031ff"
            border.width: 2
            radius: 10
        }
        
        Loader {
            id: settingsLoader
            anchors.fill: parent
            active: settingsPopup.opened
            source: "content/Settings.qml"
        }
        
        // Close button for popup
        Button {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            width: 50
            height: 50
            text: "X"
            font.bold: true
            font.pixelSize: 24
            onClicked: settingsPopup.close()
        }
    }
    
    // Log popup
    Popup {
        id: logPopup
        anchors.centerIn: parent
        width: parent.width * 0.85
        height: parent.height * 0.85
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle {
            color: "white"
            border.color: "#333333"
            border.width: 2
            radius: 10
        }
        
        Loader {
            id: logLoader
            anchors.fill: parent
            active: logPopup.opened
            source: "content/Log.qml"
        }
        
        // Close button for popup
        Button {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            width: 50
            height: 50
            text: "X"
            font.bold: true
            font.pixelSize: 24
            onClicked: logPopup.close()
        }
    }
    
    // Camera Display popup (stream view)
    Popup {
        id: cameraDisplayPopup
        anchors.centerIn: parent
        width: parent.width * 0.95
        height: parent.height * 0.95
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        background: Rectangle {
            color: "white"
            border.color: "#0031ff"
            border.width: 2
            radius: 10
        }
        
        Loader {
            id: cameraDisplayLoader
            anchors.fill: parent
            active: cameraDisplayPopup.opened
            source: "content/CameraDisplay.qml"
        }
        
        // Close button for popup
        Button {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            width: 50
            height: 50
            text: "X"
            font.bold: true
            font.pixelSize: 24
            onClicked: cameraDisplayPopup.close()
        }
    }
    
    // Function to open settings
    function openSettings() {
        settingsPopup.open()
    }
    
    // Function to open log
    function openLog() {
        logPopup.open()
    }
    
    // Function to open camera display (stream view)
    function openCameraDisplay() {
        cameraDisplayPopup.open()
    }
}
