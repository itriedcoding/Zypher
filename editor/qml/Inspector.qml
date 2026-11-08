// Inspector panel: transform editing for the selected entity.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8
    anchors.fill: parent
    anchors.margins: 10

    Label {
        text: "Inspector"
        color: "#d7dbe3"
        font.bold: true
    }

    Label {
        text: editor.selection >= 0
              ? "Entity: " + editor.selectedName
              : "No entity selected"
        color: "#8b93a1"
        elide: Text.ElideRight
        Layout.fillWidth: true
    }

    GridLayout {
        columns: 4
        enabled: editor.selection >= 0
        Layout.fillWidth: true
        columnSpacing: 6
        rowSpacing: 6

        Label { text: "Position"; color: "#8b93a1" }
        TextField { id: posX; text: editor.selectedPosition.x.toFixed(2); validator: DoubleValidator {} }
        TextField { id: posY; text: editor.selectedPosition.y.toFixed(2); validator: DoubleValidator {} }
        TextField { id: posZ; text: editor.selectedPosition.z.toFixed(2); validator: DoubleValidator {} }

        Label { text: "Scale"; color: "#8b93a1" }
        TextField { id: scaleX; text: editor.selectedScale.x.toFixed(2); validator: DoubleValidator {} }
        TextField { id: scaleY; text: editor.selectedScale.y.toFixed(2); validator: DoubleValidator {} }
        TextField { id: scaleZ; text: editor.selectedScale.z.toFixed(2); validator: DoubleValidator {} }
    }

    Button {
        text: "Apply Transform"
        enabled: editor.selection >= 0
        Layout.fillWidth: true
        onClicked: {
            editor.setSelectedTransform(
                parseFloat(posX.text) || 0, parseFloat(posY.text) || 0, parseFloat(posZ.text) || 0,
                parseFloat(scaleX.text) || 1, parseFloat(scaleY.text) || 1, parseFloat(scaleZ.text) || 1)
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3038" }

    Label { text: "Mesh"; color: "#8b93a1" }
    Label {
        text: editor.selectedMesh.length > 0 ? editor.selectedMesh : "(none)"
        color: "#c3c9d4"
        elide: Text.ElideLeft
        Layout.fillWidth: true
    }

    Label { text: "Script"; color: "#8b93a1" }
    Label {
        text: editor.selectedScript.length > 0 ? editor.selectedScript : "(none)"
        color: "#c3c9d4"
        Layout.fillWidth: true
    }

    Item { Layout.fillHeight: true }
}
