import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Components: part number -> package. The selected one is edited on the right.
SplitView {
    objectName: "componentsPage"
    orientation: Qt.Horizontal

    ColumnLayout {
        SplitView.fillWidth: true
        SplitView.minimumWidth: 320
        spacing: Theme.spacing

        RowLayout {
            spacing: 2
            AddButton {
                objectName: "addComponent"
                tip: qsTr("添加元件")
                model: programDocument.components
                table: table
            }
            RemoveButton {
                objectName: "removeComponent"
                tip: qsTr("删除元件")
                itemName: qsTr("元件")
                model: programDocument.components
                table: table
            }
            ToolSeparator {}
            IconButton { glyph: Theme.icon.importFile; tip: qsTr("从元件库导入"); enabled: false }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("搜索") }
            TextField { Layout.preferredWidth: 180; selectByMouse: true }
        }

        DataTable {
            id: table
            objectName: "componentTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: programDocument.components
            selectFirstOnReset: true
            columns: [
                { title: qsTr("料号"), role: "id", width: 160 },
                { title: qsTr("封装"), role: "package", width: 110 },
                { title: qsTr("值"), role: "value", width: 80 },
                { title: qsTr("描述"), role: "description" },
                { title: qsTr("送料器数"), role: "feederCount", width: 70 },
                { title: qsTr("贴片位数"), role: "placementCount", width: 70 }
            ]
        }
    }

    ScrollView {
        SplitView.preferredWidth: 280
        SplitView.minimumWidth: 220
        clip: true
        contentWidth: availableWidth

        GroupBox {
            width: parent.width
            title: qsTr("元件")

            RecordForm {
                objectName: "componentForm"
                anchors.fill: parent
                model: programDocument.components
                row: table.currentIndex
                fields: [
                    { role: "id", label: qsTr("料号") },
                    { role: "package", label: qsTr("封装"), kind: "reference",
                      source: programDocument.packages },
                    { role: "value", label: qsTr("值") },
                    { role: "description", label: qsTr("描述") },
                    { role: "feederCount", label: qsTr("送料器数"), readOnly: true },
                    { role: "placementCount", label: qsTr("贴片位数"), readOnly: true }
                ]
            }
        }
    }
}
