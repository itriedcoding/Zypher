// Console panel: engine/editor log output.
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "#101216"
    border.color: "#2b3038"

    property alias logText: output.text

    function append(message) {
        logText = logText.length > 0 ? logText + "\n" + message : message
        output.cursorPosition = output.length
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 6

        TextArea {
            id: output
            readOnly: true
            wrapMode: TextArea.Wrap
            color: "#a8b1c0"
            font.family: "monospace"
            font.pixelSize: 12
            background: null
        }
    }
}
