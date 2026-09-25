import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 640
    height: 280
    visible: true

    ToastNotificationsPreference {
        anchors.centerIn: parent
        objectName: "toastNotificationsPreference"
        client: sessionToastNotificationsClient
    }
}
