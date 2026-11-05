pragma Singleton
import QtQuick 2.14

// Shared sizes, colors and fonts for the UI. Colors that follow the system
// palette are taken from the controls' palette instead of being listed here.
QtObject {
    readonly property string uiFont: "Microsoft YaHei UI"
    readonly property int uiFontSize: 13
    readonly property string monoFont: "Consolas"
    // Segoe MDL2 Assets ships with Windows 10 and 11.
    readonly property string iconFont: "Segoe MDL2 Assets"

    readonly property int spacing: 6
    readonly property int margin: 6
    readonly property int toolButtonSize: 28
    readonly property int jogButtonWidth: 48
    readonly property int jogButtonHeight: 32
    readonly property int rowHeight: 24

    readonly property color cameraBackground: "#000000"
    readonly property color reticle: "#4dff4d"
    readonly property color gridLine: "#d0d0d0"
    readonly property color headerBackground: "#ececec"
    readonly property color alternateRow: "#f6f6f6"
    readonly property color droOnline: "#b9f5b0"
    readonly property color droOffline: "#e0e0e0"
    readonly property color danger: "#c62828"
    readonly property color ok: "#2e7d32"
    readonly property color mutedText: "#808080"

    // Segoe MDL2 Assets code points used by toolbars and jog buttons.
    readonly property QtObject icon: QtObject {
        readonly property string play: "\uE768"
        readonly property string pause: "\uE769"
        readonly property string stop: "\uE71A"
        readonly property string step: "\uE893"
        readonly property string add: "\uE710"
        readonly property string remove: "\uE711"
        readonly property string del: "\uE74D"
        readonly property string moveUp: "\uE70E"
        readonly property string moveDown: "\uE70D"
        readonly property string arrowUp: "\uE74A"
        readonly property string arrowDown: "\uE74B"
        readonly property string arrowLeft: "\uE72B"
        readonly property string arrowRight: "\uE72A"
        readonly property string rotate: "\uE7AD"
        readonly property string home: "\uE80F"
        readonly property string power: "\uE7E8"
        readonly property string camera: "\uE722"
        readonly property string target: "\uE81D"
        readonly property string nozzle: "\uE896"
        readonly property string open: "\uE8E5"
        readonly property string save: "\uE74E"
        readonly property string newFile: "\uE8A5"
        readonly property string importFile: "\uE8B5"
        readonly property string search: "\uE721"
        readonly property string settings: "\uE713"
        readonly property string grid: "\uE80A"
        readonly property string view: "\uE890"
        readonly property string warning: "\uE7BA"
        readonly property string clear: "\uE894"
        readonly property string check: "\uE8FB"
    }
}
