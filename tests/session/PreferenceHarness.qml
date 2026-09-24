import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 640
    height: 240
    visible: true

    ProductTelemetryConsent {
        anchors.centerIn: parent
        objectName: "productTelemetryConsentPreference"
        settingsClient: sessionSettingsClient
    }
}
