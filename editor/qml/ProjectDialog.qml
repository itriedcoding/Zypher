// Native file picker for the editor. Loaded on first use from Main.qml so that
// starting the editor never touches the platform dialog backend.
import QtQuick
import QtQuick.Dialogs

FileDialog {
    title: "Open Zypher Project"
    nameFilters: ["Zypher projects (zypher.project.json)", "All files (*)"]
    onAccepted: {
        var path = selectedFile.toString()
        if (path.indexOf("file://") === 0) {
            path = decodeURIComponent(path.substring(7))
        }
        editor.openProject(path)
    }
}
