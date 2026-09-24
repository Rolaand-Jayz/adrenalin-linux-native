import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: appDisplayName
    color: "#111318"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            color: "#191c23"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    radius: 15
                    color: "#ed1c24"

                    Text {
                        anchors.centerIn: parent
                        text: "A"
                        color: "white"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                }

                Text {
                    text: appDisplayName
                    color: "#f4f4f5"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: qsTr("Linux")
                    color: "#a8abb4"
                    font.pixelSize: 13
                }
            }
        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 560
            Layout.leftMargin: 32
            Layout.rightMargin: 32
            spacing: 12

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("%1 for Linux").arg(appDisplayName)
                color: "#f4f4f5"
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The native desktop shell is running. Hardware and feature status will appear as their Linux providers become available.")
                color: "#b5b8c2"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        Item { Layout.fillHeight: true }
    }
}
