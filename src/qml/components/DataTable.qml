import QtQuick 2.14
import QtQuick.Controls 2.14
import "."

// Table display: fixed header row plus a ListView body. Editing happens in a
// form next to it (RecordForm), bound to currentIndex.
//
// columns: [{ title: "X", role: "x", width: 70, decimals: 3 }, ...]
// A column without `width` stretches to fill the remaining space.
// `decimals` formats a numeric value with that many fraction digits.
// Booleans are shown as 是 / 否.
// `model` is any list model whose role names match the columns' `role`.
Control {
    id: root

    property var columns: []
    property alias model: view.model
    property alias currentIndex: view.currentIndex
    property string emptyText: qsTr("无数据")
    // After the model resets (e.g. another program was opened), select the
    // first row, or nothing when the model is empty.
    property bool selectFirstOnReset: false
    readonly property int minStretchWidth: 80

    readonly property real fixedWidth: {
        let sum = 0;
        for (let i = 0; i < columns.length; ++i)
            sum += columns[i].width !== undefined ? columns[i].width : 0;
        return sum;
    }
    readonly property int stretchCount: {
        let n = 0;
        for (let i = 0; i < columns.length; ++i)
            if (columns[i].width === undefined)
                ++n;
        return n;
    }
    readonly property real stretchWidth: stretchCount === 0 ? 0
        : Math.max(minStretchWidth, (view.width - fixedWidth) / stretchCount)
    readonly property real tableWidth: fixedWidth + stretchWidth * stretchCount

    // Selects a row (-1: none) and scrolls it into view.
    function select(row) {
        view.currentIndex = row;
        if (row >= 0)
            view.positionViewAtIndex(row, ListView.Contain);
    }

    function columnWidth(i) {
        return columns[i].width !== undefined ? columns[i].width : stretchWidth;
    }

    function formatCell(value, column) {
        if (value === undefined || value === null)
            return "";
        if (typeof value === "boolean")
            return value ? qsTr("是") : qsTr("否");
        if (typeof value === "number" && column.decimals !== undefined)
            return value.toFixed(column.decimals);
        return String(value);
    }

    Connections {
        target: root.selectFirstOnReset ? view.model : null
        ignoreUnknownSignals: true
        onModelReset: view.currentIndex = view.model.rowCount() > 0 ? 0 : -1
    }

    padding: 1

    background: Rectangle {
        color: root.palette.base
        border.color: Theme.gridLine
    }

    contentItem: Item {
        ListView {
            id: view
            anchors.fill: parent
            clip: true
            contentWidth: root.tableWidth
            flickableDirection: Flickable.AutoFlickIfNeeded
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: -1
            headerPositioning: ListView.OverlayHeader

            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            header: Row {
                z: 2
                Repeater {
                    model: root.columns
                    Rectangle {
                        width: root.columnWidth(index)
                        height: Theme.rowHeight
                        color: Theme.headerBackground

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            text: modelData.title
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.gridLine
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.gridLine
                        }
                    }
                }
            }

            delegate: Rectangle {
                id: row

                // Role values of this row, keyed by role name.
                readonly property var rowData: model

                width: root.tableWidth
                height: Theme.rowHeight
                color: ListView.isCurrentItem ? root.palette.highlight
                     : index % 2 ? Theme.alternateRow : root.palette.base

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: root.columns
                        Item {
                            width: root.columnWidth(index)
                            height: row.height

                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                text: root.formatCell(row.rowData[modelData.role], modelData)
                                color: row.ListView.isCurrentItem ? root.palette.highlightedText
                                                                   : root.palette.text
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Rectangle {
                                anchors.right: parent.right
                                width: 1
                                height: parent.height
                                color: Theme.gridLine
                                opacity: 0.6
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        // Take focus first: a form field being typed into
                        // commits to the row it belongs to before the
                        // selection changes.
                        row.forceActiveFocus();
                        view.currentIndex = index;
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: view.count === 0
            text: root.emptyText
            color: Theme.mutedText
        }
    }
}
