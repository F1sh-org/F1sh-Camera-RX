import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    // Scale factor for responsive UI
    readonly property real scaleX: width / 1920
    readonly property real scaleY: height / 1080
    readonly property real scaleFactor: Math.min(scaleX, scaleY)

    // Frame counter for image refresh
    property int frameCount: 0

    // Connect to streamManager signals
    Connections {
        target: streamManager
        function onFrameReady() {
            frameCount++
            // Force image reload by toggling source
            videoFrame.source = ""
            videoFrame.source = "image://videoframe/frame?" + frameCount
        }
        function onErrorOccurred(error) {
            errorText.text = error
            errorText.visible = true
        }
    }

    // Background
    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
    }

    // Video display area
    Rectangle {
        id: videoArea
        anchors.fill: parent
        anchors.topMargin: 80 * scaleFactor
        color: "black"

        // Video frame image
        Image {
            id: videoFrame
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false

            // Initial source - will be updated by onFrameReady
            source: ""

            // Placeholder when not streaming
            visible: streamManager ? streamManager.isStreaming : false
        }

        // Placeholder when not streaming
        Column {
            anchors.centerIn: parent
            spacing: 20
            visible: streamManager ? !streamManager.isStreaming : true

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "📷"
                font.pixelSize: 80 * scaleFactor
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: streamManager ? streamManager.status : "Not connected"
                color: "#888888"
                font.pixelSize: 24 * scaleFactor
            }

            Text {
                id: errorText
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#ff6666"
                font.pixelSize: 16 * scaleFactor
                visible: false
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Start Stream"
                font.pixelSize: 18 * scaleFactor
                visible: streamManager ? !streamManager.isStreaming : false
                onClicked: {
                    errorText.visible = false
                    streamManager.start()
                }
            }
        }

        // Streaming indicator
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 10 * scaleFactor
            width: streamingIndicator.width + 20 * scaleFactor
            height: streamingIndicator.height + 10 * scaleFactor
            color: "#80000000"
            radius: 5
            visible: streamManager ? streamManager.isStreaming : false

            Row {
                id: streamingIndicator
                anchors.centerIn: parent
                spacing: 8 * scaleFactor

                Rectangle {
                    width: 10 * scaleFactor
                    height: 10 * scaleFactor
                    radius: 5 * scaleFactor
                    color: "#ff0000"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        running: streamManager ? streamManager.isStreaming : false
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 500 }
                        NumberAnimation { to: 1.0; duration: 500 }
                    }
                }

                Text {
                    text: streamManager ? streamManager.currentDecoder : ""
                    color: "white"
                    font.pixelSize: 12 * scaleFactor
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // Header bar
    Pane {
        id: headerPane
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 80 * scaleFactor
        padding: 0
        background: Rectangle { color: "#2d2d2d" }

        // Logo
        Image {
            id: logo
            anchors.left: parent.left
            anchors.leftMargin: 20 * scaleFactor
            anchors.verticalCenter: parent.verticalCenter
            height: 60 * scaleFactor
            fillMode: Image.PreserveAspectFit
            source: "../assets/1764717324865-1688492344211912770c7a1fb613e0a56ae183550b0d15104ace41.png"
        }

        // Title
        Text {
            anchors.centerIn: parent
            text: "Camera View"
            color: "white"
            font.pixelSize: 32 * scaleFactor
            font.bold: true
        }

        // Control buttons
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 20 * scaleFactor
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10 * scaleFactor

            // Stream toggle button
            Button {
                width: 100 * scaleFactor
                height: 40 * scaleFactor
                text: streamManager && streamManager.isStreaming ? "Stop" : "Start"
                font.pixelSize: 14 * scaleFactor
                highlighted: streamManager ? streamManager.isStreaming : false

                onClicked: {
                    if (streamManager.isStreaming) {
                        streamManager.stop()
                    } else {
                        errorText.visible = false
                        streamManager.start()
                    }
                }
            }

            // Log button
            Button {
                width: 50 * scaleFactor
                height: 50 * scaleFactor
                flat: true
                icon.source: "../assets/logging.png"
                icon.width: 30 * scaleFactor
                icon.height: 30 * scaleFactor

                onClicked: {
                    // TODO: Open log popup
                }
            }

            // Settings button
            Button {
                width: 50 * scaleFactor
                height: 50 * scaleFactor
                flat: true
                icon.source: "../assets/setting.png"
                icon.width: 30 * scaleFactor
                icon.height: 30 * scaleFactor

                onClicked: {
                    // TODO: Open settings popup
                }
            }

            // Close button
            Button {
                width: 50 * scaleFactor
                height: 50 * scaleFactor
                flat: true
                icon.source: "../assets/exit.png"
                icon.width: 30 * scaleFactor
                icon.height: 30 * scaleFactor
                icon.color: "#ff4444"

                onClicked: {
                    streamManager.stop()
                    Qt.quit()
                }
            }
        }
    }

    // Decoder info overlay (bottom right)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10 * scaleFactor
        width: decoderColumn.width + 20 * scaleFactor
        height: decoderColumn.height + 20 * scaleFactor
        color: "#80000000"
        radius: 5
        visible: streamManager ? streamManager.isStreaming : false

        Column {
            id: decoderColumn
            anchors.centerIn: parent
            spacing: 5 * scaleFactor

            Text {
                text: "Port: " + (streamManager ? streamManager.port : "8888")
                color: "#cccccc"
                font.pixelSize: 11 * scaleFactor
            }

            Text {
                text: "Decoder: " + (streamManager ? streamManager.currentDecoder : "None")
                color: "#cccccc"
                font.pixelSize: 11 * scaleFactor
            }
        }
    }

    // Component initialization
    Component.onCompleted: {
        // Set port from config if available
        if (configManager && streamManager) {
            // Use configManager.rxStreamPort if available, otherwise default 8888
            streamManager.port = 8888
            streamManager.rotate = configManager.rotate
        }
    }
}
