import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import "../components"

// Machine configuration: device categories on the left, editor on the right.
// Stored separately from the program file.
SplitView {
    id: root
    orientation: Qt.Horizontal

    readonly property var categories: [
        qsTr("常规"), qsTr("驱动器"), qsTr("运动轴"), qsTr("贴装头"), qsTr("吸嘴"),
        qsTr("吸嘴库"), qsTr("相机"), qsTr("执行器"), qsTr("送料器站位"), qsTr("信号")
    ]

    Frame {
        SplitView.preferredWidth: 180
        SplitView.minimumWidth: 140
        padding: 1

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            model: root.categories
            currentIndex: 0
            delegate: ItemDelegate {
                width: list.width
                text: modelData
                highlighted: ListView.isCurrentItem
                onClicked: list.currentIndex = index
            }
        }
    }

    Placeholder {
        SplitView.fillWidth: true
        text: qsTr("%1 设置").arg(root.categories[list.currentIndex])
    }
}
