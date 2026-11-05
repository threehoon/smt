import QtQuick 2.14
import "."

// Adds a row to `model` (a ProgramListModel) and selects it in `table`.
IconButton {
    id: root

    property var model: null
    property var table: null

    glyph: Theme.icon.add
    enabled: model !== null && model.canAdd
    onClicked: {
        const row = model.add();
        if (row >= 0 && table)
            table.select(row);
    }
}
