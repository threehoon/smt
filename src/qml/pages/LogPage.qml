import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Application log viewer.
ColumnLayout {
    spacing: Theme.spacing

    RowLayout {
        spacing: Theme.spacing
        Label { text: qsTr("级别") }
        ComboBox {
            model: [qsTr("全部"), qsTr("信息"), qsTr("警告"), qsTr("错误")]
        }
        Item { Layout.fillWidth: true }
        IconButton {
            glyph: Theme.icon.clear
            tip: qsTr("清空")
            onClicked: logText.clear()
        }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        TextArea {
            id: logText
            readOnly: true
            selectByMouse: true
            font.family: Theme.monoFont
            wrapMode: TextEdit.NoWrap
            background: Rectangle {
                color: logText.palette.base
                border.color: Theme.gridLine
            }
        }
    }
}
