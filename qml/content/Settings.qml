import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "white"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // Title
        Text {
            id: titleText
            text: qsTr("Advanced Configuration")
            font.pixelSize: 48
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 20
        }

        // Settings form
        GridLayout {
            columns: 2
            columnSpacing: 20
            rowSpacing: 15
            Layout.alignment: Qt.AlignHCenter

            Text {
                text: qsTr("TX Server IP:")
                font.pixelSize: 24
                font.bold: true
            }
            TextField {
                id: txIpInput
                text: configManager ? configManager.txServerIp : "192.168.4.1"
                font.pixelSize: 20
                Layout.preferredWidth: 300
                Layout.preferredHeight: 40
                placeholderText: "192.168.4.1"
                onTextChanged: if (configManager) configManager.txServerIp = text
            }

            Text {
                text: qsTr("RX Server IP:")
                font.pixelSize: 24
                font.bold: true
            }
            TextField {
                id: rxIpInput
                text: configManager ? configManager.rxHostIp : "127.0.0.1"
                font.pixelSize: 20
                Layout.preferredWidth: 300
                Layout.preferredHeight: 40
                placeholderText: "127.0.0.1"
                onTextChanged: if (configManager) configManager.rxHostIp = text
            }

            Text {
                text: qsTr("Resolution:")
                font.pixelSize: 24
                font.bold: true
            }
            ComboBox {
                id: resolutionCombo
                model: configManager ? configManager.resolutionOptions : ["1280 x 720", "1920 x 1080"]
                currentIndex: configManager ? configManager.resolutionIndex : 0
                Layout.preferredWidth: 300
                Layout.preferredHeight: 40
                font.pixelSize: 18
                onCurrentIndexChanged: if (configManager) configManager.resolutionIndex = currentIndex
            }

            Text {
                text: qsTr("Frame Rate:")
                font.pixelSize: 24
                font.bold: true
            }
            ComboBox {
                id: framerateCombo
                model: configManager ? configManager.framerateOptions : ["30 fps", "50 fps", "60 fps"]
                currentIndex: configManager ? configManager.framerateIndex : 0
                Layout.preferredWidth: 300
                Layout.preferredHeight: 40
                font.pixelSize: 18
                onCurrentIndexChanged: if (configManager) configManager.framerateIndex = currentIndex
            }

            Text {
                text: qsTr("Rotate (0-3):")
                font.pixelSize: 24
                font.bold: true
            }
            SpinBox {
                id: rotateSpin
                from: 0
                to: 3
                value: configManager ? configManager.rotate : 0
                Layout.preferredWidth: 300
                Layout.preferredHeight: 40
                font.pixelSize: 18
                onValueChanged: if (configManager) configManager.rotate = value
            }
        }

        // Status label
        Text {
            id: statusLabel
            text: configManager ? configManager.statusMessage : "Ready"
            font.pixelSize: 20
            color: {
                var msg = configManager ? configManager.statusMessage : "Ready"
                if (msg === "Connection OK" || msg.indexOf("saved") >= 0) return "green"
                if (msg === "Ready") return "black"
                return "red"
            }
            Layout.alignment: Qt.AlignHCenter
        }

        // Spacer
        Item {
            Layout.fillHeight: true
        }

        // Buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            Button {
                text: qsTr("Test Connection")
                font.pixelSize: 18
                font.bold: true
                Layout.preferredWidth: 180
                Layout.preferredHeight: 50
                enabled: configManager ? !configManager.isBusy : true
                onClicked: if (configManager) configManager.testConnection()
            }

            Button {
                text: qsTr("Save Config")
                font.pixelSize: 18
                font.bold: true
                Layout.preferredWidth: 150
                Layout.preferredHeight: 50
                enabled: configManager ? !configManager.isBusy : true
                onClicked: if (configManager) configManager.saveConfig()
            }

            Button {
                text: qsTr("Cancel")
                font.pixelSize: 18
                font.bold: true
                Layout.preferredWidth: 150
                Layout.preferredHeight: 50
                onClicked: if (typeof settingsPopup !== "undefined" && settingsPopup) settingsPopup.close()
            }
        }
    }
    
    // Update fields when config changes (e.g., loaded from camera)
    Connections {
        target: configManager
        enabled: configManager !== null
        function onTxServerIpChanged() {
            if (configManager) txIpInput.text = configManager.txServerIp
        }
        function onRxHostIpChanged() {
            if (configManager) rxIpInput.text = configManager.rxHostIp
        }
        function onResolutionIndexChanged() {
            if (configManager) resolutionCombo.currentIndex = configManager.resolutionIndex
        }
        function onFramerateIndexChanged() {
            if (configManager) framerateCombo.currentIndex = configManager.framerateIndex
        }
        function onRotateChanged() {
            if (configManager) rotateSpin.value = configManager.rotate
        }
    }
}
