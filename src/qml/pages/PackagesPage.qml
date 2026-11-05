import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Packages: outline plus pick / place / vision parameters of the selected one.
SplitView {
    id: root
    objectName: "packagesPage"
    orientation: Qt.Horizontal

    ColumnLayout {
        SplitView.fillWidth: true
        SplitView.minimumWidth: 260
        spacing: Theme.spacing

        RowLayout {
            spacing: 2
            AddButton {
                tip: qsTr("添加封装")
                model: programDocument.packages
                table: table
            }
            RemoveButton {
                tip: qsTr("删除封装")
                itemName: qsTr("封装")
                model: programDocument.packages
                table: table
            }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("搜索") }
            TextField { Layout.preferredWidth: 140; selectByMouse: true }
        }

        DataTable {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: programDocument.packages
            selectFirstOnReset: true
            columns: [
                { title: "ID", role: "id" },
                { title: qsTr("长"), role: "bodyLength", width: 60, decimals: 3 },
                { title: qsTr("宽"), role: "bodyWidth", width: 60, decimals: 3 },
                { title: qsTr("高"), role: "height", width: 60, decimals: 3 },
                { title: qsTr("引脚"), role: "pinCount", width: 50 }
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
                title: qsTr("外形 (mm)")
                Layout.fillWidth: true
                RecordForm {
                    anchors.fill: parent
                    model: programDocument.packages
                    row: table.currentIndex
                    fields: [
                        { role: "id", label: "ID" },
                        { role: "bodyLength", label: qsTr("长 (X)"), kind: "number" },
                        { role: "bodyWidth", label: qsTr("宽 (Y)"), kind: "number" },
                        { role: "height", label: qsTr("高"), kind: "number" },
                        { role: "pinCount", label: qsTr("引脚数"), kind: "integer" }
                    ]
                }
            }

            GroupBox {
                title: qsTr("取料")
                Layout.fillWidth: true
                RecordForm {
                    anchors.fill: parent
                    model: programDocument.packages
                    row: table.currentIndex
                    fields: [
                        // Comma separated; empty = any nozzle.
                        { role: "nozzles", label: qsTr("吸嘴型号") },
                        { role: "pickDepth", label: qsTr("下压量 (mm)"), kind: "number" },
                        { role: "vacuumCheck", label: qsTr("真空检测"), kind: "bool" }
                    ]
                }
            }

            GroupBox {
                title: qsTr("贴装")
                Layout.fillWidth: true
                RecordForm {
                    anchors.fill: parent
                    model: programDocument.packages
                    row: table.currentIndex
                    fields: [
                        { role: "placeDepth", label: qsTr("下压量 (mm)"), kind: "number" },
                        { role: "speed", label: qsTr("速度 (%)"), kind: "integer" }
                    ]
                }
            }

            GroupBox {
                title: qsTr("视觉")
                Layout.fillWidth: true
                RecordForm {
                    anchors.fill: parent
                    model: programDocument.packages
                    row: table.currentIndex
                    fields: [
                        // Same order as VisionMethod: none, bottom, top.
                        { role: "visionMethod", label: qsTr("方式"), kind: "choice",
                          choices: [qsTr("不识别"), qsTr("底部相机"), qsTr("顶部相机")] },
                        { role: "tolerance", label: qsTr("容差 (mm)"), kind: "number" }
                    ]
                }
            }
        }
    }
}
