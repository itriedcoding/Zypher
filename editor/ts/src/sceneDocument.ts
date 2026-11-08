// Editable view of a .zscene file. Mirrors engine/src/scene/Scene.cpp: the JSON
// shape written here must stay loadable by the runtime.

import { UndoStack, type Command } from "./undoStack.js";

export type Vec3Tuple = [number, number, number];
export type Vec4Tuple = [number, number, number, number];

export interface Transform {
  position: Vec3Tuple;
  rotation: Vec3Tuple;
  scale: Vec3Tuple;
}

export interface Material {
  baseColor: Vec4Tuple;
  roughness: number;
  metallic: number;
}

export interface RigidBody {
  isStatic: boolean;
  mass: number;
  restitution: number;
  halfExtents?: Vec3Tuple;
}

export interface Entity {
  name: string;
  mesh?: string;
  transform: Transform;
  material?: Material;
  rigidbody?: RigidBody;
  script?: string;
  castShadows?: boolean;
}

export interface SceneEnvironment {
  clearColor: Vec4Tuple;
  light: { direction: Vec3Tuple; color: Vec3Tuple; intensity: number };
}

export interface SceneCamera {
  position: Vec3Tuple;
  target: Vec3Tuple;
  fov: number;
  near: number;
  far: number;
}

export interface SceneData {
  name: string;
  environment: SceneEnvironment;
  camera: SceneCamera;
  entities: Entity[];
}

export class SceneError extends Error {
  readonly issues: string[];

  constructor(issues: string[]) {
    super(issues.join("; "));
    this.name = "SceneError";
    this.issues = issues;
  }
}

function isNumberArray(value: unknown, length: number): value is number[] {
  return Array.isArray(value) && value.length === length && value.every((item) => typeof item === "number" && Number.isFinite(item));
}

function requireVec3(value: unknown, fallback: Vec3Tuple, label: string, issues: string[]): Vec3Tuple {
  if (value === undefined) {
    return [...fallback];
  }
  if (!isNumberArray(value, 3)) {
    issues.push(`${label} must be an array of 3 numbers`);
    return [...fallback];
  }
  return [value[0]!, value[1]!, value[2]!];
}

function requireVec4(value: unknown, fallback: Vec4Tuple, label: string, issues: string[]): Vec4Tuple {
  if (value === undefined) {
    return [...fallback];
  }
  if (!isNumberArray(value, 4)) {
    issues.push(`${label} must be an array of 4 numbers`);
    return [...fallback];
  }
  return [value[0]!, value[1]!, value[2]!, value[3]!];
}

function requireNumber(value: unknown, fallback: number, label: string, issues: string[]): number {
  if (value === undefined) {
    return fallback;
  }
  if (typeof value !== "number" || !Number.isFinite(value)) {
    issues.push(`${label} must be a finite number`);
    return fallback;
  }
  return value;
}

function parseEntity(value: unknown, index: number, issues: string[]): Entity {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    issues.push(`entity ${index} is not an object`);
    return defaultEntity(`Entity ${index}`);
  }
  const source = value as Record<string, unknown>;
  const name = typeof source["name"] === "string" && source["name"].trim() !== "" ? source["name"] : `Entity ${index}`;
  const transformSource = typeof source["transform"] === "object" && source["transform"] !== null
    ? (source["transform"] as Record<string, unknown>)
    : {};

  const entity: Entity = {
    name,
    transform: {
      position: requireVec3(transformSource["position"], [0, 0, 0], `${name}.position`, issues),
      rotation: requireVec3(transformSource["rotation"], [0, 0, 0], `${name}.rotation`, issues),
      scale: requireVec3(transformSource["scale"], [1, 1, 1], `${name}.scale`, issues),
    },
  };

  if (typeof source["mesh"] === "string" && source["mesh"] !== "") {
    entity.mesh = source["mesh"];
  }
  if (typeof source["script"] === "string" && source["script"] !== "") {
    entity.script = source["script"];
  }
  if (typeof source["castShadows"] === "boolean") {
    entity.castShadows = source["castShadows"];
  }

  const materialSource = source["material"];
  if (typeof materialSource === "object" && materialSource !== null) {
    const material = materialSource as Record<string, unknown>;
    entity.material = {
      baseColor: requireVec4(material["baseColor"], [1, 1, 1, 1], `${name}.baseColor`, issues),
      roughness: requireNumber(material["roughness"], 0.5, `${name}.roughness`, issues),
      metallic: requireNumber(material["metallic"], 0, `${name}.metallic`, issues),
    };
  }

  const bodySource = source["rigidbody"];
  if (typeof bodySource === "object" && bodySource !== null) {
    const body = bodySource as Record<string, unknown>;
    entity.rigidbody = {
      isStatic: body["static"] === true,
      mass: requireNumber(body["mass"], 1, `${name}.mass`, issues),
      restitution: requireNumber(body["restitution"], 0.2, `${name}.restitution`, issues),
    };
    if (body["halfExtents"] !== undefined) {
      entity.rigidbody.halfExtents = requireVec3(body["halfExtents"], [0.5, 0.5, 0.5], `${name}.halfExtents`, issues);
    }
  }

  return entity;
}

