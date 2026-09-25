import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 800
    height: 600
    visible: false

    NotificationsPanel {
        objectName: "notificationsPanel"
        anchors.top: parent.top
        anchors.right: parent.right
        client: sessionNotificationsClient
        windowWidth: parent.width
        windowHeight: parent.height
    }
}
