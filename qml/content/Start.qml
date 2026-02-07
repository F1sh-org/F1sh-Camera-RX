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

    // Camera Selection Dialog (shown when multiple cameras found via mDNS)
    Dialog {
        id: cameraSelectionDialog
        title: "Select Camera"
        modal: true
        anchors.centerIn: parent
        width: 450
        height: 350
        standardButtons: Dialog.Cancel

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            Label {
                text: "Multiple cameras found on the network. Please select one:"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ListView {
                id: cameraListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: mdnsManager ? mdnsManager.discoveredCameras : []

                delegate: ItemDelegate {
                    width: cameraListView.width
                    height: 60

                    contentItem: ColumnLayout {
                        spacing: 2

                        Label {
                            text: modelData.name || "Unknown Camera"
                            font.bold: true
                            font.pointSize: 14
                        }

                        Label {
                            text: modelData.ip + ":" + modelData.port + " (" + modelData.encoding + ")"
                            font.pointSize: 11
                            opacity: 0.7
                        }
                    }

                    onClicked: {
                        if (mdnsManager) {
                            mdnsManager.selectCamera(index)
                            cameraSelectionDialog.close()

                            // Update TX server IP and test connection
                            if (configManager && mdnsManager.cameraIp) {
                                configManager.txServerIp = mdnsManager.cameraIp
                                configManager.testConnection()
                                if (logManager) logManager.logMessage("Selected camera: " + modelData.name + " at " + modelData.ip)
                            }
                        }
                    }
                }
            }
        }
    }

    // Handle multiple cameras found signal
    Connections {
        target: mdnsManager
        function onMultipleCamerasFound() {
            cameraSelectionDialog.open()
        }

        // Auto-set TX IP when single camera is found on startup
        function onCameraFoundChanged() {
            if (mdnsManager && mdnsManager.cameraFound && mdnsManager.cameraIp) {
                if (configManager) {
                    configManager.txServerIp = mdnsManager.cameraIp
                    if (logManager) logManager.logMessage("Auto-discovered camera: " + mdnsManager.cameraHostname + " at " + mdnsManager.cameraIp)
                }
            }
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
                enabled: (serialPortManager && serialPortManager.cameraConnected) || (mdnsManager && mdnsManager.cameraFound)
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
                enabled: (serialPortManager && serialPortManager.cameraConnected) || (mdnsManager && mdnsManager.cameraFound)
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
                enabled: (serialPortManager && serialPortManager.cameraConnected) || (mdnsManager && mdnsManager.cameraFound)
                onClicked: {
                    if (configManager) {
                        // If connected via serial, use serial saveConfig
                        if (serialPortManager && serialPortManager.cameraConnected) {
                            configManager.saveConfig()
                        }
                        // If connected via mDNS/gRPC, use swapResolution
                        else if (grpcManager && mdnsManager && mdnsManager.cameraFound) {
                            var rotate = configManager.rotate
                            var swap = (rotate === 0 || rotate === 2) ? 1 : 0
                            if (logManager) logManager.logMessage("Saving rotation via gRPC (swap=" + swap + ") for rotate=" + rotate)
                            grpcManager.swapResolution(swap)
                        }
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
                visible: !((serialPortManager && serialPortManager.cameraConnected) || (mdnsManager && mdnsManager.cameraFound))
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
                visible: (serialPortManager && serialPortManager.cameraConnected) || (mdnsManager && mdnsManager.cameraFound)
                
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
                text: {
                    if (mdnsManager && mdnsManager.isDiscovering) return qsTr("Discovering...")
                    if (grpcManager && grpcManager.isBusy) return qsTr("Connecting...")
                    return qsTr("Connect Camera")
                }
                font.pointSize: 18
                font.bold: true
                // Enabled when not busy and camera found via mDNS or has TX IP
                enabled: {
                    if (!configManager) return false
                    var notBusy = !configManager.isBusy && !(grpcManager && grpcManager.isBusy)
                    var notDiscovering = !(mdnsManager && mdnsManager.isDiscovering)
                    var hasCameraOrCanDiscover = (mdnsManager && mdnsManager.cameraFound) ||
                                                  (configManager.txServerIp && configManager.txServerIp !== "192.168.4.1") ||
                                                  true  // Always allow clicking to start discovery
                    return notBusy && notDiscovering
                }
                onClicked: {
                    // If camera already found via mDNS, connect via gRPC
                    if (mdnsManager && mdnsManager.cameraFound && mdnsManager.cameraIp) {
                        // Detect local IP for the camera's subnet
                        var rxIp = configManager ? configManager.detectLocalIpForTarget(mdnsManager.cameraIp) : "127.0.0.1"
                        var grpcAddress = mdnsManager.cameraIp + ":" + mdnsManager.controlPort
                        if (logManager) logManager.logMessage("Connect Camera: Connecting to " + grpcAddress + "...")
                        if (logManager) logManager.logMessage("Connect Camera: RX IP = " + rxIp + ", Stream port = " + mdnsManager.cameraPort)
                        if (grpcManager) {
                            grpcManager.serverAddress = grpcAddress
                            // First do health check, then update host on success
                            grpcManager.healthCheck()
                        }
                        if (configManager) {
                            configManager.txServerIp = mdnsManager.cameraIp
                            configManager.grpcServerAddress = grpcAddress
                            configManager.rxHostIp = rxIp
                        }
                    }
                    // If we have a valid TX IP from previous discovery or WiFi
                    else if (configManager && configManager.txServerIp && configManager.txServerIp !== "192.168.4.1") {
                        var rxIp = configManager.detectLocalIpForTarget(configManager.txServerIp)
                        var grpcAddr = configManager.grpcServerAddress || (configManager.txServerIp + ":50051")
                        if (logManager) logManager.logMessage("Connect Camera: Connecting to " + grpcAddr + "...")
                        if (logManager) logManager.logMessage("Connect Camera: RX IP = " + rxIp)
                        if (grpcManager) {
                            grpcManager.serverAddress = grpcAddr
                            grpcManager.healthCheck()
                        }
                        if (configManager) {
                            configManager.rxHostIp = rxIp
                        }
                    }
                    // Otherwise try mDNS discovery
                    else {
                        if (logManager) logManager.logMessage("Connect Camera: Starting mDNS discovery for _f1sh-camera._tcp...")
                        if (mdnsManager) {
                            mdnsManager.startDiscovery()
                        }
                    }
                }

                // Handle gRPC health check result - then send RX host IP
                Connections {
                    target: grpcManager
                    function onHealthCheckResult(success) {
                        if (success) {
                            if (logManager) logManager.logMessage("Connect Camera: Health check OK, sending RX host IP...")
                            // Now send the RX host IP to the camera
                            var rxIp = configManager ? configManager.rxHostIp : "127.0.0.1"
                            if (grpcManager) {
                                grpcManager.updateHost(rxIp)
                            }
                        } else {
                            if (logManager) logManager.logMessage("Connect Camera: Connection failed. Check if camera is running.")
                        }
                    }

                    function onUpdateHostResult(success, message) {
                        if (success) {
                            if (logManager) logManager.logMessage("Connect Camera: Successfully configured camera to stream to this device!")
                            // Now send the rotation/orientation via SwapResolution
                            // swap=1 for landscape (rotate 0 or 2), swap=0 for portrait (rotate 1 or 3)
                            if (grpcManager && configManager) {
                                var rotate = configManager.rotate
                                var swap = (rotate === 0 || rotate === 2) ? 1 : 0
                                if (logManager) logManager.logMessage("Connect Camera: Sending rotation (swap=" + swap + ") for rotate=" + rotate)
                                grpcManager.swapResolution(swap)
                            }
                        } else {
                            if (logManager) logManager.logMessage("Connect Camera: Failed to update host - " + message)
                        }
                    }

                    function onSwapResolutionResult(success, message, width, height) {
                        if (success) {
                            if (logManager) logManager.logMessage("Connect Camera: Rotation applied successfully! Resolution: " + width + "x" + height)
                            // Save the settings after rotation is applied
                            if (configManager) {
                                configManager.saveSettings()
                            }
                        } else {
                            if (logManager) logManager.logMessage("Connect Camera: Failed to apply rotation - " + message)
                            // Still save settings even if rotation failed
                            if (configManager) {
                                configManager.saveSettings()
                            }
                        }
                    }
                }

                // Handle mDNS discovery result (for manual discovery)
                Connections {
                    target: mdnsManager
                    function onDiscoveryFinished(found, ip, port) {
                        // Only handle single camera auto-connect here
                        // Multiple cameras are handled by the cameraSelectionDialog
                        if (found && ip) {
                            if (logManager) logManager.logMessage("Connect Camera: Found camera at " + ip)
                            var rxIp = configManager ? configManager.detectLocalIpForTarget(ip) : "127.0.0.1"
                            if (configManager) {
                                configManager.txServerIp = ip
                                configManager.rxHostIp = rxIp
                            }
                            // Auto-connect via gRPC
                            if (grpcManager && mdnsManager) {
                                var grpcAddress = ip + ":" + mdnsManager.controlPort
                                grpcManager.serverAddress = grpcAddress
                                grpcManager.healthCheck()
                                if (configManager) {
                                    configManager.grpcServerAddress = grpcAddress
                                }
                            }
                        } else if (!found) {
                            if (logManager) logManager.logMessage("Connect Camera: Camera not found via mDNS. Try connecting to camera WiFi network.")
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
                // Enabled when camera is connected (gRPC connection successful)
                enabled: {
                    if (!configManager) return false
                    // Enable if camera connected via gRPC, OR if we have a valid TX IP from mDNS
                    var hasTxIp = configManager.txServerIp && configManager.txServerIp !== "192.168.4.1"
                    var hasMdnsCamera = mdnsManager && mdnsManager.cameraFound
                    var hasGrpcConnection = grpcManager && grpcManager.isConnected
                    var notBusy = !configManager.isBusy && !(grpcManager && grpcManager.isBusy)
                    return (hasGrpcConnection || hasTxIp || hasMdnsCamera) && notBusy
                }
                onClicked: {
                    // Open the camera display/stream view
                    mainWindow.openCameraDisplay()
                    if (logManager) logManager.logMessage("Opening camera stream view...")
                }
            }
        }

        }
    } // End scaledContent
}
