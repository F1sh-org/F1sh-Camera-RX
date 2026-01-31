import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// QtQuick.Studio.DesignEffects removed - not available in standard Qt6

Item {
    id: root
    // Use parent size instead of fixed dimensions
    anchors.fill: parent

    // Scale factor for responsive UI (design was for 1920x1080)
    readonly property real scaleX: width / 1920
    readonly property real scaleY: height / 1080
    readonly property real scaleFactor: Math.min(scaleX, scaleY)

    // Selected network for connection
    property var selectedNetwork: null

    // Start auto-detection when component loads
    Component.onCompleted: {
        if (typeof serialPortManager !== "undefined" && serialPortManager !== null) {
            serialPortManager.startAutoDetect()
        }
    }
    Component.onDestruction: {
        if (typeof serialPortManager !== "undefined" && serialPortManager !== null) {
            serialPortManager.stopAutoDetect()
        }
    }

    // WiFi Password Dialog
    Dialog {
        id: wifiPasswordDialog
        title: selectedNetwork ? "Connect to " + selectedNetwork.ssid : "Connect to WiFi"
        modal: true
        anchors.centerIn: parent
        width: 400
        height: 250
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            if (selectedNetwork && wifiManager) {
                // Auto-detect RX IP instead of using saved value
                var rxIp = configManager ? configManager.detectLocalIp() : "192.168.1.1"
                console.log("Using RX IP:", rxIp)
                wifiManager.connectToNetwork(selectedNetwork.bssid, passwordField.text, rxIp)
                passwordField.text = ""
            }
        }

        onRejected: {
            passwordField.text = ""
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15

            Label {
                text: selectedNetwork ? "Network: " + selectedNetwork.ssid : ""
                font.bold: true
                font.pixelSize: 16
            }

            Label {
                text: "Enter WiFi password:"
                font.pixelSize: 14
            }

            TextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: "Password"
                echoMode: TextInput.Password
                font.pixelSize: 14
            }

            Label {
                id: connectionStatus
                text: {
                    if (wifiManager && wifiManager.isConnecting) return "Connecting..."
                    if (wifiManager && wifiManager.errorMessage) return wifiManager.errorMessage
                    return ""
                }
                color: wifiManager && wifiManager.errorMessage ? "red" : "black"
                font.pixelSize: 12
                visible: text !== ""
            }
        }
    }

    // Connection success/failure handlers
    Connections {
        target: wifiManager
        function onConnectionSucceeded(txIp) {
            wifiPasswordDialog.close()
            // Update configManager with TX IP and RX IP
            if (configManager) {
                configManager.txServerIp = txIp
                // Also update RX IP with the detected local IP
                var localIp = configManager.detectLocalIp()
                configManager.rxHostIp = localIp
                // Mark direction as saved to enable Connect Camera button
                configManager.setDirectionSaved(true)
                // Save settings
                configManager.saveSettings()
            }
            console.log("WiFi connected! TX IP:", txIp)
        }
        function onConnectionFailed(error) {
            // Dialog stays open, error is shown via connectionStatus label
            console.log("WiFi connection failed:", error)
        }
    }

    // Scaled container - scales the entire UI to fit
    Item {
        id: scaledContent
        width: 1920
        height: 1080
        anchors.centerIn: parent
        scale: root.scaleFactor

        Pane {
            id: pane
            x: 0
            y: 0
            width: 1920
            height: 1080
            background: Rectangle { color: "white" }

            Pane {
                id: pane1
                x: -12
                y: -12
                width: 1920
                height: 126
                background: Rectangle { color: "white" }

            Image {
                id: image
                x: -61
                y: -6
                width: 367
                height: 113
                source: "../assets/1764717324865-1688492344211912770c7a1fb613e0a56ae183550b0d15104ace41.png"
                scale: 1
                fillMode: Image.PreserveAspectCrop

                Pane {
                    id: pane2
                    x: 49
                    y: 119
                    width: 1921
                    height: 959
                    font.bold: true
                    font.pointSize: 20
                    background: Rectangle { color: "white" }

                    Grid {
                        id: grid
                        width: 948
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 934
                        anchors.topMargin: 79
                        anchors.bottomMargin: 409

                        // Dynamic WiFi list
                        ListView {
                            id: wifiListView
                            width: parent.width
                            height: 400
                            clip: true
                            spacing: 5
                            model: wifiManager ? wifiManager.networks : []

                            delegate: Button {
                                width: wifiListView.width
                                height: 50
                                highlighted: modelData.isConnected
                                
                                contentItem: Row {
                                    spacing: 10
                                    leftPadding: 10
                                    
                                    // WiFi signal indicator
                                    Text {
                                        text: {
                                            var strength = modelData.signalStrength
                                            if (strength >= 75) return "▂▄▆█"
                                            else if (strength >= 50) return "▂▄▆_"
                                            else if (strength >= 25) return "▂▄__"
                                            else return "▂___"
                                        }
                                        font.pixelSize: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: modelData.isConnected ? "#00aa00" : "#666666"
                                    }
                                    
                                    // SSID name
                                    Text {
                                        text: modelData.ssid
                                        font.bold: true
                                        font.pointSize: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: modelData.isConnected ? "#00aa00" : "#000000"
                                    }
                                    
                                    // Lock icon for secured networks
                                    Text {
                                        text: modelData.isSecured ? "🔒" : ""
                                        font.pixelSize: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    
                                    // Connected indicator
                                    Text {
                                        text: modelData.isConnected ? "(Connected)" : ""
                                        font.italic: true
                                        font.pointSize: 12
                                        color: "#00aa00"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                
                                onClicked: {
                                    console.log("Selected WiFi:", modelData.ssid)
                                    root.selectedNetwork = modelData
                                    wifiPasswordDialog.open()
                                }
                            }

                            // Show message when no WiFi networks found or scanning
                            Label {
                                anchors.centerIn: parent
                                text: {
                                    if (wifiManager && wifiManager.isScanning) return qsTr("Scanning...")
                                    if (serialPortManager && !serialPortManager.cameraConnected) return qsTr("No camera connected")
                                    if (wifiManager && wifiManager.errorMessage) return wifiManager.errorMessage
                                    return qsTr("Press Refresh to scan WiFi")
                                }
                                font.pixelSize: 20
                                color: "#888888"
                                visible: wifiListView.count === 0
                            }
                        }

                    }



                    Image {
                        id: image1
                        x: 20
                        y: 79
                        width: 856
                        height: 484
                        source: "qrcimages/template_image.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    Image {
                        id: image2
                        x: 906
                        y: -12
                        width: 4
                        height: 947
                        source: "qrcimages/template_image.png"
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        id: text1
                        x: 1329
                        y: -12
                        text: qsTr("Wifi Setting")
                        font.pixelSize: 50
                        font.bold: true
                    }
                    Text {
                        id: text2
                        x: 273
                        y: -12
                        text: qsTr("Camera Direction")
                        font.pixelSize: 50
                        font.bold: true
                    }
                }
            }
        }

        Button {
            id: button2
            x: 1605
            y: -6
            width: 103
            height: 74
            text: qsTr("Log")
            icon.cache: true
            icon.height: 80
            icon.width: 80
            icon.source: "../assets/logging.png"
            display: AbstractButton.IconOnly
            flat: true
            
            onClicked: {
                mainWindow.openLog()
            }
        }

        Button {
            id: button1
            x: 1714
            y: -6
            width: 103
            height: 77
            visible: true
            text: qsTr("Button")
            icon.color: "#000000"
            flat: true
            clip: false
            icon.height: 80
            icon.width: 80
            icon.source: "../assets/setting.png"
            display: AbstractButton.IconOnly
            
            onClicked: {
                mainWindow.openSettings()
            }
        }

        Button {
            id: button
            x: 1823
            y: -9
            width: 79
            height: 80
            opacity: 1
            visible: true
            text: qsTr("X")
            icon.height: 80
            icon.width: 80
            icon.source: "../assets/exit.png"
            font.weight: Font.Bold
            focusPolicy: Qt.ClickFocus
            clip: false
            display: AbstractButton.IconOnly
            highlighted: false
            flat: true
            font.bold: true
            font.pointSize: 30
            icon.color: "#ff0000"
            
            onClicked: {
                Qt.quit()
            }
        }

        Rectangle {
            id: rectangle
            x: -12
            y: 108
            width: 938
            height: 746
            color: "#00f60000"
            border.color: "#0031ff"
            border.width: 8

            Button {
                id: button7
                x: 32
                y: 637
                width: 195
                height: 65
                text: qsTr("+90°")
                font.bold: true
                font.pointSize: 20
                enabled: serialPortManager ? serialPortManager.cameraConnected : false
                onClicked: {
                    if (configManager) {
                        var newRotate = (configManager.rotate + 1) % 4
                        configManager.rotate = newRotate
                    }
                }
            }

            Button {
                id: button8
                x: 360
                y: 637
                width: 195
                height: 65
                text: qsTr("-90°")
                font.bold: true
                font.pointSize: 20
                enabled: serialPortManager ? serialPortManager.cameraConnected : false
                onClicked: {
                    if (configManager) {
                        var newRotate = (configManager.rotate + 3) % 4  // +3 is same as -1 mod 4
                        configManager.rotate = newRotate
                    }
                }
            }

            Button {
                id: button9
                x: 667
                y: 637
                width: 195
                height: 65
                text: qsTr("Save")
                font.pointSize: 20
                font.bold: true
                enabled: serialPortManager ? serialPortManager.cameraConnected : false
                onClicked: {
                    if (configManager) {
                        configManager.saveConfig()
                    }
                }
            }

            Button {
                id: button10
                x: 1112
                y: 637
                width: 195
                height: 65
                text: (wifiManager && wifiManager.isScanning) ? qsTr("Scanning...") : qsTr("Refresh")
                font.pointSize: 20
                font.bold: true
                enabled: wifiManager && serialPortManager ? (!wifiManager.isScanning && serialPortManager.cameraConnected) : false
                onClicked: {
                    if (wifiManager) wifiManager.refresh()
                }
            }

            Button {
                id: button13
                x: 1550
                y: 637
                width: 195
                height: 65
                text: qsTr("Forget")
                font.pointSize: 20
                font.bold: true
            }

            // "No camera connected" label - shown when no camera detected
            Label {
                id: noCameraLabel
                x: 200
                y: 200
                width: 500
                height: 300
                text: qsTr("No Camera Connected")
                font.pixelSize: 40
                font.bold: true
                color: "#ff0000"
                visible: serialPortManager ? !serialPortManager.cameraConnected : true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            // Camera rotation preview (visual indicator like ui_rotate.c)
            // Only shown when camera is connected
            Rectangle {
                id: rotationPreview
                x: 200
                y: 100
                width: 500
                height: 500
                color: "#f8f8f8"
                border.color: "#cccccc"
                border.width: 2
                radius: 10
                visible: serialPortManager ? serialPortManager.cameraConnected : false
                
                // Rotation angle text
                Text {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 20
                    text: configManager ? (configManager.rotate * 90) + "°" : "0°"
                    font.pixelSize: 40
                    font.bold: true
                    color: "#333333"
                }
                
                // Camera body representation
                Rectangle {
                    id: cameraBody
                    width: 100
                    height: 200
                    color: "#e0e0e0"
                    border.color: "#000000"
                    border.width: 3
                    anchors.centerIn: parent
                    rotation: configManager ? configManager.rotate * 90 : 0
                    
                    Behavior on rotation {
                        NumberAnimation { duration: 200 }
                    }
                    
                    // Camera lens
                    Rectangle {
                        width: 50
                        height: 50
                        radius: 25
                        color: "#b0b0b0"
                        border.color: "#000000"
                        border.width: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 30
                        
                        // Inner lens
                        Rectangle {
                            width: 24
                            height: 24
                            radius: 12
                            color: "#000000"
                            anchors.centerIn: parent
                        }
                    }
                }
                
                // Rotation labels around the preview
                Text {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 10
                    text: "0°"
                    font.pixelSize: 20
                    color: configManager && configManager.rotate === 0 ? "#0066ff" : "#888888"
                    font.bold: configManager && configManager.rotate === 0
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 15
                    text: "90°"
                    font.pixelSize: 20
                    color: configManager && configManager.rotate === 1 ? "#0066ff" : "#888888"
                    font.bold: configManager && configManager.rotate === 1
                }
                Text {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 60
                    text: "180°"
                    font.pixelSize: 20
                    color: configManager && configManager.rotate === 2 ? "#0066ff" : "#888888"
                    font.bold: configManager && configManager.rotate === 2
                }
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 10
                    text: "270°"
                    font.pixelSize: 20
                    color: configManager && configManager.rotate === 3 ? "#0066ff" : "#888888"
                    font.bold: configManager && configManager.rotate === 3
                }
            }
            // color: "#ffffff"
        }

        Rectangle {
            id: rectangle1
            x: 919
            y: 108
            width: 989
            height: 746
            color: "#00f60000"
            border.color: "#0031ff"
            border.width: 8
        }

        // Bottom center buttons - Connect Camera and Open Stream
        Row {
            id: bottomButtonRow
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30
            spacing: 40

            Button {
                id: connectCameraButton
                width: 220
                height: 70
                text: qsTr("Connect Camera")
                font.pointSize: 18
                font.bold: true
                // Enabled when direction has been saved AND we have a TX IP (from WiFi connection)
                enabled: configManager ? configManager.directionSaved && !configManager.isBusy : false
                onClicked: {
                    // If we have a valid TX IP from WiFi connection, test HTTP connection
                    // Otherwise, just mark as connected since we're using serial
                    if (configManager) {
                        if (configManager.txServerIp && configManager.txServerIp !== "192.168.4.1") {
                            configManager.testConnection()
                            if (logManager) logManager.log("Connect Camera: Testing connection to TX server...")
                        } else {
                            // No WiFi connection yet, but camera is connected via serial
                            // This shouldn't happen if workflow is followed correctly
                            if (logManager) logManager.log("Connect Camera: No TX server IP. Please connect to WiFi first.")
                        }
                    }
                }
            }

            Button {
                id: openStreamButton
                width: 220
                height: 70
                text: qsTr("Open Stream")
                font.pointSize: 18
                font.bold: true
                highlighted: true
                // Enabled when we have a valid TX IP (from WiFi connection) and direction is saved
                enabled: {
                    if (!configManager) return false
                    // Enable if camera connected via HTTP test, OR if we have a valid TX IP from WiFi
                    var hasTxIp = configManager.txServerIp && configManager.txServerIp !== "192.168.4.1"
                    var dirSaved = configManager.directionSaved
                    return (configManager.cameraConnected || hasTxIp) && dirSaved && !configManager.isBusy
                }
                onClicked: {
                    // Open the camera display/stream view
                    mainWindow.openCameraDisplay()
                    if (logManager) logManager.log("Opening camera stream view...")
                }
            }
        }

        }
    } // End scaledContent
}
