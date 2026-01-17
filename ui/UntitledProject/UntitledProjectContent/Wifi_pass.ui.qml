/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/

import QtQuick
import QtQuick.Controls
import UntitledProject 1.0

Item {
    id: root
    width: Constants.width
    height: Constants.height

    State {
        name: "clicked"
    }

    Grid {
        id: grid
        x: 545
        y: 381
        width: 400
        height: 400
    }

    Image {
        id: image
        x: 0
        y: 0
        width: 1920
        height: 1080
        source: "../Assets/asset-v1_STEAMforVietnam+Scratch101+2020_08+type@asset+block@course_card__1_.png"
        fillMode: Image.PreserveAspectFit
        Rectangle {
            id: rectangle
            x: 0
            y: 0
            width: 1920
            height: 1080
            color: "#ffffff"

            BorderImage {
                id: borderImage
                x: 0
                y: 0
                width: 1920
                height: 1080
                source: "../Assets/asset-v1_STEAMforVietnam+Scratch101+2020_08+type@asset+block@course_card__1_.png"
            }
        }

        Button {
            id: button1
            x: 855
            y: 898
            width: 210
            height: 58
            text: qsTr("Save")
            highlighted: true
            font.pointSize: 21
            font.family: "Times New Roman"
            font.bold: true
        }

        Text {
            id: text2
            x: 501
            y: 780
            text: qsTr("Password:")
            font.pixelSize: 40
            font.bold: true
        }

        TextArea {
            id: textArea1
            x: 741
            y: 780
            width: 701
            height: 53
            visible: rectangle.color.valid
            verticalAlignment: Text.AlignVCenter
            placeholderTextColor: "#eaeaea"
            placeholderText: qsTr("Please Enter Password")
            font.pointSize: 15
            clip: rectangle.color.valid
            background: Rectangle {
                color: "#ccc7c7"
                radius: 5
            }
        }

    }
}
