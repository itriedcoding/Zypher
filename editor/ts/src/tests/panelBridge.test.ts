import assert from "node:assert/strict";
import { test } from "node:test";

import { SceneBridge, decodeRequest, type PanelResponse } from "../panelBridge.js";
import { SceneDocument } from "../sceneDocument.js";

const sceneText = JSON.stringify({
  name: "Bridge Scene",
  entities: [{ name: "Box", transform: { position: [0, 1, 0] } }],
});

function send(bridge: SceneBridge, command: unknown): PanelResponse {
  return JSON.parse(bridge.handle(JSON.stringify({ id: 1, command }))) as PanelResponse;
}

test("requests are validated", () => {
  assert.deepEqual(decodeRequest({ id: 3, command: { type: "select", index: 0 } }), {
    id: 3,
    command: { type: "select", index: 0 },
  });
  assert.throws(() => decodeRequest({ id: 1, command: { type: "explode" } }), /unknown command type/);
  assert.throws(() => decodeRequest({ id: -1, command: { type: "undo" } }), /id must be/);
  assert.throws(() => decodeRequest({ id: 1, command: { type: "setTransform", index: 0, position: [1, 2] } }), /3 numbers/);
});

test("selecting and editing emit scene events", () => {
  const bridge = new SceneBridge(SceneDocument.parse(sceneText));
  const selected = send(bridge, { type: "select", index: 0 });
  assert.equal(selected.ok, true);
  assert.deepEqual(selected.event, { kind: "selectionChanged", index: 0, name: "Box" });

  const moved = send(bridge, { type: "setTransform", index: 0, position: [2, 3, 4] });
  assert.equal(moved.ok, true);
  assert.equal(moved.event?.kind, "sceneChanged");
  if (moved.event?.kind === "sceneChanged") {
    assert.equal(moved.event.dirty, true);
    assert.deepEqual(moved.event.entityNames, ["Box"]);
  }
  assert.deepEqual(bridge.scene.entities[0]?.transform.position, [2, 3, 4]);
});

test("undo and redo flow through the bridge", () => {
  const bridge = new SceneBridge(SceneDocument.parse(sceneText));
  send(bridge, { type: "addEntity", name: "Enemy" });
  assert.deepEqual(bridge.scene.entities.map((entity) => entity.name), ["Box", "Enemy"]);

  const undone = send(bridge, { type: "undo" });
  assert.equal(undone.ok, true);
  assert.equal(undone.event?.kind, "undoState");
  assert.deepEqual(bridge.scene.entities.map((entity) => entity.name), ["Box"]);

  send(bridge, { type: "redo" });
  assert.deepEqual(bridge.scene.entities.map((entity) => entity.name), ["Box", "Enemy"]);
});

test("save writes through the callback and clears the dirty flag", () => {
  let saved = "";
  const bridge = new SceneBridge(SceneDocument.parse(sceneText), { onSave: (text) => { saved = text; } });
  send(bridge, { type: "removeEntity", index: 0 });
  const response = send(bridge, { type: "save" });
  assert.equal(response.ok, true);
  assert.equal(response.event?.kind, "saved");
  assert.ok(saved.includes("\"entities\": []"));
  assert.ok(bridge.scene.isDirty === false);
});

test("invalid messages fail without throwing", () => {
  const bridge = new SceneBridge(SceneDocument.parse(sceneText));
  const malformed = JSON.parse(bridge.handle("{not json")) as PanelResponse;
  assert.equal(malformed.ok, false);
  assert.match(malformed.error ?? "", /invalid JSON/);

  const outOfRange = JSON.parse(bridge.handle(JSON.stringify({ id: 7, command: { type: "select", index: 99 } }))) as PanelResponse;
  assert.equal(outOfRange.id, 7);
  assert.equal(outOfRange.ok, false);
  assert.match(outOfRange.error ?? "", /out of range/);
});
