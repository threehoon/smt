import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Feeder setup: slot -> component and pick location. Machine specific.
SplitView {
    objectName: "feedersPage"
    orientation: Qt.Horizontal

    ColumnLayout {
        SplitView.fillWidth: true
        SplitView.minimumWidth: 360
        spacing: Theme.spacing

        RowLayout {
            spacing: 2
            AddButton {
                tip: qsTr("添加送料器")
                model: programDocument.feeders
                table: table
            }
            RemoveButton {
                tip: qsTr("删除送料器")
                itemName: qsTr("送料器")
                model: programDocument.feeders
                table: table
            }
            ToolSeparator {}
            IconButton { glyph: Theme.icon.camera; tip: qsTr("相机移到取料位"); enabled: false }
            IconButton { glyph: Theme.icon.nozzle; tip: qsTr("吸嘴移到取料位"); enabled: false }
            IconButton { glyph: Theme.icon.target; tip: qsTr("用相机当前位置设定取料位"); enabled: false }
            ToolSeparator {}
            IconButton { glyph: Theme.icon.play; tip: qsTr("试取料"); enabled: false }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("机型") }
            EditField {
                Layout.preferredWidth: 140
                enabled: programDocument.loaded
                value: programDocument.machineModel
                onCommitted: programDocument.setMachineModel(text)
            }
        }

        DataTable {
            id: table
            objectName: "feederTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: programDocument.feeders
            selectFirstOnReset: true
            columns: [
                { title: qsTr("站位"), role: "slot", width: 70 },
                { title: "ID", role: "id", width: 70 },
                { title: qsTr("类型"), role: "type", width: 60 },
                { title: qsTr("元件"), role: "component" },
                { title: qsTr("取料 X"), role: "pickX", width: 75, decimals: 3 },
                { title: qsTr("取料 Y"), role: "pickY", width: 75, decimals: 3 },
                { title: qsTr("取料 Z"), role: "pickZ", width: 75, decimals: 3 },
                { title: qsTr("角度"), role: "rotation", width: 55, decimals: 2 },
                { title: qsTr("优先级"), role: "priority", width: 55 },
                { title: qsTr("启用"), role: "enabled", width: 45 }
            ]
        }
    }

    ScrollView {
        SplitView.preferredWidth: 280
        SplitView.minimumWidth: 220
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacing

            GroupBox {
                title: qsTr("送料器")
                Layout.fillWidth: true
                RecordForm {
                    anchors.fill: parent
                    model: programDocument.feeders
                    row: table.currentIndex
                    fields: [
                        { role: "id", label: "ID" },
                        { role: "slot", label: qsTr("站位") },
                        // Same order as FeederType: tape, tray, tube, bulk.
                        // An unknown type shows no choice until one is picked.
                        { role: "typeIndex", label: qsTr("类型"), kind: "choice",
                          choices: [qsTr("料带"), qsTr("料盘"), qsTr("料管"), qsTr("散料")] },
                        { role: "component", label: qsTr("元件"), kind: "reference",
                          source: programDocument.components },
                        { role: "pickX", label: qsTr("取料 X"), kind: "number" },
                        { role: "pickY", label: qsTr("取料 Y"), kind: "number" },
                        { role: "pickZ", label: qsTr("取料 Z"), kind: "number" },
                        { role: "rotation", label: qsTr("角度 (°)"), kind: "number" },
                        // Higher is used first among feeders of one component.
                        { role: "priority", label: qsTr("优先级"), kind: "integer" },
                        { role: "enabled", label: qsTr("启用"), kind: "bool" }
                    ]
                }
            }

            GroupBox {
                title: qsTr("参数")
                Layout.fillWidth: true

                ColumnLayout {
                    // typeIndex of the selected feeder; -1 = unknown type.
                    readonly property var feederType: table.currentIndex >= 0
                                                && programDocument.feeders.count >= 0
                        ? programDocument.feeders.get(table.currentIndex).typeIndex : undefined

                    anchors.fill: parent
                    spacing: Theme.spacing

                    RecordForm {
                        Layout.fillWidth: true
                        model: programDocument.feeders
                        row: table.currentIndex
                        fields: [
                            { role: "tapeWidth", label: qsTr("料带宽 (mm)"), kind: "number",
                              shownWhen: function(r) { return r.typeIndex === 0; } },
                            { role: "tapePitch", label: qsTr("料距 (mm)"), kind: "number",
                              shownWhen: function(r) { return r.typeIndex === 0; } },
                            { role: "tapeFeedCount", label: qsTr("每次送料数"), kind: "integer",
                              shownWhen: function(r) { return r.typeIndex === 0; } },
                            { role: "trayRows", label: qsTr("行数"), kind: "integer",
                              shownWhen: function(r) { return r.typeIndex === 1; } },
                            { role: "trayColumns", label: qsTr("列数"), kind: "integer",
                              shownWhen: function(r) { return r.typeIndex === 1; } },
                            { role: "trayRowPitch", label: qsTr("行距 (mm)"), kind: "number",
                              shownWhen: function(r) { return r.typeIndex === 1; } },
                            { role: "trayColumnPitch", label: qsTr("列距 (mm)"), kind: "number",
                              shownWhen: function(r) { return r.typeIndex === 1; } },
                            { role: "trayStartIndex", label: qsTr("起始穴位"), kind: "integer",
                              shownWhen: function(r) { return r.typeIndex === 1; } },
                            { role: "tubePitch", label: qsTr("间距 (mm)"), kind: "number",
                              shownWhen: function(r) { return r.typeIndex === 2; } }
                        ]
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.mutedText
                        visible: parent.feederType === 3 || parent.feederType === -1
                        text: parent.feederType === 3
                              ? qsTr("散料没有参数。")
                              : qsTr("本软件不认识此类型，参数按原样保留；选择类型后改为该类型的参数。")
                    }
                }
            }
        }
    }
}
