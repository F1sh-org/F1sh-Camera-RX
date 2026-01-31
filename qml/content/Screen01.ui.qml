

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick
import QtQuick.Controls
import UntitledProject

Rectangle {
    id: rectangle
    width: Constants.width
    height: Constants.height

    color: Constants.backgroundColor

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
        source: "../assets/asset-v1_STEAMforVietnam+Scratch101+2020_08+type@asset+block@course_card__1_.png"
        fillMode: Image.PreserveAspectFit

        Button {
            id: button1
            x: 863
            y: 931
            width: 210
            height: 58
            text: qsTr("Login")
            font.bold: true
            highlighted: true
            font.family: "Times New Roman"
            font.pointSize: 21
        }

        Text {
            id: text1
            x: 622
            y: 777
            text: qsTr("User Name:")
            font.pixelSize: 40
            font.bold: true
        }

        Text {
            id: text2
            x: 652
            y: 853
            text: qsTr("Password:")
            font.pixelSize: 40
            font.bold: true
        }

        TextArea {
            id: textArea1
            x: 855
            y: 853
            width: 466
            height: 53

            visible: rectangle.color.valid
            verticalAlignment: Text.AlignVCenter
            clip: rectangle.color.valid
            placeholderTextColor: "#eaeaea"
            font.pointSize: 15
            placeholderText: qsTr("Please Enter Password")
            background: Rectangle {
                color: "#ccc7c7" // Set your desired background color (e.g., light red)
                radius: 5 // Optional: Add rounded corners
            }
        }

        TextArea {
            id: textArea2
            x: 855
            y: 777
            width: 466
            height: 53
            visible: rectangle.color.valid
            verticalAlignment: Text.AlignVCenter
            placeholderTextColor: "#eaeaea"
            placeholderText: qsTr("Please Enter Username")
            font.pointSize: 15
            clip: rectangle.color.valid
            background: Rectangle {
                color: "#ccc7c7"
                radius: 5
            }
        }
    }
    states: [
        State {
            name: "clicked"
        }
    ]
}
