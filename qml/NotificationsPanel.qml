import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var client
    required property real windowWidth
    required property real windowHeight

    implicitWidth: 40
    implicitHeight: 40
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Notifications")

    function messageTitle(key) {
        switch (key) {
        case "settings.telemetry_consent.applied":
            return qsTr("Preference updated")
        default:
            return qsTr("System notification")
        }
    }

    function messageBody(key) {
        switch (key) {
        case "settings.telemetry_consent.enabled":
            return qsTr("Product telemetry participation was turned on.")
        case "settings.telemetry_consent.disabled":
            return qsTr("Product telemetry participation was turned off.")
        default:
            return qsTr("This notification’s details are not available in this version.")
        }
    }

    function formattedTime(value) {
        const parsed = Date.parse(value)
        if (!Number.isFinite(parsed))
            return qsTr("Time unavailable")
        return Qt.formatDateTime(new Date(parsed), Qt.DefaultLocaleShortDate)
    }

    ToolButton {
        id: bellButton
        objectName: "notificationsButton"
        anchors.fill: parent
        text: "🔔"
        Accessible.id: "notificationsButton"
        Accessible.name: root.client === null
            ? qsTr("Notifications, unavailable")
            : root.client.unreadCount > 0
                ? qsTr("Notifications, %1 unread").arg(root.client.unreadCount)
                : qsTr("Notifications")
        Accessible.description: qsTr("Open notification history")
        onClicked: {
            historyPopup.open()
            if (root.client !== null)
                root.client.refresh()
        }

        contentItem: Text {
            text: "🔔"
            color: "#e5e7eb"
            font.pixelSize: 23
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: bellButton.down ? "#343944" : bellButton.hovered ? "#292d36" : "transparent"
        }

        Rectangle {
            objectName: "notificationsUnreadBadge"
            visible: root.client !== null && root.client.unreadCount > 0
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 1
            anchors.topMargin: 1
            width: Math.max(17, badgeLabel.implicitWidth + 8)
            height: 17
            radius: 9
            color: "#ed1c24"

            Text {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.client !== null && root.client.unreadCount > 99
                    ? "99+"
                    : root.client === null ? "" : String(root.client.unreadCount)
                color: "white"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }
    }

    Popup {
        id: historyPopup
        objectName: "notificationsHistoryPopup"
        x: Math.max(12, root.windowWidth - width - 24)
        y: 76
        width: Math.min(420, root.windowWidth - 24)
        height: Math.min(560, root.windowHeight - 96)
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        background: Rectangle {
            color: "#191c23"
            border.color: "#343944"
            border.width: 1
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 18

                Label {
                    text: qsTr("Notifications")
                    Accessible.role: Accessible.Heading
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    color: "#f4f4f5"
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    objectName: "notificationsRefreshButton"
                    text: qsTr("Refresh")
                    enabled: root.client !== null
                    Accessible.name: qsTr("Refresh notification history")
                    onClicked: root.client.refresh()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#343944"
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    visible: root.client !== null && !root.client.ready
                        && (root.client.status === "CONNECTING"
                            || root.client.status === "RECONCILING"
                            || root.client.status === "SAVING")

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: visible
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.client !== null && root.client.status === "SAVING"
                            ? qsTr("Updating notification…")
                            : qsTr("Loading notification history…")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "#b5b8c2"
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    visible: root.client === null
                        || (!root.client.ready && root.client.status !== "CONNECTING"
                            && root.client.status !== "RECONCILING"
                            && root.client.status !== "SAVING")

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Notification history is unavailable. Try refreshing in a moment.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "#f2b84b"
                    }
                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Retry")
                        enabled: root.client !== null
                        onClicked: root.client.refresh()
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    visible: root.client !== null && root.client.ready && root.client.count === 0
                    text: qsTr("No notifications yet.")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: "#b5b8c2"
                }

                ListView {
                    id: notificationList
                    objectName: "notificationHistoryList"
                    anchors.fill: parent
                    clip: true
                    model: root.client
                    spacing: 1
                    visible: root.client !== null && root.client.ready && count > 0
                    Accessible.name: qsTr("Notification history")

                    delegate: Rectangle {
                        objectName: "notificationHistoryItem"
                        width: notificationList.width
                        height: card.implicitHeight + 24
                        color: isRead ? "#191c23" : "#20242d"

                        ColumnLayout {
                            id: card
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            anchors.topMargin: 12
                            anchors.bottomMargin: 12
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: root.messageTitle(titleMessageKey)
                                    Accessible.role: Accessible.Heading
                                    font.pixelSize: 15
                                    font.weight: isRead ? Font.Medium : Font.DemiBold
                                    color: "#f4f4f5"
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    visible: critical
                                    text: qsTr("Important")
                                    color: "#ffb7b7"
                                    font.pixelSize: 11
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.messageBody(bodyMessageKey)
                                wrapMode: Text.WordWrap
                                color: "#c2c5ce"
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: root.formattedTime(createdAtUtc)
                                    Accessible.name: qsTr("Received %1").arg(text)
                                    color: "#9297a3"
                                    font.pixelSize: 11
                                }
                                Label {
                                    visible: isRead
                                    text: qsTr("Read")
                                    color: "#9297a3"
                                    font.pixelSize: 11
                                }
                                Button {
                                    objectName: "notificationsMarkReadButton"
                                    visible: !isRead
                                    enabled: root.client !== null && root.client.ready
                                    text: qsTr("Mark as read")
                                    Accessible.name: qsTr("Mark notification as read")
                                    onClicked: {
                                        if (root.client !== null)
                                            root.client.markRead(notificationId)
                                    }
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { }
                }
            }
        }
    }
}
