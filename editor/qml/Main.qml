// Zypher editor shell. The panels talk to the C++ EditorController through the
// `editor` context property (editor/qt/main.cpp).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    visible: true
    title: editor.projectName.length > 0
           ? editor.projectName + (editor.dirty ? " *" : "") + " - Zypher"
           : "Zypher Editor"

    readonly property color shellColor: "#15171c"
    readonly property color panel: "#1c1f26"
    readonly property color border: "#2b3038"
    readonly property color accent: "#4c9aff"
    readonly property color text: "#d7dbe3"

    color: shellColor

    menuBar: MenuBar {
        Menu {
            title: "File"
            Action {
                text: "Open Project..."
                shortcut: "Ctrl+O"
                onTriggered: {
                    // Built on first use so startup does not touch the native dialog backend.
                    if (projectDialog.source.toString().length === 0) {
                        projectDialog.source = "ProjectDialog.qml"
                    }
                    if (projectDialog.item) {
                        projectDialog.item.open()
                    }
                }
            }
            Action { text: "Save Scene"; shortcut: "Ctrl+S"; onTriggered: editor.saveScene() }
            MenuSeparator {}
            Action { text: "Quit"; shortcut: "Ctrl+Q"; onTriggered: Qt.quit() }
        }
        Menu {
            title: "Edit"
            Action {
                text: "Undo " + (editor.undoLabel.length > 0 ? editor.undoLabel : "")
                shortcut: "Ctrl+Z"
                enabled: editor.canUndo
                onTriggered: editor.undo()
            }
            Action {
                text: "Redo " + (editor.redoLabel.length > 0 ? editor.redoLabel : "")
                shortcut: "Ctrl+Shift+Z"
                enabled: editor.canRedo
                onTriggered: editor.redo()
            }
        }
        Menu {
            title: "Entity"
            Action { text: "Add Entity"; shortcut: "Ctrl+N"; onTriggered: editor.addEntity("Entity", "") }
            Action { text: "Delete Selected"; shortcut: "Delete"; onTriggered: editor.removeSelected() }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 1

        // Left: scene hierarchy + asset browser ------------------------------
        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                SceneHierarchy { Layout.fillHeight: true }

                Rectangle { Layout.fillWidth: true; height: 1; color: border }

                AssetBrowser { Layout.preferredHeight: 240 }
            }
        }

        // Centre: viewport placeholder --------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0f1116"
            border.color: border

            Column {
                anchors.centerIn: parent
                spacing: 10
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Viewport"
                    color: text
                    font.pixelSize: 22
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: editor.sceneName.length > 0
                          ? editor.sceneName + " - " + editor.entityCount + " entities"
                          : "Open a project to start"
                    color: "#8b93a1"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: editor.statusMessage
                    color: "#8b93a1"
                    wrapMode: Text.WordWrap
                    width: 420
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // Right: inspector ---------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            color: panel
            Inspector {}
        }
    }

    // Bottom: console ------------------------------------------------------
    ConsolePanel {
        id: console
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 160
    }

    // Holds the native file picker; its source is set when the menu is first used.
    Loader {
        id: projectDialog
    }

    Connections {
        target: editor
        function onLogMessage(message) { console.append(message) }
    }

    Component.onCompleted: editor.startup()
}
