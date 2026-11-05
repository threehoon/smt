import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import Qt.labs.platform 1.0 as Platform
import "components"
import "dialogs"
import "panels"

// Main window layout:
//   left:  camera view (top) and machine controls (bottom)
//   right: workspace tabs (production, panel, boards, ...)
//   footer: status message, progress and machine position
ApplicationWindow {
    id: root

    // Runs after a successful save from saveThen(); null when none.
    property var afterSave: null
    // Runs once the user answered the "save changes?" question.
    property var afterPrompt: null
    // Set once the user agreed to close, so onClosing lets it through.
    property bool closeConfirmed: false

    // A form field being typed into commits when it loses focus. Called
    // before anything that reads or replaces the program.
    function commitEdits() {
        root.contentItem.forceActiveFocus();
    }

    // Saves (asking for a file name when there is none yet), then runs action.
    function saveThen(action) {
        commitEdits();
        if (programDocument.filePath === "" || programDocument.saveAsRequired) {
            afterSave = action;
            saveDialog.open();
        } else if (programDocument.save() && action) {
            action();
        }
    }

    // Runs action now, or after asking whether to save the changes first.
    function whenSaved(action) {
        commitEdits();
        if (!programDocument.modified) {
            action();
            return;
        }
        afterPrompt = action;
        savePrompt.open();
    }

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: qsTr("%1%2 - 迅贴 SwiftPlace")
           .arg(programDocument.displayName !== "" ? programDocument.displayName : qsTr("未命名"))
           .arg(programDocument.modified ? " *" : "")

    font.family: Theme.uiFont
    font.pixelSize: Theme.uiFontSize

    onClosing: {
        commitEdits();
        if (closeConfirmed || !programDocument.modified)
            return;
        close.accepted = false;
        whenSaved(function() {
            root.closeConfirmed = true;
            root.close();
        });
    }

    menuBar: MainMenuBar {
        canSave: programDocument.loaded && !programDocument.saveAsRequired
        canSaveAs: programDocument.loaded
        canUndo: programDocument.canUndo
        canRedo: programDocument.canRedo
        undoText: programDocument.undoText
        redoText: programDocument.redoText
        onNewRequested: root.whenSaved(function() { programDocument.newProgram(); })
        onOpenRequested: root.whenSaved(function() { openDialog.open(); })
        onSaveRequested: root.saveThen(null)
        onSaveAsRequested: { root.commitEdits(); saveDialog.open(); }
        onQuitRequested: root.close()
        onUndoRequested: { root.commitEdits(); programDocument.undo(); }
        onRedoRequested: { root.commitEdits(); programDocument.redo(); }
        onAboutRequested: aboutDialog.open()
        onImportRequested: {
            root.commitEdits();
            programDocument.cadImport.request(programDocument.boardPlacements.boardIndex);
        }
    }

    // Window-wide, so they work with the menus closed. A text field being
    // edited keeps Ctrl+Z / Ctrl+Y for its own text.
    Shortcut {
        sequence: StandardKey.New
        onActivated: root.whenSaved(function() { programDocument.newProgram(); })
    }
    Shortcut {
        sequence: StandardKey.Open
        onActivated: root.whenSaved(function() { openDialog.open(); })
    }
    Shortcut {
        sequence: StandardKey.Save
        enabled: programDocument.loaded
        onActivated: root.saveThen(null)
    }
    Shortcut {
        sequence: StandardKey.SaveAs
        enabled: programDocument.loaded
        onActivated: { root.commitEdits(); saveDialog.open(); }
    }
    Shortcut {
        sequence: StandardKey.Undo
        enabled: programDocument.canUndo
        onActivated: { root.commitEdits(); programDocument.undo(); }
    }
    Shortcut {
        sequence: StandardKey.Redo
        enabled: programDocument.canRedo
        onActivated: { root.commitEdits(); programDocument.redo(); }
    }

    footer: StatusBar {
        id: statusBar
        totalCount: programDocument.jobPlacements.enabledCount
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        SplitView {
            orientation: Qt.Vertical
            SplitView.preferredWidth: 540
            SplitView.minimumWidth: 420

            CameraPanel {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 200
            }
            MachineControlPanel {
                SplitView.preferredHeight: 300
                SplitView.minimumHeight: 260
            }
        }

        WorkspaceView {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
        }
    }

    // Native file dialogs. Qt.labs.platform rather than QtQuick.Dialogs: the
    // latter falls back to a Controls 1 dialog.
    Platform.FileDialog {
        id: openDialog
        title: qsTr("打开程序")
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [qsTr("SMT 程序 (*.smt.xml)"), qsTr("所有文件 (*)")]
        onAccepted: programDocument.open(file)
    }

    Platform.FileDialog {
        id: saveDialog
        title: qsTr("程序另存为")
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [qsTr("SMT 程序 (*.smt.xml)")]
        defaultSuffix: "smt.xml"
        onAccepted: {
            const action = root.afterSave;
            root.afterSave = null;
            if (programDocument.saveAs(file) && action)
                action();
        }
        onRejected: root.afterSave = null
    }

    // "Save changes?" before the program is replaced or the window closes.
    Dialog {
        id: savePrompt
        objectName: "savePrompt"
        title: qsTr("保存修改")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 40 : 420)

        onAccepted: {
            const action = root.afterPrompt;
            root.afterPrompt = null;
            root.saveThen(action);
        }
        onRejected: root.afterPrompt = null

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("%1 有未保存的修改，是否保存？")
                  .arg(programDocument.displayName !== "" ? programDocument.displayName
                                                          : qsTr("当前程序"))
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("保存")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                objectName: "discardButton"
                text: qsTr("不保存")
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            }
            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            onDiscarded: {
                const action = root.afterPrompt;
                root.afterPrompt = null;
                savePrompt.close();
                if (action)
                    action();
            }
        }
    }

    MessageBox {
        id: messageBox
        objectName: "messageBox"
    }

    Connections {
        target: programDocument
        onMessageRaised: messageBox.show(title, text, details)
        onStatusMessage: statusBar.show(text)
    }

    // Import placements: menu or boards page -> file dialog -> import dialog.
    Platform.FileDialog {
        id: cadFileDialog
        title: qsTr("导入贴片坐标")
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [qsTr("贴片坐标文件 (*.pos *.csv *.txt *.tsv)"), qsTr("所有文件 (*)")]
        onAccepted: programDocument.cadImport.open(file)
    }

    CadImportDialog {}

    Connections {
        target: programDocument.cadImport
        onRequested: {
            root.commitEdits();
            cadFileDialog.open();
        }
        onImported: {
            messageBox.show(title, text, "");
            statusBar.show(qsTr("已导入贴片坐标"));
        }
    }

    Dialog {
        id: aboutDialog
        title: qsTr("关于迅贴 SwiftPlace")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Ok

        Label {
            text: qsTr("迅贴 SwiftPlace %1\nSMT 贴片机上位机软件").arg(Qt.application.version)
        }
    }
}
