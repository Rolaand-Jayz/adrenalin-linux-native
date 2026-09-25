import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    Accessible.id: "toastNotificationsPreference"
    Accessible.name: qsTr("Toast notification preference")
    Accessible.role: Accessible.Grouping

    required property var client
    implicitWidth: 520
    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 10

        RowLayout {
            objectName: "toastNotificationsPreferenceRow"
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    objectName: "toastNotificationsPreferenceLabel"
                    Layout.fillWidth: true
                    text: qsTr("Toast notifications")
                    Accessible.id: "toastNotificationsPreferenceLabel"
                    Accessible.name: text
                    Accessible.role: Accessible.StaticText
                    font.pixelSize: 16
                }

                Label {
                    objectName: "toastNotificationsPreferenceDescription"
                    Layout.fillWidth: true
                    text: qsTr("Choose whether Adrenalin toast notifications are enabled. Delivery is not implemented yet; this saves your preference.")
                    wrapMode: Text.WordWrap
                    opacity: 0.72
                }
            }

            Switch {
                id: notificationsSwitch
                objectName: "toastNotificationsSwitch"
                Accessible.id: "toastNotificationsSwitch"
                Accessible.role: Accessible.Switch
                enabled: root.client !== null && root.client.ready
                checked: root.client !== null && root.client.configured
                    && root.client.enabled
                onToggled: {
                    if (root.client !== null && root.client.ready)
                        root.client.setEnabled(checked)
                }
                Accessible.name: qsTr("Toast notifications")
                Accessible.description: qsTr("Choose whether Adrenalin toast notifications are enabled. Delivery is not implemented yet; this saves your preference.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.client !== null && root.client.ready
                && !root.client.configured

            Label {
                objectName: "toastNotificationsUnconfiguredStatus"
                Layout.fillWidth: true
                text: qsTr("No preference has been saved.")
                Accessible.id: "toastNotificationsUnconfiguredStatus"
                Accessible.name: text
                Accessible.role: Accessible.StaticText
                wrapMode: Text.WordWrap
            }

            Button {
                objectName: "saveToastNotificationsOffButton"
                Accessible.id: "saveToastNotificationsOffButton"
                Accessible.name: qsTr("Save notifications off")
                text: qsTr("Keep off")
                onClicked: {
                    if (root.client !== null && root.client.ready)
                        root.client.setEnabled(false)
                }
            }
        }

        Label {
            objectName: "toastNotificationsServiceStatus"
            Accessible.id: "toastNotificationsServiceStatus"
            Accessible.name: text
            Accessible.role: Accessible.StaticText
            Layout.fillWidth: true
            visible: root.client === null || !root.client.ready
                || root.client.lastOperationCode !== ""
            text: root.client === null
                ? qsTr("Settings service unavailable")
                : root.client.lastOperationCode !== ""
                    ? qsTr("Preference update result: %1").arg(root.client.lastOperationCode)
                    : qsTr("Settings service: %1").arg(root.client.status)
            wrapMode: Text.WordWrap
            color: "#f2b84b"
        }
    }
}
