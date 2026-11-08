// Editor session: project loading, scene mutation, undo/redo and saving.
//
// Deliberately independent of Qt so it can be compiled and tested headlessly;
// the Qt/QML shell in editor/qt drives it through EditorController.
#pragma once

#include "zypher/scene/Scene.h"

#include <functional>
#include <string>
#include <vector>

namespace zy::editor {

class EditorSession {
public:
    // Loads zypher.project.json and the scene it references.
    [[nodiscard]] bool openProject(const std::string& projectPath);
    [[nodiscard]] bool saveScene();

    [[nodiscard]] const std::string& projectPath() const { return m_projectPath; }
    [[nodiscard]] const std::string& projectName() const { return m_projectName; }
    [[nodiscard]] const std::string& assetRoot() const { return m_assetRoot; }
    [[nodiscard]] const std::string& scenePath() const { return m_scenePath; }
    [[nodiscard]] const SceneDesc& scene() const { return m_scene; }
    [[nodiscard]] int selection() const { return m_selection; }
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] bool isOpen() const { return !m_projectPath.empty(); }

    void select(int index);

    [[nodiscard]] bool setEntityPosition(usize index, const Vec3& position);
    [[nodiscard]] bool setEntityScale(usize index, const Vec3& scale);
    [[nodiscard]] bool renameEntity(usize index, const std::string& name);
    [[nodiscard]] bool addEntity(const std::string& name, const std::string& meshPath);
    [[nodiscard]] bool removeEntity(usize index);
    [[nodiscard]] bool moveEntity(usize index, int delta);

    [[nodiscard]] bool canUndo() const { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.empty(); }
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();

    void markSaved() { m_dirty = false; }

private:
    struct Snapshot {
        std::string label;
        SceneDesc scene;
    };

    [[nodiscard]] bool fail(std::string message);
    void commit(const std::string& label, const std::function<void(SceneDesc&)>& edit);
    [[nodiscard]] bool validIndex(usize index) const;

    std::string m_projectPath;
    std::string m_projectName = "Untitled Project";
    std::string m_assetRoot = "assets";
    std::string m_scenePath;
    std::string m_lastError;
    SceneDesc m_scene;
    int m_selection = -1;
    bool m_dirty = false;
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    static constexpr usize kMaxUndo = 128;
};

} // namespace zy::editor
