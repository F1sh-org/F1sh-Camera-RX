import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: 1920
    height: 1080

    Pane {
        id: pane
        x: 0
        y: 0
        width: 1920
        height: 1080
        Pane {
            id: pane1
            x: -12
            y: -12
            width: 1920
            height: 126
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
                    font.pointSize: 20
                    font.bold: true
                }
            }

            Text {
                id: text1
                x: 776
                y: 11
                text: qsTr("Camera View")
                font.pixelSize: 60
                font.bold: true
            }
        }

        Button {
            id: button2
            x: 1605
            y: -6
            width: 103
            height: 74
            text: qsTr("Button")
            icon.width: 80
            icon.source: "../assets/logging.png"
            icon.height: 80
            icon.cache: true
            flat: true
            display: AbstractButton.IconOnly
        }

        Button {
            id: button1
            x: 1714
            y: -6
            width: 103
            height: 77
            visible: true
            text: qsTr("Button")
            icon.width: 80
            icon.source: "../assets/setting.png"
            icon.height: 80
            icon.color: "#000000"
            flat: true
            display: AbstractButton.IconOnly
            clip: false
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
            icon.width: 80
            icon.source: "../assets/exit.png"
            icon.height: 80
            icon.color: "#ff0000"
            highlighted: false
            font.weight: Font.Bold
            font.pointSize: 30
            font.bold: true
            focusPolicy: Qt.ClickFocus
            flat: true
            display: AbstractButton.IconOnly
            clip: false
        }
    }
}
