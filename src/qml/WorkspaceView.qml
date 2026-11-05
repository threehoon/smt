import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "components"
import "pages"

// Right side of the main window: one tab per editor / view.
Pane {
    id: root
    padding: Theme.margin

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs
            objectName: "workspaceTabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("生产"); width: implicitWidth }
            TabButton { text: qsTr("拼板"); width: implicitWidth }
            TabButton { text: qsTr("单板"); width: implicitWidth }
            TabButton { text: qsTr("元件"); width: implicitWidth }
            TabButton { text: qsTr("封装"); width: implicitWidth }
            TabButton { text: qsTr("送料器"); width: implicitWidth }
            TabButton { text: qsTr("视觉"); width: implicitWidth }
            TabButton { text: qsTr("机器设置"); width: implicitWidth }
            TabButton { text: qsTr("日志"); width: implicitWidth }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: Theme.margin

            StackLayout {
                anchors.fill: parent
                currentIndex: tabs.currentIndex

                JobPage {}
                PanelPage {}
                BoardsPage {}
                ComponentsPage {}
                PackagesPage {}
                FeedersPage {}
                VisionPage {}
                MachineSetupPage {}
                LogPage {}
            }
        }
    }
}
