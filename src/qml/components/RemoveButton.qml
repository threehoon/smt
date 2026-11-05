import QtQuick 2.14
import QtQuick.Controls 2.14
import "."

// Removes the row selected in `table` from `model` (a ProgramListModel).
// When other data goes with it (model.removalImpact), asks first. Undo brings
// everything back either way.
IconButton {
    id: root

    property var model: null
    property var table: null
    // Names the row in the question, e.g. "元件".
    property string itemName: qsTr("此项")

    function removeRow(row) {
        if (!model.remove(row))
            return;
        const count = model.rowCount();
        table.select(count === 0 ? -1 : Math.min(row, count - 1));
    }

    glyph: Theme.icon.remove
    enabled: model !== null && table !== null && table.currentIndex >= 0
    onClicked: {
        const row = table.currentIndex;
        const impact = model.removalImpact(row);
        if (impact === "") {
            removeRow(row);
            return;
        }
        const id = model.get(row).id;
        confirm.ask(qsTr("删除%1").arg(itemName),
                    qsTr("删除%1 %2？%3\n可以用“撤销”恢复。").arg(itemName).arg(id).arg(impact),
                    function() { root.removeRow(row); });
    }

    ConfirmDialog {
        id: confirm
        objectName: "removeConfirm"
        parent: Overlay.overlay
    }
}
