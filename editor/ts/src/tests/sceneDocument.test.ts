import assert from "node:assert/strict";
import { test } from "node:test";

import { SceneDocument, SceneError, parseScene } from "../sceneDocument.js";

const sceneText = JSON.stringify({
  name: "Test Scene",
  environment: { clearColor: [0.1, 0.1, 0.1, 1], light: { direction: [-1, -1, 0], color: [1, 1, 1], intensity: 1 } },
  camera: { position: [0, 2, 6], target: [0, 1, 0], fov: 60, near: 0.1, far: 100 },
  entities: [
    { name: "Ground", mesh: "models/plane.obj", transform: { position: [0, 0, 0], scale: [10, 1, 10] } },
    { name: "Player", mesh: "models/cube.obj", transform: { position: [0, 1, 0] }, script: "PlayerController" },
  ],
});

test("scenes parse and round-trip", () => {
  const document = SceneDocument.parse(sceneText);
  assert.equal(document.name, "Test Scene");
  assert.equal(document.entities.length, 2);
  const reparsed = SceneDocument.parse(document.toText());
  assert.deepEqual(reparsed.entities.map((entity) => entity.name), ["Ground", "Player"]);
  assert.equal(reparsed.entities[1]?.script, "PlayerController");
});

test("missing entity arrays are rejected", () => {
  assert.throws(() => parseScene({ name: "empty" }), SceneError);
  assert.throws(() => SceneDocument.parse("[]"), SceneError);
});

test("invalid transforms and duplicate names report issues", () => {
  assert.throws(
    () => parseScene({ entities: [{ name: "A", transform: { position: [0, 0] } }] }),
    /position must be an array of 3 numbers/,
  );
  assert.throws(
    () => parseScene({ entities: [{ name: "A" }, { name: "A" }] }),
    /duplicate entity name: A/,
  );
});

test("transforms edit and undo restores the previous state", () => {
  const document = SceneDocument.parse(sceneText);
  const original = document.entities[1]?.transform.position;
  document.setTransform(1, { position: [3, 4, 5] });
  assert.deepEqual(document.entities[1]?.transform.position, [3, 4, 5]);
  assert.ok(document.isDirty);

  assert.equal(document.undo(), "transform Player");
  assert.deepEqual(document.entities[1]?.transform.position, original);
  assert.equal(document.undoStack.canRedo, true);

  assert.equal(document.redo(), "transform Player");
  assert.deepEqual(document.entities[1]?.transform.position, [3, 4, 5]);
});

test("entities can be added, renamed, reordered and removed with undo", () => {
  const document = SceneDocument.parse(sceneText);
  const index = document.addEntity();
  assert.equal(index, 2);
  assert.equal(document.entities[2]?.name, "Entity");

  document.renameEntity(2, "Enemy");
  assert.equal(document.entities[2]?.name, "Enemy");
  assert.throws(() => document.renameEntity(2, "Player"), /duplicate entity name/);
  assert.throws(() => document.renameEntity(2, "  "), /must not be empty/);

  assert.equal(document.moveEntity(2, -1), 1);
  assert.deepEqual(document.entities.map((entity) => entity.name), ["Ground", "Enemy", "Player"]);

  document.removeEntity(1);
  assert.deepEqual(document.entities.map((entity) => entity.name), ["Ground", "Player"]);

  document.undo();
  assert.deepEqual(document.entities.map((entity) => entity.name), ["Ground", "Enemy", "Player"]);
  document.undo();
  assert.deepEqual(document.entities.map((entity) => entity.name), ["Ground", "Player", "Enemy"]);
  document.undo();
  assert.deepEqual(document.entities.map((entity) => entity.name), ["Ground", "Player", "Entity"]);
});

test("saving clears the dirty flag", () => {
  const document = SceneDocument.parse(sceneText);
  document.setTransform(0, { scale: [2, 2, 2] });
  assert.ok(document.isDirty);
  document.markSaved();
  assert.ok(!document.isDirty);
});
