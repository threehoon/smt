import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Vision pipelines: list on the left, parameters of the selected one on the right.
SplitView {
    orientation: Qt.Horizontal

    ColumnLayout {
        SplitView.preferredWidth: 240
        SplitView.minimumWidth: 180
        spacing: Theme.spacing

        RowLayout {
            spacing: 2
            IconButton { glyph: Theme.icon.add; tip: qsTr("新建视觉方案") }
            IconButton { glyph: Theme.icon.remove; tip: qsTr("删除视觉方案") }
            Item { Layout.fillWidth: true }
        }

        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: [
                { title: qsTr("名称"), role: "name" },
                { title: qsTr("用途"), role: "usage", width: 80 }
            ]
        }
    }

    Placeholder {
        SplitView.fillWidth: true
        text: qsTr("选择左侧视觉方案以编辑参数")
    }
}
