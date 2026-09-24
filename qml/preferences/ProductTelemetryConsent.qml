import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var settingsClient
    implicitWidth: 520
    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 10

        RowLayout {
            objectName: "telemetryPreferenceRow"
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    objectName: "telemetryPreferenceLabel"
                    Layout.fillWidth: true
                    text: qsTr("Product telemetry participation")
                    font.pixelSize: 16
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Stores your opt-in choice. Telemetry transmission is not implemented.")
                    wrapMode: Text.WordWrap
                    opacity: 0.72
                }
            }

            Switch {
                id: consentSwitch
                objectName: "productTelemetryConsentSwitch"
                enabled: root.settingsClient !== null && root.settingsClient.ready
                checked: root.settingsClient !== null
                    && root.settingsClient.productTelemetryConsent
                onToggled: {
                    if (root.settingsClient !== null && root.settingsClient.ready)
                        root.settingsClient.setProductTelemetryConsent(checked)
                }
                Accessible.name: qsTr("Product telemetry participation opt-in")
                Accessible.description: qsTr("Only saves your consent preference. No telemetry is sent by this build.")
            }
        }

        Label {
            objectName: "serviceStatus"
            Layout.fillWidth: true
            visible: root.settingsClient !== null
                && (root.settingsClient.lastOperationCode !== ""
                    || (root.settingsClient.status !== "READY"
                        && root.settingsClient.status !== "CONNECTING"))
            text: root.settingsClient === null
                ? qsTr("Settings service unavailable")
                : root.settingsClient.lastOperationCode === "STALE_REVISION"
                    ? qsTr("Preferences changed elsewhere. Reloading the current choice.")
                    : root.settingsClient.lastOperationCode !== ""
                        ? qsTr("Settings update result: %1").arg(root.settingsClient.lastOperationCode)
                        : qsTr("Settings service: %1").arg(root.settingsClient.status)
            wrapMode: Text.WordWrap
            color: "#f2b84b"
        }
    }
}
