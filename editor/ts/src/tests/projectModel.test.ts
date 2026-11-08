import assert from "node:assert/strict";
import { test } from "node:test";

import {
  ProjectError,
  assetId,
  assetsByType,
  formatAssetId,
  normalizeAssetPath,
  parseProject,
  parseProjectText,
  resolveProjectPath,
} from "../projectModel.js";


const sample = {
  name: "Zypher Demo",
  version: "0.1.0",
  assetRoot: "assets",
  output: "dist/demo.zpkg",
  scene: "scene.zscene",
  textureCompression: "none",
  assets: [
    { path: "models/cube.obj", type: "mesh" },
    { path: "models/plane.obj", type: "mesh" },
    { path: "textures/grid.png", type: "texture" },
  ],
};

test("asset ids match the engine and Rust cooker", () => {
  // Verified against engine/src/assets/AssetId.cpp and tools/assetcooker.
  assert.equal(assetId("models/cube.obj"), 0x47610562bdfd77f2n);
  assert.equal(formatAssetId(assetId("models/cube.obj")), "0x47610562bdfd77f2");
});

test("asset paths normalize separators and dot segments", () => {
  assert.equal(normalizeAssetPath("models\\cube.obj"), "models/cube.obj");
  assert.equal(normalizeAssetPath("./models//cube.obj"), "models/cube.obj");
  assert.equal(assetId("models\\cube.obj"), assetId("models/cube.obj"));
  assert.equal(assetId("models/./cube.obj"), assetId("models/cube.obj"));
  assert.notEqual(assetId("models/cube.obj"), assetId("models/Cube.obj"));
});

test("paths that escape the project are rejected", () => {
  assert.throws(() => normalizeAssetPath("../outside.png"), ProjectError);
  assert.throws(() => normalizeAssetPath("models/../../outside.png"), ProjectError);
});

test("project manifests parse with typed assets", () => {
  const project = parseProject(sample);
  assert.equal(project.name, "Zypher Demo");
  assert.equal(project.assets.length, 3);
  const grouped = assetsByType(project);
  assert.equal(grouped.get("mesh")?.length, 2);
  assert.equal(grouped.get("texture")?.length, 1);
});

test("invalid manifests report every issue", () => {
  assert.throws(() => parseProject({}), (error: unknown) => {
    assert.ok(error instanceof ProjectError);
    assert.ok(error.issues.some((issue) => issue.includes("'assets'")));
    return true;
  });

  assert.throws(
    () => parseProject({ ...sample, assets: [{ path: "a.png", type: "nope" }, { path: "a.png", type: "mesh" }] }),
    /unknown type 'nope'/,
  );

  assert.throws(() => parseProjectText("{ not json"), /not valid JSON/);
});

test("duplicate asset paths are rejected after normalization", () => {
  assert.throws(
    () => parseProject({ ...sample, assets: [{ path: "models/cube.obj", type: "mesh" }, { path: "./models//cube.obj", type: "mesh" }] }),
    /duplicate asset path/,
  );
});

test("project paths resolve relative to the manifest", () => {
  assert.equal(resolveProjectPath("samples/demo/", "scene.zscene"), "samples/demo/scene.zscene");
  assert.equal(resolveProjectPath("samples/demo", "dist/demo.zpkg"), "samples/demo/dist/demo.zpkg");
});
