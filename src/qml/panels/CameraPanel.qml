import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Camera selector and live view. No camera is wired in yet, so the view
// shows the reticle over a black background.
GroupBox {
    id: root
    title: qsTr("相机")

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        ComboBox {
            Layout.fillWidth: true
            enabled: count > 0
            model: []
            displayText: count > 0 ? currentText : qsTr("未连接相机")
        }

        Rectangle {
            id: viewport
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.cameraBackground
            clip: true

            // Reticle: crosshair through the center plus a ring.
            Rectangle {
                width: 1
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.reticle
                opacity: 0.7
            }
            Rectangle {
                width: parent.width
                height: 1
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.reticle
                opacity: 0.7
            }
            Rectangle {
                readonly property real size: Math.min(viewport.width, viewport.height) * 0.3
                width: size
                height: size
                radius: size / 2
                anchors.centerIn: parent
                color: "transparent"
                border.color: Theme.reticle
                opacity: 0.7
            }

            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 8
                text: qsTr("无图像")
                color: Theme.mutedText
            }
        }
    }
}
