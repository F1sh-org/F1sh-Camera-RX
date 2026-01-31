import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: mainWindow
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    visibility: Window.Windowed
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "white"
    title: "F1sh Camera RX"

    // Allow dragging the window by clicking anywhere on the header area
    property point dragOffset: Qt.point(0, 0)

    // Center on screen at startup
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2

    // Custom title bar for borderless window
    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        color: "#2d2d2d"
        z: 1000

        // Drag handler
        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)

            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
            }

            onPositionChanged: function(mouse) {
                if (pressed) {
                    mainWindow.x += mouse.x - clickPos.x
                    mainWindow.y += mouse.y - clickPos.y
                }
            }

            onDoubleClicked: {
                if (mainWindow.visibility === Window.Maximized) {
                    mainWindow.visibility = Window.Windowed
                } else {
                    mainWindow.visibility = Window.Maximized
                }
            }
        }

        // Window title
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "F1sh Camera RX"
            color: "white"
            font.pixelSize: 14
            font.bold: true
        }

        // Window controls
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            // Minimize button
            Rectangle {
                width: 30
                height: 24
                color: minimizeArea.containsMouse ? "#505050" : "transparent"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: "−"
                    color: "white"
                    font.pixelSize: 16
                }

                MouseArea {
                    id: minimizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: mainWindow.showMinimized()
                }
            }

            // Maximize/Restore button
            Rectangle {
                width: 30
                height: 24
                color: maximizeArea.containsMouse ? "#505050" : "transparent"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: mainWindow.visibility === Window.Maximized ? "❐" : "□"
                    color: "white"
                    font.pixelSize: 14
                }

                MouseArea {
                    id: maximizeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (mainWindow.visibility === Window.Maximized) {
                            mainWindow.visibility = Window.Windowed
                        } else {
                            mainWindow.visibility = Window.Maximized
                        }
                    }
                }
            }

            // Close button
            Rectangle {
                width: 30
                height: 24
                color: closeArea.containsMouse ? "#e81123" : "transparent"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: Qt.quit()
                }
            }
        }
    }

    Loader {
        id: startLoader
        anchors.fill: parent
        anchors.topMargin: titleBar.height
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
