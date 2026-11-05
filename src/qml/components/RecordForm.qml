import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "."

// Editors for the fields of one row of a ProgramListModel. Each change is
// written with model.set(row, role, value): one undoable edit per field, which
// the document refuses (and explains) if the program would become invalid.
//
// fields: [{ role: "x", label: "X", kind: "number", decimals: 4 }, ...]
//   kind: "text" (default), "number", "integer", "bool",
//         "choice"    with choices: [text, ...]; the role holds the index
//         "reference" with source: a list model; the role holds one of its ids
//   readOnly: true                        shown, not editable
//   shownWhen: function(record) { ... }   hide the field for some rows
// Editors are named "field_<role>" for tests.
ColumnLayout {
    id: root

    property var model: null
    property int row: -1
    property var fields: []
    property int labelWidth: 90

    // Roles of the row. Reading count re-evaluates after every edit.
    readonly property var record: model && model.count >= 0 && row >= 0 ? model.get(row) : ({})
    readonly property bool hasRecord: Object.keys(record).length > 0

    enabled: hasRecord
    spacing: Theme.spacing

    Repeater {
        model: root.fields

        RowLayout {
            id: line

            // Not modelData: the Repeater turns each entry into a QVariantMap,
            // which drops the shownWhen function.
            readonly property var field: root.fields[index]
            readonly property string kind: field.kind !== undefined ? field.kind : "text"
            readonly property bool editable: field.readOnly !== true
            readonly property var current: root.record[field.role]

            Layout.fillWidth: true
            visible: field.shownWhen === undefined || !root.hasRecord || field.shownWhen(root.record)
            spacing: Theme.spacing

            Label {
                Layout.preferredWidth: root.labelWidth
                text: line.field.label
                elide: Text.ElideRight
            }

            Loader {
                Layout.fillWidth: true
                sourceComponent: line.kind === "bool" ? boolEditor
                               : line.kind === "choice" ? choiceEditor
                               : line.kind === "reference" ? referenceEditor
                               : textEditor
            }

            Component {
                id: textEditor
                EditField {
                    // The row being typed into; the selection may change
                    // before the field loses focus.
                    property int editRow: -1
                    objectName: "field_" + line.field.role
                    readOnly: !line.editable
                    value: line.current
                    decimals: line.field.decimals !== undefined ? line.field.decimals
                            : line.kind === "number" ? 4 : -1
                    onDirtyChanged: if (dirty) editRow = root.row
                    onCommitted: root.model.set(editRow, line.field.role, text)
                }
            }

            Component {
                id: boolEditor
                CheckBox {
                    objectName: "field_" + line.field.role
                    enabled: line.editable
                    checked: line.current === true
                    onToggled: {
                        root.model.set(root.row, line.field.role, checked);
                        // Show the model again, also when the edit was refused.
                        checked = Qt.binding(function() { return line.current === true; });
                    }
                }
            }

            Component {
                id: choiceEditor
                ComboBox {
                    objectName: "field_" + line.field.role
                    enabled: line.editable
                    model: line.field.choices
                    currentIndex: line.current !== undefined ? line.current : -1
                    onActivated: {
                        root.model.set(root.row, line.field.role, index);
                        currentIndex = Qt.binding(function() {
                            return line.current !== undefined ? line.current : -1;
                        });
                    }
                }
            }

            Component {
                id: referenceEditor
                ComboBox {
                    function currentRow() {
                        return find(line.current !== undefined ? String(line.current) : "");
                    }

                    objectName: "field_" + line.field.role
                    enabled: line.editable
                    model: line.field.source
                    textRole: "id"
                    // count: re-evaluate when the referenced list changes.
                    currentIndex: count >= 0 ? currentRow() : -1
                    onActivated: {
                        root.model.set(root.row, line.field.role, textAt(index));
                        currentIndex = Qt.binding(function() { return count >= 0 ? currentRow() : -1; });
                    }
                }
            }
        }
    }
}
