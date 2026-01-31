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
        spacing: 10

        // Title row
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Text {
                text: qsTr("Application Log")
                font.pixelSize: 36
                font.bold: true
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: logManager ? qsTr("%1 entries").arg(logManager.logCount) : "0 entries"
                font.pixelSize: 18
                color: "#666666"
            }
        }

        // Log text area with scroll
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f5f5f5"
            border.color: "#cccccc"
            border.width: 1
            radius: 5

            ScrollView {
                id: scrollView
                anchors.fill: parent
                anchors.margins: 5
                clip: true

                TextArea {
                    id: logTextArea
                    text: logManager ? logManager.logText : ""
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 14
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    color: "#333333"
                    background: Rectangle { color: "transparent" }
                    
                    // Auto-scroll to bottom when new log entries are added
                    onTextChanged: {
                        cursorPosition = text.length
                    }
                }
            }
        }

        // Buttons row
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            Button {
                text: qsTr("Clear Log")
                font.pixelSize: 18
                font.bold: true
                Layout.preferredWidth: 150
                Layout.preferredHeight: 50
                onClicked: {
                    if (logManager) logManager.clear()
                }
            }

            Button {
                text: qsTr("Close")
                font.pixelSize: 18
                font.bold: true
                Layout.preferredWidth: 150
                Layout.preferredHeight: 50
                onClicked: {
                    if (typeof logPopup !== "undefined" && logPopup) logPopup.close()
                }
            }
        }
    }
    
    // Connection to auto-scroll when new log entries arrive
    Connections {
        target: logManager
        enabled: logManager !== null
        function onNewLogEntry(message) {
            // Scroll to bottom
            logTextArea.cursorPosition = logTextArea.text.length
        }
    }
}
