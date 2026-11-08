#include "EditorController.h"

#include "zypher/core/FileSystem.h"
#include "zypher/json/Json.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

namespace {

// Project locations searched on startup, relative to the executable: the
// installed layout (bin/zypher_editor next to share/), the macOS .app bundle
// (Contents/MacOS/../Resources), and a source checkout for development builds.
[[nodiscard]] QStringList demoProjectCandidates() {
    const QString exeDir = QCoreApplication::applicationDirPath();
    return {
        exeDir + QStringLiteral("/share/zypher/samples/demo/zypher.project.json"),
        exeDir + QStringLiteral("/../share/zypher/samples/demo/zypher.project.json"),
        exeDir + QStringLiteral("/../Resources/share/zypher/samples/demo/zypher.project.json"),
        exeDir + QStringLiteral("/../../../Resources/share/zypher/samples/demo/zypher.project.json"),
        QDir::currentPath() + QStringLiteral("/samples/demo/zypher.project.json"),
    };
}

} // namespace

EditorController::EditorController(QObject* parent) : QObject(parent) {}

QString EditorController::bundledDemoProject() const {
    for (const QString& candidate : demoProjectCandidates()) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.canonicalFilePath();
        }
    }
    return {};
}

bool EditorController::demoAvailable() const { return !bundledDemoProject().isEmpty(); }

void EditorController::startup() {
    publish(tr("Zypher editor ready."));

    const QString project =
        !m_initialProject.isEmpty() && QFileInfo::exists(m_initialProject) ? m_initialProject : bundledDemoProject();
    if (project.isEmpty()) {
        publish(tr("No project open. Use File > Open Project (Ctrl+O) to start."));
        return;
    }
    openProject(project);
}

void EditorController::openDemoProject() {
    const QString project = bundledDemoProject();
    if (project.isEmpty()) {
        publish(tr("No demo project was found next to the editor."));
        return;
    }
    openProject(project);
}

void EditorController::openProjectDialog() {
    const QString path = QFileDialog::getOpenFileName(nullptr, tr("Open Zypher Project"), QString(),
                                                      tr("Zypher projects (zypher.project.json)"));
    if (path.isEmpty()) {
        return;
    }
    openProject(path);
}

bool EditorController::openProject(const QString& path) {
    if (!m_session.openProject(path.toStdString())) {
        publish(QString::fromStdString(m_session.lastError()));
        return false;
    }
    loadAssets();
    publish(tr("Opened %1 (%2 entities)")
                .arg(QString::fromStdString(m_session.projectName()))
                .arg(m_session.scene().entities.size()));
    return true;
}

void EditorController::loadAssets() {
    m_assetPaths.clear();
    const std::string path = m_session.projectPath();
    if (path.empty()) {
        return;
    }
    auto text = zy::FileSystem::readText(path);
    if (!text.has_value()) {
        return;
    }
    auto document = zy::parseJson(*text);
    if (document.hasError() || !document.value().isObject()) {
        return;
    }
    const auto& assets = document.value()["assets"];
    for (zy::usize index = 0; index < assets.size(); ++index) {
        m_assetPaths.append(QString::fromStdString(assets.at(index).getString("path", "")));
    }
}

void EditorController::saveScene() {
    if (!m_session.saveScene()) {
        publish(QString::fromStdString(m_session.lastError()));
        return;
    }
    publish(tr("Saved %1").arg(QString::fromStdString(m_session.scenePath())));
}

void EditorController::select(int index) {
    m_session.select(index);
    publish();
}

void EditorController::addEntity(const QString& name, const QString& meshPath) {
    m_session.addEntity(name.toStdString(), meshPath.toStdString());
    publish(tr("Added %1").arg(QString::fromStdString(m_session.scene().entities.empty()
                                                          ? std::string()
                                                          : m_session.scene().entities.back().name)));
}

void EditorController::removeSelected() {
    if (m_session.selection() < 0) {
        return;
    }
    m_session.removeEntity(static_cast<zy::usize>(m_session.selection()));
    publish("Removed entity");
}

void EditorController::moveSelected(int delta) {
    if (m_session.selection() < 0) {
        return;
    }
    m_session.moveEntity(static_cast<zy::usize>(m_session.selection()), delta);
    publish("Reordered entity");
}

void EditorController::undo() {
    if (m_session.undo()) {
        publish(tr("Undo"));
    }
}

void EditorController::redo() {
    if (m_session.redo()) {
        publish(tr("Redo"));
    }
}

void EditorController::setSelectedTransform(float px, float py, float pz, float sx, float sy, float sz) {
    if (m_session.selection() < 0) {
        return;
    }
    const auto index = static_cast<zy::usize>(m_session.selection());
    m_session.setEntityPosition(index, zy::Vec3{px, py, pz});
    m_session.setEntityScale(index, zy::Vec3{sx, sy, sz});
    publish("Updated transform");
}

void EditorController::selectAsset(const QString& path) {
    publish(tr("Asset: %1").arg(path));
}

void EditorController::publish(const QString& message) {
    if (!message.isEmpty()) {
        m_status = message;
        emit logMessage(message);
    }
    emit changed();
}

QString EditorController::projectName() const { return QString::fromStdString(m_session.projectName()); }
QString EditorController::sceneName() const { return QString::fromStdString(m_session.scene().name); }
QString EditorController::assetRoot() const { return QString::fromStdString(m_session.assetRoot()); }
QString EditorController::statusMessage() const { return m_status; }
QStringList EditorController::entityNames() const {
    QStringList names;
    for (const auto& entity : m_session.scene().entities) {
        names.append(QString::fromStdString(entity.name));
    }
    return names;
}
QStringList EditorController::assetPaths() const { return m_assetPaths; }
int EditorController::assetCount() const { return m_assetPaths.size(); }
int EditorController::entityCount() const { return static_cast<int>(m_session.scene().entities.size()); }
int EditorController::selection() const { return m_session.selection(); }
bool EditorController::dirty() const { return m_session.isDirty(); }
bool EditorController::canUndo() const { return m_session.canUndo(); }
bool EditorController::canRedo() const { return m_session.canRedo(); }
QString EditorController::undoLabel() const { return QString::fromStdString(m_session.undoLabel()); }
QString EditorController::redoLabel() const { return QString::fromStdString(m_session.redoLabel()); }

QVariantMap EditorController::selectedPosition() const {
    const int index = m_session.selection();
    if (index < 0) {
        return {};
    }
    const zy::Vec3 position = m_session.scene().entities[static_cast<zy::usize>(index)].transform.position;
    return QVariantMap{{"x", position.x}, {"y", position.y}, {"z", position.z}};
}

QVariantMap EditorController::selectedScale() const {
    const int index = m_session.selection();
    if (index < 0) {
        return {};
    }
    const zy::Vec3 scale = m_session.scene().entities[static_cast<zy::usize>(index)].transform.scale;
    return QVariantMap{{"x", scale.x}, {"y", scale.y}, {"z", scale.z}};
}

QString EditorController::selectedName() const {
    const int index = m_session.selection();
    return index < 0 ? QString() : QString::fromStdString(m_session.scene().entities[static_cast<zy::usize>(index)].name);
}

QString EditorController::selectedMesh() const {
    const int index = m_session.selection();
    return index < 0 ? QString() : QString::fromStdString(m_session.scene().entities[static_cast<zy::usize>(index)].meshPath);
}

QString EditorController::selectedScript() const {
    const int index = m_session.selection();
    return index < 0 ? QString() : QString::fromStdString(m_session.scene().entities[static_cast<zy::usize>(index)].scriptClass);
}
