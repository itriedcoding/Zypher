#include "EditorSession.h"

#include "zypher/core/FileSystem.h"
#include "zypher/json/Json.h"

namespace zy::editor {
namespace {

[[nodiscard]] std::string joinPath(std::string_view directory, std::string_view relative) {
    return FileSystem::join(directory, relative);
}

} // namespace

bool EditorSession::fail(std::string message) {
    m_lastError = std::move(message);
    return false;
}

bool EditorSession::openProject(const std::string& projectPath) {
    auto text = FileSystem::readText(projectPath);
    if (!text.has_value()) {
        return fail("cannot read project '" + projectPath + "'");
    }
    auto document = parseJson(*text);
    if (document.hasError()) {
        return fail("'" + projectPath + "': " + document.error().message);
    }
    if (!document.value().isObject()) {
        return fail("project root must be a JSON object");
    }

    m_projectPath = projectPath;
    m_projectName = document.value().getString("name", "Untitled Project");
    m_assetRoot = document.value().getString("assetRoot", "assets");
    const std::string sceneRelative = document.value().getString("scene", "scene.zscene");
    m_scenePath = joinPath(FileSystem::parentPath(projectPath), sceneRelative);

    auto scene = loadSceneFile(m_scenePath);
    if (scene.hasError()) {
        return fail(scene.error().message);
    }
    m_scene = std::move(scene).value();
    m_selection = m_scene.entities.empty() ? -1 : 0;
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    m_lastError.clear();
    return true;
}

bool EditorSession::saveScene() {
    if (!isOpen()) {
        return fail("no project is open");
    }
    const std::string text = sceneToJson(m_scene).dump(true) + "\n";
    const Status status = FileSystem::writeText(m_scenePath, text);
    if (status.hasError()) {
        return fail("cannot write '" + m_scenePath + "': " + status.error().message);
    }
    m_dirty = false;
    return true;
}

bool EditorSession::validIndex(usize index) const {
    return index < m_scene.entities.size();
}

void EditorSession::select(int index) {
    if (index < 0 || !validIndex(static_cast<usize>(index))) {
        m_selection = -1;
        return;
    }
    m_selection = index;
}

void EditorSession::commit(const std::string& label, const std::function<void(SceneDesc&)>& edit) {
    m_undo.push_back(Snapshot{label, m_scene});
    if (m_undo.size() > kMaxUndo) {
        m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
    edit(m_scene);
    m_dirty = true;
}

bool EditorSession::setEntityPosition(usize index, const Vec3& position) {
    if (!validIndex(index)) {
        return fail("entity index out of range");
    }
    commit("move " + m_scene.entities[index].name, [index, position](SceneDesc& scene) {
        scene.entities[index].transform.position = position;
    });
    return true;
}

bool EditorSession::setEntityScale(usize index, const Vec3& scale) {
    if (!validIndex(index)) {
        return fail("entity index out of range");
    }
    commit("scale " + m_scene.entities[index].name, [index, scale](SceneDesc& scene) {
        scene.entities[index].transform.scale = scale;
    });
    return true;
}

bool EditorSession::renameEntity(usize index, const std::string& name) {
    if (!validIndex(index)) {
        return fail("entity index out of range");
    }
    if (name.empty()) {
        return fail("entity name must not be empty");
    }
    for (usize other = 0; other < m_scene.entities.size(); ++other) {
        if (other != index && m_scene.entities[other].name == name) {
            return fail("duplicate entity name: " + name);
        }
    }
    commit("rename " + m_scene.entities[index].name, [index, name](SceneDesc& scene) {
        scene.entities[index].name = name;
    });
    return true;
}

bool EditorSession::addEntity(const std::string& name, const std::string& meshPath) {
    std::string candidate = name.empty() ? "Entity" : name;
    int suffix = 2;
    const auto taken = [this](const std::string& value) {
        for (const auto& entity : m_scene.entities) {
            if (entity.name == value) {
                return true;
            }
        }
        return false;
    };
    while (taken(candidate)) {
        candidate = (name.empty() ? "Entity" : name) + " " + std::to_string(suffix++);
    }

    commit("add " + candidate, [candidate, meshPath](SceneDesc& scene) {
        EntityDesc entity;
        entity.name = candidate;
        entity.meshPath = FileSystem::normalize(meshPath);
        scene.entities.push_back(std::move(entity));
    });
    m_selection = static_cast<int>(m_scene.entities.size()) - 1;
    return true;
}

bool EditorSession::removeEntity(usize index) {
    if (!validIndex(index)) {
        return fail("entity index out of range");
    }
    const std::string name = m_scene.entities[index].name;
    commit("remove " + name, [index](SceneDesc& scene) {
        scene.entities.erase(scene.entities.begin() + static_cast<isize>(index));
    });
    if (m_selection >= static_cast<int>(m_scene.entities.size())) {
        m_selection = static_cast<int>(m_scene.entities.size()) - 1;
    }
    return true;
}

bool EditorSession::moveEntity(usize index, int delta) {
    if (!validIndex(index)) {
        return fail("entity index out of range");
    }
    const int target = static_cast<int>(index) + delta;
    if (delta == 0 || target < 0 || target >= static_cast<int>(m_scene.entities.size())) {
        return true;
    }
    commit("reorder " + m_scene.entities[index].name, [index, target](SceneDesc& scene) {
        EntityDesc moved = std::move(scene.entities[index]);
        scene.entities.erase(scene.entities.begin() + static_cast<isize>(index));
        scene.entities.insert(scene.entities.begin() + target, std::move(moved));
    });
    m_selection = target;
    return true;
}

std::string EditorSession::undoLabel() const {
    return m_undo.empty() ? std::string() : m_undo.back().label;
}

std::string EditorSession::redoLabel() const {
    return m_redo.empty() ? std::string() : m_redo.back().label;
}

bool EditorSession::undo() {
    if (m_undo.empty()) {
        return false;
    }
    Snapshot snapshot = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back(Snapshot{snapshot.label, m_scene});
    m_scene = std::move(snapshot.scene);
    m_dirty = true;
    if (m_selection >= static_cast<int>(m_scene.entities.size())) {
        m_selection = static_cast<int>(m_scene.entities.size()) - 1;
    }
    return true;
}

bool EditorSession::redo() {
    if (m_redo.empty()) {
        return false;
    }
    Snapshot snapshot = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back(Snapshot{snapshot.label, m_scene});
    m_scene = std::move(snapshot.scene);
    m_dirty = true;
    if (m_selection >= static_cast<int>(m_scene.entities.size())) {
        m_selection = static_cast<int>(m_scene.entities.size()) - 1;
    }
    return true;
}

} // namespace zy::editor