export function defaultEntity(name: string): Entity {
  return { name, transform: { position: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1] } };
}

export function parseScene(value: unknown): SceneData {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new SceneError(["scene root must be an object"]);
  }
  const source = value as Record<string, unknown>;
  const issues: string[] = [];

  const environmentSource = typeof source["environment"] === "object" && source["environment"] !== null
    ? (source["environment"] as Record<string, unknown>)
    : {};
  const lightSource = typeof environmentSource["light"] === "object" && environmentSource["light"] !== null
    ? (environmentSource["light"] as Record<string, unknown>)
    : {};
  const cameraSource = typeof source["camera"] === "object" && source["camera"] !== null
    ? (source["camera"] as Record<string, unknown>)
    : {};

  const entitiesSource = source["entities"];
  if (!Array.isArray(entitiesSource)) {
    throw new SceneError(["scene is missing an 'entities' array"]);
  }

  const entities = entitiesSource.map((entry, index) => parseEntity(entry, index, issues));
  const names = new Set<string>();
  for (const entity of entities) {
    if (names.has(entity.name)) {
      issues.push(`duplicate entity name: ${entity.name}`);
    }
    names.add(entity.name);
  }
  if (issues.length > 0) {
    throw new SceneError(issues);
  }

  return {
    name: typeof source["name"] === "string" && source["name"] !== "" ? source["name"] : "Untitled",
    environment: {
      clearColor: requireVec4(environmentSource["clearColor"], [0.06, 0.07, 0.09, 1], "environment.clearColor", issues),
      light: {
        direction: requireVec3(lightSource["direction"], [-0.4, -1, -0.55], "light.direction", issues),
        color: requireVec3(lightSource["color"], [1, 1, 1], "light.color", issues),
        intensity: requireNumber(lightSource["intensity"], 1, "light.intensity", issues),
      },
    },
    camera: {
      position: requireVec3(cameraSource["position"], [0, 2.5, 7], "camera.position", issues),
      target: requireVec3(cameraSource["target"], [0, 1, 0], "camera.target", issues),
      fov: requireNumber(cameraSource["fov"], 55, "camera.fov", issues),
      near: requireNumber(cameraSource["near"], 0.1, "camera.near", issues),
      far: requireNumber(cameraSource["far"], 500, "camera.far", issues),
    },
    entities,
  };
}

function cloneEntity(entity: Entity): Entity {
  return JSON.parse(JSON.stringify(entity)) as Entity;
}

export class SceneDocument {
  private scene: SceneData;
  private revision = 0;
  private savedRevision = 0;
  readonly undoStack: UndoStack;

  constructor(scene: SceneData, undoStack: UndoStack = new UndoStack()) {
    this.scene = scene;
    this.undoStack = undoStack;
  }

  static parse(text: string): SceneDocument {
    let document: unknown;
    try {
      document = JSON.parse(text);
    } catch (error) {
      throw new SceneError([`scene is not valid JSON: ${(error as Error).message}`]);
    }
    return new SceneDocument(parseScene(document));
  }

  get name(): string {
    return this.scene.name;
  }

