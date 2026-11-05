import QtQuick 2.14
import QtQuick.Controls 2.14
import "."

// Toolbar / jog button. Shows a Segoe MDL2 glyph when `glyph` is set,
// otherwise the bold `text`. `tip` becomes the tooltip.
Button {
    id: control

    property string glyph
    property string tip
    property bool mirrorGlyph: false
    property color glyphColor: palette.buttonText

    flat: true
    focusPolicy: Qt.NoFocus
    implicitWidth: Theme.toolButtonSize
    implicitHeight: Theme.toolButtonSize
    padding: 0

    contentItem: Text {
        id: glyphText
        text: control.glyph !== "" ? control.glyph : control.text
        font.family: control.glyph !== "" ? Theme.iconFont : control.font.family
        font.pixelSize: control.glyph !== "" ? 16 : control.font.pixelSize + 2
        font.bold: control.glyph === ""
        color: control.enabled ? control.glyphColor : Theme.mutedText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        transform: Scale {
            origin.x: glyphText.width / 2
            xScale: control.mirrorGlyph ? -1 : 1
        }
    }

    ToolTip.visible: tip !== "" && hovered
    ToolTip.text: tip
    ToolTip.delay: 600
}
