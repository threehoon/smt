import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "."

// Asks before a destructive action. Call ask(title, text, action); action runs
// when the user confirms with the button labelled `confirmText`.
Dialog {
    id: root

    property alias text: message.text
    property string confirmText: qsTr("删除")
    property var action: null

    function ask(title, text, action) {
        root.title = title;
        root.text = text;
        root.action = action;
        open();
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, Overlay.overlay ? Overlay.overlay.width - 40 : 440)

    onAccepted: {
        const run = action;
        action = null;
        if (run)
            run();
    }
    onRejected: action = null

    Label {
        id: message
        width: parent.width
        wrapMode: Text.Wrap
    }

    footer: DialogButtonBox {
        Button {
            text: root.confirmText
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        Button {
            text: qsTr("取消")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