  get entities(): readonly Entity[] {
    return this.scene.entities;
  }

  get environment(): SceneEnvironment {
    return this.scene.environment;
  }

  get camera(): SceneCamera {
    return this.scene.camera;
  }

  get currentRevision(): number {
    return this.revision;
  }

  get isDirty(): boolean {
    return this.revision !== this.savedRevision;
  }

  markSaved(): void {
    this.savedRevision = this.revision;
  }

  findEntity(name: string): number {
    return this.scene.entities.findIndex((entity) => entity.name === name);
  }

  entityAt(index: number): Entity {
    const entity = this.scene.entities[index];
    if (entity === undefined) {
      throw new RangeError(`entity index out of range: ${index}`);
    }
    return entity;
  }

  addEntity(entity: Entity = defaultEntity(this.uniqueName("Entity"))): number {
    const index = this.scene.entities.length;
    this.mutate(`add ${entity.name}`, () => this.scene.entities.push(cloneEntity(entity)));
    return index;
  }

  removeEntity(index: number): void {
    this.entityAt(index);
    const removed = cloneEntity(this.entityAt(index));
    this.mutate(`remove ${removed.name}`, () => {
      this.scene.entities.splice(index, 1);
    });
  }

  renameEntity(index: number, name: string): void {
    if (name.trim() === "") {
      throw new SceneError(["entity name must not be empty"]);
    }
    const collision = this.scene.entities.findIndex((entity, position) => entity.name === name && position !== index);
    if (collision !== -1) {
      throw new SceneError([`duplicate entity name: ${name}`]);
    }
    this.entityAt(index);
    this.mutate(`rename to ${name}`, () => {
      this.entityAt(index).name = name;
    });
  }

  setTransform(index: number, patch: Partial<Transform>): void {
    this.entityAt(index);
    const before = cloneEntity(this.entityAt(index)).transform;
    const after: Transform = {
      position: [...(patch.position ?? before.position)] as Vec3Tuple,
      rotation: [...(patch.rotation ?? before.rotation)] as Vec3Tuple,
      scale: [...(patch.scale ?? before.scale)] as Vec3Tuple,
    };
    this.mutate(`transform ${this.entityAt(index).name}`, () => {
      this.entityAt(index).transform = after;
    });
  }

  moveEntity(index: number, delta: number): number {
    const target = index + delta;
    if (index < 0 || index >= this.scene.entities.length || target < 0 || target >= this.scene.entities.length || delta === 0) {
      return index;
    }
    this.mutate("reorder entity", () => {
      const [entity] = this.scene.entities.splice(index, 1);
      if (entity !== undefined) {
        this.scene.entities.splice(target, 0, entity);
      }
    });
    return target;
  }

  undo(): string | null {
    const label = this.undoStack.undo();
    if (label !== null) {
      this.revision += 1;
    }
    return label;
  }

  redo(): string | null {
    const label = this.undoStack.redo();
    if (label !== null) {
      this.revision += 1;
    }
    return label;
  }

  private uniqueName(base: string): string {
    if (this.findEntity(base) === -1) {
      return base;
    }
    let counter = 2;
    while (this.findEntity(`${base} ${counter}`) !== -1) {
      counter += 1;
    }
    return `${base} ${counter}`;
  }

  private mutate(label: string, apply: () => void): void {
    // Snapshot based commands: scenes are small and this keeps every edit
    // (including reordering and bulk transform changes) exactly reversible.
    const before = JSON.parse(JSON.stringify(this.scene)) as SceneData;
    apply();
    const after = JSON.parse(JSON.stringify(this.scene)) as SceneData;
    const command: Command = {
      label,
      apply: () => {
        this.scene = JSON.parse(JSON.stringify(after)) as SceneData;
      },
      revert: () => {
        this.scene = JSON.parse(JSON.stringify(before)) as SceneData;
      },
    };
    this.undoStack.execute(command);
    this.revision += 1;
  }

  toData(): SceneData {
    return JSON.parse(JSON.stringify(this.scene)) as SceneData;
  }

  toText(): string {
    return `${JSON.stringify(this.toData(), null, 2)}\n`;
  }
}
