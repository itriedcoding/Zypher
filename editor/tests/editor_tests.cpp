// Editor core tests. Kept separate from the engine test binary so the editor
// can be built and tested without Qt.
#include "core/EditorSession.h"

#include "zypher/core/FileSystem.h"

#include <cstdio>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* message) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "  FAIL %s\n", message);
    }
}

[[nodiscard]] std::string fixtureDirectory() {
    return zy::FileSystem::join(zy::FileSystem::temporaryDirectory(), "zypher-editor-tests");
}

[[nodiscard]] bool writeFixture() {
    const std::string directory = fixtureDirectory();
    if (!zy::FileSystem::createDirectories(directory)) {
        return false;
    }
    const std::string scene = R"({
  "name": "Editor Test",
  "entities": [
    { "name": "Ground", "mesh": "models/plane.obj",
      "transform": { "position": [0, 0, 0], "scale": [8, 1, 8] } },
    { "name": "Box", "mesh": "models/cube.obj",
      "transform": { "position": [0, 2, 0] } }
  ]
})";
    const std::string project = R"({
  "name": "Editor Test Project",
  "version": "0.1.0",
  "assetRoot": "assets",
  "output": "dist/test.zpkg",
  "scene": "scene.zscene",
  "assets": [{ "path": "models/cube.obj", "type": "mesh" }]
})";
    return zy::FileSystem::writeText(zy::FileSystem::join(directory, "scene.zscene"), scene).hasValue() &&
           zy::FileSystem::writeText(zy::FileSystem::join(directory, "zypher.project.json"), project).hasValue();
}

void testOpenAndEdit() {
    zy::editor::EditorSession session;
    const std::string projectPath = zy::FileSystem::join(fixtureDirectory(), "zypher.project.json");
    check(session.openProject(projectPath), "opens the fixture project");
    check(session.scene().entities.size() == 2, "loads two entities");
    check(session.projectName() == "Editor Test Project", "reads the project name");
    check(!session.isDirty(), "fresh session is clean");
    check(session.selection() == 0, "selects the first entity");

    check(session.setEntityPosition(1, zy::Vec3{1.0f, 3.0f, -2.0f}), "moves an entity");
    check(session.scene().entities[1].transform.position.x == 1.0f, "new position stored");
    check(session.isDirty(), "edit marks the session dirty");
    check(session.undoLabel() == "move Box", "undo label describes the edit");

    check(session.undo(), "undo succeeds");
    check(session.scene().entities[1].transform.position.y == 2.0f, "undo restores position");
    check(session.redo(), "redo succeeds");
    check(session.scene().entities[1].transform.position.y == 3.0f, "redo re-applies position");
}

void testAddRenameRemove() {
    zy::editor::EditorSession session;
    check(session.openProject(zy::FileSystem::join(fixtureDirectory(), "zypher.project.json")),
          "opens the fixture project for add/remove tests");

    check(session.addEntity("Box", "models/cube.obj"), "adds an entity with a unique name");
    check(session.scene().entities.size() == 3, "entity count grew");
    check(session.scene().entities[2].name == "Box 2", "duplicate name is suffixed");
    check(session.selection() == 2, "new entity is selected");

    check(!session.renameEntity(2, "Box"), "rejects duplicate rename");
    check(session.renameEntity(2, "Crate"), "renames to a free name");

    check(session.moveEntity(2, -1), "moves the entity up");
    check(session.scene().entities[1].name == "Crate", "entity order changed");

    check(session.removeEntity(1), "removes an entity");
    check(session.scene().entities.size() == 2, "entity count shrank");
    check(session.undo(), "undo removal");
    check(session.scene().entities.size() == 3, "undo restores the entity");

    session.select(99);
    check(session.selection() == -1, "out of range selection is ignored and clears the selection");
}

void testSaveAndReload() {
    zy::editor::EditorSession session;
    check(session.openProject(zy::FileSystem::join(fixtureDirectory(), "zypher.project.json")),
          "opens the fixture project for save tests");
    check(session.setEntityScale(0, zy::Vec3{4.0f, 1.0f, 4.0f}), "edits scale");
    check(session.saveScene(), "saves the scene");
    check(!session.isDirty(), "save clears the dirty flag");

    zy::editor::EditorSession reloaded;
    check(reloaded.openProject(zy::FileSystem::join(fixtureDirectory(), "zypher.project.json")),
          "reopens the saved project");
    check(reloaded.scene().entities[0].transform.scale.x == 4.0f, "saved scale round-trips");
    check(reloaded.undoLabel().empty(), "reloaded session has no history");
}

void testErrors() {
    zy::editor::EditorSession session;
    check(!session.openProject(zy::FileSystem::join(fixtureDirectory(), "missing.json")),
          "missing project is rejected");
    check(!session.lastError().empty(), "missing project reports an error");
    check(!session.saveScene(), "cannot save without an open project");
    check(!session.undo(), "undo on an empty history returns false");
}

} // namespace

int main() {
    if (!writeFixture()) {
        std::fprintf(stderr, "cannot create the editor test fixture\n");
        return 1;
    }

    testOpenAndEdit();
    testAddRenameRemove();
    testSaveAndReload();
    testErrors();

    std::printf("\n%d editor check(s), %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
