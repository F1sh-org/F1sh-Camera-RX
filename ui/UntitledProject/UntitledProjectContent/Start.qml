import QtQuick
import QtQuick.Controls
import QtQuick.Studio.DesignEffects

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
                source: "../Assets/1764717324865-1688492344211912770c7a1fb613e0a56ae183550b0d15104ace41.png"
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

                    Grid {
                        id: grid
                        width: 948
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 934
                        anchors.topMargin: 79
                        anchors.bottomMargin: 409

                        Button {
                            id: button3
                            height: 50
                            text: qsTr("Wifi 1")
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.topMargin: 0
                            display: AbstractButton.TextOnly
                            icon.color: "#ff0000"
                            highlighted: false
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                            DesignEffect {
                                effects: [
                                    DesignDropShadow {
                                    }
                                ]
                            }
                        }

                        Button {
                            id: button4
                            height: 50
                            text: qsTr("Wifi 2")
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.topMargin: 50
                            icon.color: "#ff0000"
                            highlighted: false
                            display: AbstractButton.TextOnly
                            DesignEffect {
                                effects: [
                                    DesignDropShadow {
                                    }]
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                        }

                        Button {
                            id: button5
                            height: 50
                            text: qsTr("Wifi 3")
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.topMargin: 100
                            icon.color: "#ff0000"
                            highlighted: false
                            display: AbstractButton.TextOnly
                            DesignEffect {
                                effects: [
                                    DesignDropShadow {
                                    }]
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                        }

                        Button {
                            id: button6
                            height: 50
                            text: qsTr("Wifi 4")
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.topMargin: 150
                            icon.color: "#ff0000"
                            highlighted: false
                            display: AbstractButton.TextOnly
                            DesignEffect {
                                effects: [
                                    DesignDropShadow {
                                    }]
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
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
                        text: qsTr("Camera Settings")
                        font.pixelSize: 50
                        font.bold: true
                    }

                    Text {
                        id: text3
                        x: 35
                        y: 88
                        text: qsTr("Camera:")
                        font.pixelSize: 30
                        horizontalAlignment: Text.AlignLeft
                        font.bold: true
                    }

                    Text {
                        id: text4
                        x: 35
                        y: 233
                        text: qsTr("Direction:")
                        font.pixelSize: 30
                        horizontalAlignment: Text.AlignLeft
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
            text: qsTr("Button")
            icon.cache: true
            icon.height: 80
            icon.width: 80
            icon.source: "../Assets/logging.png"
            display: AbstractButton.IconOnly
            flat: true

            DesignEffect {
                effects: [
                    DesignDropShadow {
                    }
                ]
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
            icon.source: "../Assets/setting.png"
            display: AbstractButton.IconOnly

            DesignEffect {
                effects: [
                    DesignDropShadow {
                    }
                ]
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
            icon.source: "../Assets/exit.png"
            font.weight: Font.Bold
            focusPolicy: Qt.ClickFocus
            clip: false
            display: AbstractButton.IconOnly
            highlighted: false
            flat: true
            font.bold: true
            font.pointSize: 30
            icon.color: "#ff0000"

            DesignEffect {
                effects: [
                    DesignDropShadow {
                    }
                ]
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
                text: qsTr("Change")
                font.bold: true
                font.pointSize: 20
            }

            Button {
                id: button8
                x: 360
                y: 637
                width: 195
                height: 65
                text: qsTr("Reverse")
                font.bold: true
                font.pointSize: 20
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
            }

            Button {
                id: button11
                x: 360
                y: 801
                width: 269
                height: 85
                text: qsTr("Connect")
                font.pointSize: 25
                font.bold: true
            }

            Button {
                id: button12
                x: 1214
                y: 801
                width: 269
                height: 85
                text: qsTr("Disconnect")
                font.pointSize: 20
                font.bold: true
            }

            Button {
                id: button10
                x: 1112
                y: 637
                width: 195
                height: 65
                text: qsTr("Refresh")
                font.pointSize: 20
                font.bold: true
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

            ComboBox {
                id: comboBox
                x: 74
                y: 165
                width: 526
                height: 45
                font.bold: true
                font.pointSize: 20
                displayText: "Camera 1"
            }

            ComboBox {
                id: comboBox1
                x: 74
                y: 316
                width: 526
                height: 45
                font.pointSize: 20
                font.bold: true
                displayText: "Front"
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


    }
}
