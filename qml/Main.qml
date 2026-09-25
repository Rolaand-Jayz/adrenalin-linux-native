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
            objectName: "header"
            Accessible.id: "header"
            Accessible.name: qsTr("Application header")
            Accessible.role: Accessible.Grouping
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            color: "#191c23"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 14

                Rectangle {
                    objectName: "brandMark"
                    Accessible.ignored: true
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
                    objectName: "applicationTitle"
                    text: appDisplayName
                    Accessible.id: "applicationTitle"
                    Accessible.name: text
                    Accessible.role: Accessible.StaticText
                    color: "#f4f4f5"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                NotificationsPanel {
                    objectName: "notificationsPanel"
                    client: sessionNotificationsClient
                    windowWidth: window.width
                    windowHeight: window.height
                }

                Text {
                    objectName: "platformLabel"
                    text: qsTr("Linux")
                    Accessible.id: "platformLabel"
                    Accessible.name: text
                    Accessible.role: Accessible.StaticText
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
                objectName: "screenTitle"
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("%1 for Linux").arg(appDisplayName)
                Accessible.id: "screenTitle"
                Accessible.name: text
                Accessible.role: Accessible.Heading
                color: "#f4f4f5"
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            Text {
                objectName: "screenDescription"
                Layout.fillWidth: true
                text: qsTr("The native desktop shell is running. Hardware and feature status will appear as their Linux providers become available.")
                Accessible.id: "screenDescription"
                Accessible.name: text
                Accessible.role: Accessible.StaticText
                color: "#b5b8c2"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            ProductTelemetryConsent {
                objectName: "productTelemetryConsent"
                Layout.fillWidth: true
                settingsClient: sessionSettingsClient
            }
        }

        Item { Layout.fillHeight: true }
    }
}
