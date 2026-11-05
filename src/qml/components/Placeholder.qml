import QtQuick 2.14
import QtQuick.Controls 2.14
import "."

// Centered gray hint for areas whose content is not implemented yet.
Item {
    property alias text: label.text

    implicitWidth: label.implicitWidth + 20
    implicitHeight: label.implicitHeight + 20

    Label {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, parent.width - 20)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: Theme.mutedText
    }
}
