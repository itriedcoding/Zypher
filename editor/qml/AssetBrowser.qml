// Asset browser: project assets grouped by type.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    Label {
        text: "Assets"
        color: "#d7dbe3"
        font.bold: true
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: editor.assetPaths
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            width: ListView.view.width
            height: 24
            color: "transparent"

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 6
                text: modelData
                color: "#c3c9d4"
                elide: Text.ElideMiddle
                width: parent.width - 12
            }

            MouseArea {
                anchors.fill: parent
                onClicked: editor.selectAsset(modelData)
            }
        }
    }

    Label {
        text: editor.assetCount + " asset(s) - drop files into " + editor.assetRoot
        color: "#7d8592"
        font.pixelSize: 11
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
