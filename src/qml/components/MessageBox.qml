import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "."

// Modal message with optional details, e.g. the diagnostics of a file that
// could not be opened. Call show(title, text, details).
Dialog {
    id: root

    property alias text: message.text
    property alias details: detailText.text

    function show(title, text, details) {
        root.title = title;
        root.text = text;
        root.details = details;
        open();
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, Overlay.overlay ? Overlay.overlay.width - 40 : 560)
    standardButtons: Dialog.Ok

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        Label {
            id: message
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            visible: detailText.text.length > 0

            TextArea {
                id: detailText
                readOnly: true
                selectByMouse: true
                font.family: Theme.monoFont
                wrapMode: TextEdit.NoWrap
                background: Rectangle {
                    color: detailText.palette.base
                    border.color: Theme.gridLine
                }
            }
        }
    }
}
