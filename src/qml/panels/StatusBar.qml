import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Window footer: status message, placement progress and machine position (DRO).
ToolBar {
    id: root

    readonly property string idleMessage: qsTr("就绪")
    property string message: idleMessage
    property int placedCount: 0
    property int totalCount: 0
    property bool machineOnline: false
    property real posX: 0
    property real posY: 0
    property real posZ: 0
    property real posC: 0

    // Shows feedback such as "已保存 x.smt.xml" for a few seconds.
    function show(text) {
        message = text;
        messageTimer.restart();
    }

    position: ToolBar.Footer

    Timer {
        id: messageTimer
        interval: 5000
        onTriggered: root.message = root.idleMessage
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.margin
        anchors.rightMargin: Theme.margin
        spacing: Theme.spacing * 2

        Label {
            objectName: "statusMessage"
            Layout.fillWidth: true
            text: root.message
            elide: Text.ElideRight
        }

        ToolSeparator {}

        Label {
            text: qsTr("贴装：%1 / %2").arg(root.placedCount).arg(root.totalCount)
        }

        ProgressBar {
            Layout.preferredWidth: 160
            from: 0
            to: Math.max(1, root.totalCount)
            value: root.placedCount
        }

        ToolSeparator {}

        Rectangle {
            Layout.preferredHeight: dro.implicitHeight + 6
            Layout.preferredWidth: dro.implicitWidth + 16
            color: root.machineOnline ? Theme.droOnline : Theme.droOffline
            border.color: Theme.gridLine

            Row {
                id: dro
                anchors.centerIn: parent
                spacing: 14

                Repeater {
                    model: [
                        { axis: "X", value: root.posX },
                        { axis: "Y", value: root.posY },
                        { axis: "Z", value: root.posZ },
                        { axis: "C", value: root.posC }
                    ]
                    Label {
                        text: modelData.axis + ": " + modelData.value.toFixed(3)
                        font.family: Theme.monoFont
                    }
                }
                Label {
                    text: qsTr("（未连接）")
                    color: Theme.mutedText
                    visible: !root.machineOnline
                }
            }
        }
    }
}
