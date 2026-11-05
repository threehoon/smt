import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Manual jog: X/Y/Z/C moves, park positions, step size and speed.
// Buttons are not wired to the machine yet.
Item {
    id: root

    readonly property var stepSizes: [10, 1, 0.1, 0.01, 0.001]
    property real stepSize: 1

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Theme.margin
        spacing: Theme.spacing * 2

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: Theme.spacing

            JogButton { glyph: Theme.icon.home; tip: qsTr("回原点") }
            JogButton { glyph: Theme.icon.power; tip: qsTr("机器上电/断电"); glyphColor: Theme.ok }
        }

        // Cells are placed by row/column; GridLayout drops empty spacer items.
        GridLayout {
            Layout.alignment: Qt.AlignTop
            rowSpacing: Theme.spacing
            columnSpacing: Theme.spacing

            Label {
                Layout.row: 0; Layout.column: 0; Layout.columnSpan: 3
                Layout.alignment: Qt.AlignHCenter
                text: "X / Y"; font.bold: true
            }
            Label {
                Layout.row: 0; Layout.column: 3; Layout.leftMargin: Theme.spacing * 2
                Layout.alignment: Qt.AlignHCenter
                text: "Z"; font.bold: true
            }

            JogButton { Layout.row: 1; Layout.column: 1; glyph: Theme.icon.arrowUp; tip: qsTr("Y+") }
            JogButton { Layout.row: 2; Layout.column: 0; glyph: Theme.icon.arrowLeft; tip: qsTr("X-") }
            JogButton { Layout.row: 2; Layout.column: 1; text: "P"; tip: qsTr("X/Y 移到停靠位") }
            JogButton { Layout.row: 2; Layout.column: 2; glyph: Theme.icon.arrowRight; tip: qsTr("X+") }
            JogButton { Layout.row: 3; Layout.column: 1; glyph: Theme.icon.arrowDown; tip: qsTr("Y-") }

            JogButton {
                Layout.row: 1; Layout.column: 3; Layout.leftMargin: Theme.spacing * 2
                glyph: Theme.icon.arrowUp; tip: qsTr("Z+（抬起）")
            }
            JogButton {
                Layout.row: 2; Layout.column: 3; Layout.leftMargin: Theme.spacing * 2
                text: "P"; tip: qsTr("Z 移到安全高度")
            }
            JogButton {
                Layout.row: 3; Layout.column: 3; Layout.leftMargin: Theme.spacing * 2
                glyph: Theme.icon.arrowDown; tip: qsTr("Z-（下降）")
            }

            JogButton {
                Layout.row: 4; Layout.column: 0
                glyph: Theme.icon.rotate; mirrorGlyph: true; tip: qsTr("C+（逆时针）")
            }
            JogButton { Layout.row: 4; Layout.column: 1; text: "P"; tip: qsTr("C 回零度") }
            JogButton { Layout.row: 4; Layout.column: 2; glyph: Theme.icon.rotate; tip: qsTr("C-（顺时针）") }
            Label {
                Layout.row: 4; Layout.column: 3; Layout.leftMargin: Theme.spacing * 2
                Layout.alignment: Qt.AlignCenter
                text: "C"; font.bold: true
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: 2

            Label { text: qsTr("步距 (mm/°)") }
            ButtonGroup { id: stepGroup }
            Repeater {
                model: root.stepSizes
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 26
                    text: String(modelData)
                    checkable: true
                    checked: root.stepSize === modelData
                    focusPolicy: Qt.NoFocus
                    ButtonGroup.group: stepGroup
                    onClicked: root.stepSize = modelData
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            spacing: 2

            Label { text: qsTr("速度") }
            Slider {
                id: speed
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 130
                orientation: Qt.Vertical
                from: 1
                to: 100
                stepSize: 1
                value: 100
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: Math.round(speed.value) + "%"
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            spacing: Theme.spacing

            Item { implicitHeight: 18 }
            JogButton { glyph: Theme.icon.nozzle; tip: qsTr("吸嘴移到相机中心位置") }
            JogButton { glyph: Theme.icon.camera; tip: qsTr("相机移到吸嘴位置") }
        }
    }
}
