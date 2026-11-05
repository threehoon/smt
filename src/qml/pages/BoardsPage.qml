import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Board definitions (top) and the selected board's placements, fiducials and
// bad marks (bottom). Each table has its form on the right.
SplitView {
    id: root
    objectName: "boardsPage"
    orientation: Qt.Vertical

    GroupBox {
        title: qsTr("单板")
        SplitView.preferredHeight: 200
        SplitView.minimumHeight: 120

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
                        tip: qsTr("新建单板")
                        model: programDocument.boards
                        table: boardTable
                    }
                    RemoveButton {
                        tip: qsTr("删除单板")
                        itemName: qsTr("单板")
                        model: programDocument.boards
                        table: boardTable
                    }
                    ToolSeparator {}
                    IconButton {
                        objectName: "boardCadImportButton"
                        glyph: Theme.icon.importFile
                        tip: qsTr("从 CAD 坐标文件导入到所选单板")
                        enabled: programDocument.loaded && boardTable.currentIndex >= 0
                        onClicked: programDocument.cadImport.request(boardTable.currentIndex)
                    }
                    Item { Layout.fillWidth: true }
                }

                DataTable {
                    id: boardTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: programDocument.boards
                    selectFirstOnReset: true
                    columns: [
                        { title: "ID", role: "id" },
                        { title: qsTr("宽"), role: "width", width: 80, decimals: 3 },
                        { title: qsTr("长"), role: "height", width: 80, decimals: 3 },
                        { title: qsTr("厚"), role: "thickness", width: 70, decimals: 3 },
                        { title: qsTr("贴片位"), role: "placementCount", width: 70 },
                        { title: qsTr("Mark"), role: "fiducialCount", width: 60 }
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
                    model: programDocument.boards
                    row: boardTable.currentIndex
                    fields: [
                        { role: "id", label: "ID" },
                        { role: "width", label: qsTr("宽 (mm)"), kind: "number" },
                        { role: "height", label: qsTr("长 (mm)"), kind: "number" },
                        { role: "thickness", label: qsTr("厚 (mm)"), kind: "number" }
                    ]
                }
            }
        }
    }

    GroupBox {
        title: qsTr("单板内容")
        SplitView.fillHeight: true
        SplitView.minimumHeight: 150

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing

            TabBar {
                id: tabs
                Layout.fillWidth: true
                TabButton { text: qsTr("贴片位"); width: implicitWidth }
                TabButton { text: qsTr("Mark 点"); width: implicitWidth }
                TabButton { text: qsTr("坏板标记"); width: implicitWidth }
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                ColumnLayout {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 300
                    spacing: Theme.spacing

                    RowLayout {
                        spacing: 2
                        AddButton {
                            tip: qsTr("添加")
                            model: content.currentModel
                            table: content.currentTable
                        }
                        RemoveButton {
                            tip: qsTr("删除")
                            itemName: [qsTr("贴片位"), qsTr("Mark 点"), qsTr("坏板标记")][tabs.currentIndex]
                            model: content.currentModel
                            table: content.currentTable
                        }
                        ToolSeparator {}
                        IconButton {
                            glyph: Theme.icon.camera
                            tip: qsTr("相机移到所选位置")
                            enabled: false
                        }
                        IconButton {
                            glyph: Theme.icon.target
                            tip: qsTr("用相机当前位置设定坐标")
                            enabled: false
                        }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("搜索"); visible: tabs.currentIndex === 0 }
                        TextField {
                            Layout.preferredWidth: 180
                            selectByMouse: true
                            visible: tabs.currentIndex === 0
                        }
                    }

                    StackLayout {
                        id: content

                        readonly property var currentModel:
                            [programDocument.boardPlacements, programDocument.boardFiducials,
                             programDocument.boardBadMarks][currentIndex]
                        readonly property var currentTable:
                            [placementTable, fiducialTable, badMarkTable][currentIndex]

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: tabs.currentIndex

                        DataTable {
                            id: placementTable
                            model: programDocument.boardPlacements
                            columns: [
                                { title: qsTr("位号"), role: "id", width: 80 },
                                { title: qsTr("元件"), role: "component" },
                                { title: qsTr("面"), role: "side", width: 50 },
                                { title: "X", role: "x", width: 80, decimals: 3 },
                                { title: "Y", role: "y", width: 80, decimals: 3 },
                                { title: qsTr("角度"), role: "rotation", width: 60, decimals: 2 },
                                { title: qsTr("启用"), role: "enabled", width: 50 }
                            ]
                        }
                        DataTable {
                            id: fiducialTable
                            model: programDocument.boardFiducials
                            columns: [
                                { title: "ID", role: "id", width: 80 },
                                { title: "X", role: "x", width: 90, decimals: 3 },
                                { title: "Y", role: "y", width: 90, decimals: 3 },
                                { title: qsTr("形状"), role: "shape", width: 80 },
                                { title: qsTr("尺寸"), role: "size", decimals: 3 }
                            ]
                        }
                        DataTable {
                            id: badMarkTable
                            model: programDocument.boardBadMarks
                            columns: [
                                { title: "ID", role: "id", width: 80 },
                                { title: "X", role: "x", width: 90, decimals: 3 },
                                { title: "Y", role: "y", decimals: 3 }
                            ]
                        }
                    }
                }

                ScrollView {
                    SplitView.preferredWidth: 260
                    SplitView.minimumWidth: 200
                    clip: true
                    leftPadding: Theme.margin
                    contentWidth: availableWidth

                    StackLayout {
                        width: parent.width
                        currentIndex: tabs.currentIndex

                        RecordForm {
                            model: programDocument.boardPlacements
                            row: placementTable.currentIndex
                            fields: [
                                { role: "id", label: qsTr("位号") },
                                { role: "component", label: qsTr("元件"), kind: "reference",
                                  source: programDocument.components },
                                // Same order as Side: top, bottom.
                                { role: "sideIndex", label: qsTr("面"), kind: "choice",
                                  choices: [qsTr("顶面"), qsTr("底面")] },
                                { role: "x", label: "X (mm)", kind: "number" },
                                { role: "y", label: "Y (mm)", kind: "number" },
                                { role: "rotation", label: qsTr("角度 (°)"), kind: "number" },
                                { role: "enabled", label: qsTr("贴装"), kind: "bool" }
                            ]
                        }
                        RecordForm {
                            model: programDocument.boardFiducials
                            row: fiducialTable.currentIndex
                            fields: root.fiducialFields
                        }
                        RecordForm {
                            model: programDocument.boardBadMarks
                            row: badMarkTable.currentIndex
                            fields: [
                                { role: "id", label: "ID" },
                                { role: "x", label: "X (mm)", kind: "number" },
                                { role: "y", label: "Y (mm)", kind: "number" }
                            ]
                        }
                    }
                }
            }
        }
    }

    // Same order as FiducialShape: circle, square, cross.
    readonly property var fiducialFields: [
        { role: "id", label: "ID" },
        { role: "x", label: "X (mm)", kind: "number" },
        { role: "y", label: "Y (mm)", kind: "number" },
        { role: "shapeIndex", label: qsTr("形状"), kind: "choice",
          choices: [qsTr("圆形"), qsTr("方形"), qsTr("十字")] },
        { role: "size", label: qsTr("尺寸 (mm)"), kind: "number" }
    ]

    // The three content tables show the board selected above.
    Binding {
        target: programDocument.boardPlacements
        property: "boardIndex"
        value: boardTable.currentIndex
    }
    Binding {
        target: programDocument.boardFiducials
        property: "boardIndex"
        value: boardTable.currentIndex
    }
    Binding {
        target: programDocument.boardBadMarks
        property: "boardIndex"
        value: boardTable.currentIndex
    }
}
