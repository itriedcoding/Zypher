// Scene hierarchy panel: entity list with selection and reordering.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    Label {
        text: "Hierarchy"
        color: "#d7dbe3"
        font.bold: true
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: editor.entityNames
        currentIndex: editor.selection
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            width: list.width
            height: 28
            color: index === editor.selection ? "#2c3547" : "transparent"
            radius: 4

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 8
                text: modelData
                color: index === editor.selection ? "#ffffff" : "#c3c9d4"
                elide: Text.ElideRight
                width: parent.width - 24
            }

            MouseArea {
                anchors.fill: parent
                onClicked: editor.select(index)
                onDoubleClicked: editor.select(index)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Button {
            text: "Add"
            Layout.fillWidth: true
            onClicked: editor.addEntity("Entity", "")
        }
        Button {
            text: "Delete"
            Layout.fillWidth: true
            enabled: editor.selection >= 0
            onClicked: editor.removeSelected()
        }
        Button {
            text: "Up"
            enabled: editor.selection > 0
            onClicked: editor.moveSelected(-1)
        }
        Button {
            text: "Down"
            enabled: editor.selection >= 0 && editor.selection < editor.entityCount - 1
            onClicked: editor.moveSelected(1)
        }
    }
}
