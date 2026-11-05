import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Nozzle selector plus tabs for jogging and other manual machine controls.
GroupBox {
    id: root
    title: qsTr("机器控制")

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        ComboBox {
            Layout.fillWidth: true
            enabled: count > 0
            model: []
            displayText: count > 0 ? currentText : qsTr("未配置吸嘴")
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: qsTr("点动"); width: implicitWidth }
            TabButton { text: qsTr("特殊"); width: implicitWidth }
            TabButton { text: qsTr("执行器"); width: implicitWidth }
            TabButton { text: qsTr("安全"); width: implicitWidth }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            JogPanel {}
            Placeholder { text: qsTr("吸嘴更换、真空测试等特殊操作") }
            Placeholder { text: qsTr("执行器（气缸、真空阀、灯光等）手动控制") }
            Placeholder { text: qsTr("安全门、急停与软限位状态") }
        }
    }
}
