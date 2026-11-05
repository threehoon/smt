import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Import placements from a CAD placement file into one board.
// Open while programDocument.cadImport has a file loaded: the window loads a
// file with cadImport.open(url), this dialog shows how the file is read and
// what the import does, and closing it forgets the file.
Dialog {
    id: root
    objectName: "cadImportDialog"

    readonly property var importer: programDocument.cadImport

    function doImport() {
        if (importer.replace && importer.removedCount > 0) {
            confirm.ask(qsTr("替换贴片位"),
                        qsTr("单板 %1 上有 %2 个贴片位不在文件中，替换会删除它们及其子板跳过设置。可以撤销。是否继续？")
                            .arg(importer.boardChoices[importer.boardIndex])
                            .arg(importer.removedCount),
                        function() { root.importer.importPlacements(); });
        } else {
            importer.importPlacements();
        }
    }

    title: qsTr("导入贴片坐标 - %1").arg(importer.fileName)
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(980, Overlay.overlay ? Overlay.overlay.width - 40 : 980)
    height: Math.min(720, Overlay.overlay ? Overlay.overlay.height - 40 : 720)

    onClosed: importer.close()

    // The importer decides: a loaded file opens the dialog, an import or
    // close() closes it.
    Connections {
        target: root.importer
        onChanged: {
            if (root.importer.loaded && !root.visible) {
                tabs.currentIndex = 0;
                root.open();
            } else if (!root.importer.loaded && root.visible) {
                root.close();
            }
        }
    }

    ConfirmDialog {
        id: confirm
        objectName: "cadReplaceConfirm"
        confirmText: qsTr("替换")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // Nested layouts fill both ways by default; only the tables should
        // take the extra height.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: Theme.spacing

            GroupBox {
                title: qsTr("读取方式")
                Layout.fillHeight: true

                GridLayout {
                    columns: 2
                    columnSpacing: Theme.spacing
                    rowSpacing: 2

                    Label { text: qsTr("格式") }
                    Label {
                        objectName: "formatLabel"
                        text: root.importer.formatName
                    }

                    Label { text: qsTr("分隔符") }
                    ComboBox {
                        objectName: "delimiterChoice"
                        Layout.preferredWidth: 150
                        model: root.importer.delimiterChoices
                        currentIndex: root.importer.delimiterChoice
                        onActivated: {
                            root.importer.delimiterChoice = index;
                            currentIndex = Qt.binding(function() { return root.importer.delimiterChoice; });
                        }
                    }

                    Label { text: qsTr("表头行") }
                    RowLayout {
                        spacing: Theme.spacing
                        CheckBox {
                            objectName: "autoHeader"
                            text: qsTr("自动")
                            checked: root.importer.autoHeader
                            onToggled: {
                                root.importer.autoHeader = checked;
                                checked = Qt.binding(function() { return root.importer.autoHeader; });
                            }
                        }
                        SpinBox {
                            objectName: "headerLine"
                            Layout.preferredWidth: 110
                            from: 0
                            to: Math.max(0, root.importer.lineCount)
                            editable: true
                            value: root.importer.headerLine
                            textFromValue: function(v) { return v === 0 ? qsTr("无") : String(v); }
                            valueFromText: function(t) { return t === qsTr("无") ? 0 : parseInt(t); }
                            onValueModified: {
                                root.importer.headerLine = value;
                                value = Qt.binding(function() { return root.importer.headerLine; });
                            }
                        }
                    }

                    Label { text: qsTr("单位") }
                    ComboBox {
                        objectName: "unitChoice"
                        Layout.preferredWidth: 150
                        model: root.importer.unitChoices
                        currentIndex: root.importer.unitChoice
                        onActivated: {
                            root.importer.unitChoice = index;
                            currentIndex = Qt.binding(function() { return root.importer.unitChoice; });
                        }
                    }
                }
            }

            GroupBox {
                title: qsTr("列（* 为必选）")
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    columns: 2
                    columnSpacing: Theme.margin * 2
                    rowSpacing: 2

                    Repeater {
                        model: root.importer.fieldNames

                        RowLayout {
                            id: fieldRow
                            readonly property int field: index

                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            spacing: Theme.spacing

                            Label {
                                Layout.preferredWidth: 50
                                text: modelData
                            }
                            ComboBox {
                                objectName: "column_" + fieldRow.field
                                Layout.fillWidth: true
                                model: root.importer.columnChoices
                                currentIndex: root.importer.fieldColumns[fieldRow.field]
                                onActivated: {
                                    root.importer.setFieldColumn(fieldRow.field, index);
                                    currentIndex = Qt.binding(function() {
                                        return root.importer.fieldColumns[fieldRow.field];
                                    });
                                }
                            }
                        }
                    }
                }
            }

            GroupBox {
                title: qsTr("板原点")
                Layout.fillHeight: true

                GridLayout {
                    columns: 2
                    columnSpacing: Theme.spacing
                    rowSpacing: 2

                    Label { text: "X (mm)" }
                    EditField {
                        objectName: "originX"
                        Layout.preferredWidth: 100
                        value: root.importer.originX
                        decimals: 4
                        onCommitted: root.importer.setOriginX(text)
                    }
                    Label { text: "Y (mm)" }
                    EditField {
                        objectName: "originY"
                        Layout.preferredWidth: 100
                        value: root.importer.originY
                        decimals: 4
                        onCommitted: root.importer.setOriginY(text)
                    }
                    Label {
                        Layout.columnSpan: 2
                        Layout.preferredWidth: 180
                        text: qsTr("板左下角在文件坐标中的位置，从每个坐标中减去")
                        color: Theme.mutedText
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: Theme.spacing

            Label { text: qsTr("导入到单板") }
            ComboBox {
                objectName: "boardChoice"
                Layout.preferredWidth: 160
                model: root.importer.boardChoices
                currentIndex: root.importer.boardIndex
                onActivated: {
                    root.importer.boardIndex = index;
                    currentIndex = Qt.binding(function() { return root.importer.boardIndex; });
                }
            }
            ToolSeparator {}
            RadioButton {
                objectName: "mergeMode"
                text: qsTr("合并（更新同名位号，保留其他）")
                checked: !root.importer.replace
                onToggled: if (checked) root.importer.replace = false
            }
            RadioButton {
                objectName: "replaceMode"
                text: qsTr("替换（只保留文件中的位号）")
                checked: root.importer.replace
                onToggled: if (checked) root.importer.replace = true
            }
            Item { Layout.fillWidth: true }
        }

        TabBar {
            id: tabs
            objectName: "cadTabs"
            Layout.fillWidth: true
            TabButton {
                text: qsTr("贴片位 (%1)").arg(root.importer.rowCount)
                width: implicitWidth
            }
            TabButton {
                text: qsTr("元件匹配 (%1)").arg(root.importer.parts.count)
                width: implicitWidth
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            currentIndex: tabs.currentIndex

            DataTable {
                id: rowTable
                objectName: "cadRowTable"
                model: root.importer.rows
                emptyText: qsTr("没有数据行")
                columns: [
                    { title: qsTr("行"), role: "line", width: 50 },
                    { title: qsTr("位号"), role: "designator", width: 80 },
                    { title: "X", role: "x", width: 80, decimals: 3 },
                    { title: "Y", role: "y", width: 80, decimals: 3 },
                    { title: qsTr("角度"), role: "rotation", width: 60, decimals: 2 },
                    { title: qsTr("面"), role: "side", width: 50 },
                    { title: qsTr("封装"), role: "footprint", width: 130 },
                    { title: qsTr("值"), role: "value", width: 90 },
                    { title: qsTr("元件"), role: "component", width: 140 },
                    { title: qsTr("结果"), role: "status" }
                ]
            }

            SplitView {
                orientation: Qt.Horizontal

                DataTable {
                    id: partTable
                    objectName: "cadPartTable"
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 300
                    model: root.importer.parts
                    selectFirstOnReset: true
                    emptyText: qsTr("没有可导入的行")
                    columns: [
                        { title: qsTr("封装"), role: "footprint", width: 130 },
                        { title: qsTr("值"), role: "value", width: 90 },
                        { title: qsTr("数量"), role: "count", width: 50 },
                        { title: qsTr("元件"), role: "component", width: 150 },
                        { title: qsTr("元件的封装"), role: "package", width: 120 },
                        { title: qsTr("位号"), role: "designators" }
                    ]
                }

                ScrollView {
                    SplitView.preferredWidth: 280
                    SplitView.minimumWidth: 220
                    clip: true
                    leftPadding: Theme.margin
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacing

                        RecordForm {
                            objectName: "cadPartForm"
                            Layout.fillWidth: true
                            model: root.importer.parts
                            row: partTable.currentIndex
                            fields: [
                                { role: "footprint", label: qsTr("封装"), readOnly: true },
                                { role: "value", label: qsTr("值"), readOnly: true },
                                { role: "componentIndex", label: qsTr("元件"), kind: "choice",
                                  choices: root.importer.componentChoices },
                                { role: "packageIndex", label: qsTr("封装"), kind: "choice",
                                  choices: root.importer.packageChoices,
                                  shownWhen: function(r) { return r.createsComponent === true; } }
                            ]
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("选“新建”时按封装名和值生成元件；封装名与已有封装相同时直接使用该封装。")
                            color: Theme.mutedText
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        Label {
            objectName: "cadSummary"
            Layout.fillWidth: true
            text: root.importer.summaryText
            wrapMode: Text.Wrap
        }
    }

    footer: DialogButtonBox {
        Button {
            objectName: "cadImportButton"
            text: qsTr("导入")
            enabled: root.importer.canImport
            // Not AcceptRole: the dialog stays open if the import is refused.
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
        }
        Button {
            text: qsTr("取消")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        onApplied: root.doImport()
    }
}
