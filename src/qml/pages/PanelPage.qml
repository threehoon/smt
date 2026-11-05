import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Panel editor: outline size, panel fiducials and board instances.
ColumnLayout {
    objectName: "panelPage"
    spacing: Theme.spacing

    GroupBox {
        title: qsTr("拼板尺寸")
        Layout.fillWidth: true

        RowLayout {
            spacing: Theme.spacing
            enabled: programDocument.loaded
            Label { text: qsTr("宽 (mm)") }
            EditField {
                Layout.preferredWidth: 90
                value: programDocument.loaded ? programDocument.panelWidth : undefined
                decimals: 3
                onCommitted: programDocument.setPanelWidth(text)
            }
            Label { text: qsTr("长 (mm)") }
            EditField {
                Layout.preferredWidth: 90
                value: programDocument.loaded ? programDocument.panelHeight : undefined
                decimals: 3
                onCommitted: programDocument.setPanelHeight(text)
            }
            ToolSeparator {}
            Button { text: qsTr("按阵列生成子板..."); enabled: false }
        }
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Vertical

        GroupBox {
            title: qsTr("拼板 Mark")
            SplitView.preferredHeight: 220
            SplitView.minimumHeight: 110

            SplitView {
                anchors.fill: parent
                orientation: Qt.Horizontal

                ColumnLayout {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 300
                    spacing: Theme.spacing

                    RowLayout {
                        spacing: 2
                        AddButton {
                            tip: qsTr("添加 Mark")
                            model: programDocument.panelFiducials
                            table: fiducialTable
                        }
                        RemoveButton {
                            tip: qsTr("删除 Mark")
                            itemName: qsTr("Mark 点")
                            model: programDocument.panelFiducials
                            table: fiducialTable
                        }
                        ToolSeparator {}
                        IconButton { glyph: Theme.icon.camera; tip: qsTr("相机移到所选 Mark"); enabled: false }
                        IconButton { glyph: Theme.icon.target; tip: qsTr("用相机当前位置设定坐标"); enabled: false }
                        Item { Layout.fillWidth: true }
                    }

                    DataTable {
                        id: fiducialTable
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: programDocument.panelFiducials
                        columns: [
                            { title: "ID", role: "id", width: 80 },
                            { title: "X", role: "x", width: 90, decimals: 3 },
                            { title: "Y", role: "y", width: 90, decimals: 3 },
                            { title: qsTr("形状"), role: "shape", width: 80 },
                            { title: qsTr("尺寸"), role: "size", decimals: 3 }
                        ]
                    }
                }

                ScrollView {
                    SplitView.preferredWidth: 260
                    SplitView.minimumWidth: 200
                    clip: true
                    leftPadding: Theme.margin
                    contentWidth: availableWidth

                    RecordForm {
                        width: parent.width
                        model: programDocument.panelFiducials
                        row: fiducialTable.currentIndex
                        fields: [
                            { role: "id", label: "ID" },
                            { role: "x", label: "X (mm)", kind: "number" },
                            { role: "y", label: "Y (mm)", kind: "number" },
                            // Same order as FiducialShape: circle, square, cross.
                            { role: "shapeIndex", label: qsTr("形状"), kind: "choice",
                              choices: [qsTr("圆形"), qsTr("方形"), qsTr("十字")] },
                            { role: "size", label: qsTr("尺寸 (mm)"), kind: "number" }
                        ]
                    }
                }
            }
        }

        GroupBox {
            title: qsTr("子板实例")
            SplitView.fillHeight: true
            SplitView.minimumHeight: 150

            SplitView {
                anchors.fill: parent
                orientation: Qt.Horizontal

                ColumnLayout {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 300
                    spacing: Theme.spacing

                    RowLayout {
                        spacing: 2
                        AddButton {
                            tip: qsTr("添加子板")
                            model: programDocument.boardInstances
                            table: instanceTable
                        }
                        RemoveButton {
                            tip: qsTr("删除子板")
                            itemName: qsTr("子板实例")
                            model: programDocument.boardInstances
                            table: instanceTable
                        }
                        IconButton {
                            glyph: Theme.icon.moveUp
                            tip: qsTr("上移")
                            enabled: instanceTable.currentIndex > 0
                            onClicked: {
                                const row = programDocument.boardInstances.move(instanceTable.currentIndex, -1);
                                if (row >= 0)
                                    instanceTable.select(row);
                            }
                        }
                        IconButton {
                            glyph: Theme.icon.moveDown
                            tip: qsTr("下移")
                            enabled: instanceTable.currentIndex >= 0
                                     && instanceTable.currentIndex < programDocument.boardInstances.count - 1
                            onClicked: {
                                const row = programDocument.boardInstances.move(instanceTable.currentIndex, 1);
                                if (row >= 0)
                                    instanceTable.select(row);
                            }
                        }
                        ToolSeparator {}
                        IconButton { glyph: Theme.icon.camera; tip: qsTr("相机移到所选子板原点"); enabled: false }
                        Item { Layout.fillWidth: true }
                    }

                    DataTable {
                        id: instanceTable
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: programDocument.boardInstances
                        columns: [
                            { title: qsTr("实例"), role: "id", width: 60 },
                            { title: qsTr("单板"), role: "board" },
                            { title: "X", role: "x", width: 90, decimals: 3 },
                            { title: "Y", role: "y", width: 90, decimals: 3 },
                            { title: qsTr("角度"), role: "rotation", width: 70, decimals: 2 },
                            { title: qsTr("启用"), role: "enabled", width: 50 },
                            { title: qsTr("跳过位号"), role: "skips", width: 120 }
                        ]
                    }
                }

                ScrollView {
                    SplitView.preferredWidth: 260
                    SplitView.minimumWidth: 200
                    clip: true
                    leftPadding: Theme.margin
                    contentWidth: availableWidth

                    RecordForm {
                        width: parent.width
                        model: programDocument.boardInstances
                        row: instanceTable.currentIndex
                        fields: [
                            { role: "id", label: qsTr("实例") },
                            { role: "board", label: qsTr("单板"), kind: "reference",
                              source: programDocument.boards },
                            { role: "x", label: "X (mm)", kind: "number" },
                            { role: "y", label: "Y (mm)", kind: "number" },
                            { role: "rotation", label: qsTr("角度 (°)"), kind: "number" },
                            { role: "enabled", label: qsTr("启用"), kind: "bool" },
                            // Placement ids separated by commas or spaces.
                            { role: "skips", label: qsTr("跳过位号") },
                            { role: "placementCount", label: qsTr("贴片数"), readOnly: true }
                        ]
                    }
                }
            }
        }
    }
}
