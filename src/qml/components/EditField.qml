import QtQuick 2.14
import QtQuick.Controls 2.14

// Text field for one value of the program. Shows `value` (numbers with
// `decimals` fraction digits), and emits committed(text) when the user typed
// something and pressed Enter or left the field. The receiver applies it as an
// edit; whether accepted or not, the field then shows `value` again, so a
// rejected input does not stay on screen. Escape discards the typing.
TextField {
    id: root

    property var value
    property int decimals: -1  // -1: show the value as it is
    // True from the first keystroke until the text is committed or discarded.
    property bool dirty: false

    signal committed(string text)

    function display(v) {
        if (v === undefined || v === null)
            return "";
        if (typeof v === "number" && decimals >= 0)
            return v.toFixed(decimals);
        return String(v);
    }

    function refresh() {
        dirty = false;
        text = display(value);
    }

    selectByMouse: true
    // While typing, a model update (another field, undo) must not overwrite
    // the input; it is shown once the field is committed or discarded.
    onValueChanged: if (!dirty) refresh()
    onDecimalsChanged: if (!dirty) refresh()
    Component.onCompleted: refresh()

    onTextEdited: dirty = true
    onEditingFinished: {
        if (!dirty)
            return;
        const typed = text;
        dirty = false;
        committed(typed);
        refresh();
    }
    Keys.onEscapePressed: {
        if (dirty) {
            refresh();
            event.accepted = true;
        } else {
            event.accepted = false;
        }
    }
}
