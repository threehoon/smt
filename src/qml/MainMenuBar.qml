import QtQuick 2.14
import QtQuick.Controls 2.14

// Main window menus. File (including Import placements), Edit > Undo / Redo
// and About work; the rest are placeholders until the matching C++ commands
// exist. Keyboard shortcuts are Shortcut items in main.qml, which work while
// the menus are closed.
MenuBar {
    id: root

    property bool canSave: false
    property bool canSaveAs: false
    property bool canUndo: false
    property bool canRedo: false
    property string undoText
    property string redoText

    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    signal quitRequested()
    signal undoRequested()
    signal redoRequested()
    signal aboutRequested()
    signal importRequested()

    Menu {
        title: qsTr("文件(&F)")
        MenuItem { text: qsTr("新建程序"); onTriggered: root.newRequested() }
        MenuItem { text: qsTr("打开程序..."); onTriggered: root.openRequested() }
        Menu {
            title: qsTr("最近打开")
            MenuItem { text: qsTr("（无）"); enabled: false }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("保存")
            enabled: root.canSave
            onTriggered: root.saveRequested()
        }
        MenuItem {
            text: qsTr("另存为...")
            enabled: root.canSaveAs
            onTriggered: root.saveAsRequested()
        }
        MenuSeparator {}
        // With no program open, the import creates one.
        MenuItem {
            text: qsTr("导入贴片坐标...")
            onTriggered: root.importRequested()
        }
        MenuSeparator {}
        MenuItem { text: qsTr("退出"); onTriggered: root.quitRequested() }
    }

    Menu {
        title: qsTr("编辑(&E)")
        MenuItem {
            text: root.undoText !== "" ? qsTr("撤销 %1").arg(root.undoText) : qsTr("撤销")
            enabled: root.canUndo
            onTriggered: root.undoRequested()
        }
        MenuItem {
            text: root.redoText !== "" ? qsTr("重做 %1").arg(root.redoText) : qsTr("重做")
            enabled: root.canRedo
            onTriggered: root.redoRequested()
        }
        MenuSeparator {}
        MenuItem { text: qsTr("剪切"); enabled: false }
        MenuItem { text: qsTr("复制"); enabled: false }
        MenuItem { text: qsTr("粘贴"); enabled: false }
        MenuItem { text: qsTr("删除"); enabled: false }
    }

    Menu {
        title: qsTr("视图(&V)")
        MenuItem { text: qsTr("放大") }
        MenuItem { text: qsTr("缩小") }
        MenuItem { text: qsTr("重置布局") }
    }

    Menu {
        title: qsTr("生产(&J)")
        MenuItem { text: qsTr("开始") }
        MenuItem { text: qsTr("单步") }
        MenuItem { text: qsTr("暂停") }
        MenuItem { text: qsTr("停止") }
        MenuSeparator {}
        MenuItem { text: qsTr("重置贴装状态") }
    }

    Menu {
        title: qsTr("机器(&M)")
        MenuItem { text: qsTr("连接") }
        MenuItem { text: qsTr("断开") }
        MenuSeparator {}
        MenuItem { text: qsTr("回原点") }
        MenuItem { text: qsTr("急停") }
    }

    Menu {
        title: qsTr("工具(&T)")
        MenuItem { text: qsTr("选项...") }
    }

    Menu {
        title: qsTr("帮助(&H)")
        MenuItem { text: qsTr("关于迅贴 SwiftPlace"); onTriggered: root.aboutRequested() }
    }
}
