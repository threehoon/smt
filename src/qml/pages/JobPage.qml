import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Production: run controls, board instances of the panel and the flattened
// placement list (instance x placement) with run status.
SplitView {
    objectName: "jobPage"
    orientation: Qt.Vertical

    GroupBox {
        title: qsTr("子板")
        SplitView.preferredHeight: 220
        SplitView.minimumHeight: 120

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing

            RowLayout {
                spacing: 2
                IconButton { glyph: Theme.icon.play; tip: qsTr("开始"); glyphColor: Theme.ok }
                IconButton { glyph: Theme.icon.step; tip: qsTr("单步") }
                IconButton { glyph: Theme.icon.pause; tip: qsTr("暂停") }
                IconButton { glyph: Theme.icon.stop; tip: qsTr("停止"); glyphColor: Theme.danger }
                ToolSeparator {}
                IconButton { glyph: Theme.icon.target; tip: qsTr("识别拼板 Mark") }
                IconButton { glyph: Theme.icon.camera; tip: qsTr("相机移到所选子板原点") }
                IconButton { glyph: Theme.icon.nozzle; tip: qsTr("吸嘴移到所选子板原点") }
                ToolSeparator {}
                IconButton {
                    glyph: Theme.icon.check
                    tip: qsTr("启用/禁用所选子板")
                    enabled: instanceTable.currentIndex >= 0
                    onClicked: {
                        const row = instanceTable.currentIndex;
                        const m = programDocument.boardInstances;
                        m.set(row, "enabled", !m.get(row).enabled);
                    }
                }
                IconButton { glyph: Theme.icon.clear; tip: qsTr("重置贴装状态") }
                Item { Layout.fillWidth: true }
            }

            DataTable {
                id: instanceTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: programDocument.boardInstances
                // 已贴 and 状态 come from the job runner, which does not exist yet.
                columns: [
                    { title: qsTr("实例"), role: "id", width: 60 },
                    { title: qsTr("单板"), role: "board" },
                    { title: "X", role: "x", width: 80, decimals: 3 },
                    { title: "Y", role: "y", width: 80, decimals: 3 },
                    { title: qsTr("角度"), role: "rotation", width: 60, decimals: 2 },
                    { title: qsTr("启用"), role: "enabled", width: 50 },
                    { title: qsTr("贴片数"), role: "placementCount", width: 60 },
                    { title: qsTr("已贴"), role: "placedCount", width: 60 },
                    { title: qsTr("状态"), role: "status", width: 80 }
                ]
            }
        }
    }

    GroupBox {
        title: qsTr("贴片位")
        SplitView.fillHeight: true
        SplitView.minimumHeight: 150

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing

            RowLayout {
                spacing: 2
                IconButton { glyph: Theme.icon.camera; tip: qsTr("相机移到所选贴片位") }
                IconButton { glyph: Theme.icon.nozzle; tip: qsTr("吸嘴移到所选贴片位") }
                ToolSeparator {}
                IconButton {
                    glyph: Theme.icon.check
                    tip: qsTr("跳过/恢复所选贴片位")
                    enabled: placementTable.currentIndex >= 0
                    onClicked: programDocument.jobPlacements.toggleSkip(placementTable.currentIndex)
                }
                Item { Layout.fillWidth: true }
                Label { text: qsTr("搜索") }
                TextField { Layout.preferredWidth: 180; selectByMouse: true }
            }

            DataTable {
                id: placementTable
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: programDocument.jobPlacements
                // X / Y / 角度 are board coordinates.
                columns: [
                    { title: qsTr("子板"), role: "instance", width: 50 },
                    { title: qsTr("位号"), role: "id", width: 70 },
                    { title: qsTr("元件"), role: "component" },
                    { title: qsTr("面"), role: "side", width: 50 },
                    { title: "X", role: "x", width: 80, decimals: 3 },
                    { title: "Y", role: "y", width: 80, decimals: 3 },
                    { title: qsTr("角度"), role: "rotation", width: 60, decimals: 2 },
                    { title: qsTr("送料器"), role: "feeder", width: 110 },
                    { title: qsTr("启用"), role: "enabled", width: 50 },
                    { title: qsTr("状态"), role: "status", width: 70 }
                ]
            }
        }
    }
}
