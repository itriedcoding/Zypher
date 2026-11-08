// Command bridge between the QML panels and the TypeScript scene logic.
//
// QML sends JSON strings, the bridge validates them (UI input is untrusted and
// may come from a Monaco panel extension), mutates the scene document and
// returns a JSON response plus an event describing what changed.

import { SceneDocument, SceneError, type Entity, type Transform, type Vec3Tuple } from "./sceneDocument.js";

export type PanelCommand =
  | { type: "select"; index: number }
  | { type: "addEntity"; name?: string }
  | { type: "removeEntity"; index: number }
  | { type: "renameEntity"; index: number; name: string }
  | { type: "setTransform"; index: number; position?: Vec3Tuple; rotation?: Vec3Tuple; scale?: Vec3Tuple }
  | { type: "moveEntity"; index: number; delta: number }
  | { type: "undo" }
  | { type: "redo" }
  | { type: "save" };

export interface PanelRequest {
  id: number;
  command: PanelCommand;
}

export type PanelEvent =
  | { kind: "selectionChanged"; index: number; name: string | null }
  | { kind: "sceneChanged"; revision: number; dirty: boolean; entityNames: string[]; selected: number }
  | { kind: "saved"; revision: number }
  | { kind: "undoState"; canUndo: boolean; canRedo: boolean; undoLabel: string | null; redoLabel: string | null };

export interface PanelResponse {
  id: number;
  ok: boolean;
  error?: string;
  event?: PanelEvent;
}

export class BridgeError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "BridgeError";
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function readVec3(value: unknown, label: string): Vec3Tuple {
  if (!Array.isArray(value) || value.length !== 3 || !value.every((item) => typeof item === "number" && Number.isFinite(item))) {
    throw new BridgeError(`${label} must be an array of 3 numbers`);
  }
  return [value[0] as number, value[1] as number, value[2] as number];
}

function readIndex(value: unknown, label: string): number {
  if (typeof value !== "number" || !Number.isInteger(value) || value < 0) {
    throw new BridgeError(`${label} must be a non-negative integer`);
  }
  return value;
}

export function decodeRequest(value: unknown): PanelRequest {
  if (!isRecord(value)) {
    throw new BridgeError("request must be a JSON object");
  }
  const id = readIndex(value["id"], "id");
  const command = value["command"];
  if (!isRecord(command)) {
    throw new BridgeError("request.command must be an object");
  }
  const type = command["type"];
  if (typeof type !== "string") {
    throw new BridgeError("command.type must be a string");
  }

  switch (type) {
    case "select":
      return { id, command: { type, index: readIndex(command["index"], "index") } };
    case "addEntity": {
      const name = command["name"];
      if (name !== undefined && (typeof name !== "string" || name.trim() === "")) {
        throw new BridgeError("name must be a non-empty string");
      }
      return { id, command: name === undefined ? { type } : { type, name } };
    }
    case "removeEntity":
      return { id, command: { type, index: readIndex(command["index"], "index") } };
    case "renameEntity": {
      const name = command["name"];
      if (typeof name !== "string" || name.trim() === "") {
        throw new BridgeError("name must be a non-empty string");
      }
      return { id, command: { type, index: readIndex(command["index"], "index"), name } };
    }
    case "setTransform": {
      const index = readIndex(command["index"], "index");
      const patch: Partial<Transform> = {};
      if (command["position"] !== undefined) {
        patch.position = readVec3(command["position"], "position");
      }
      if (command["rotation"] !== undefined) {
        patch.rotation = readVec3(command["rotation"], "rotation");
      }
      if (command["scale"] !== undefined) {
        patch.scale = readVec3(command["scale"], "scale");
      }
      return { id, command: { type, index, ...patch } };
    }
    case "moveEntity": {
      const delta = command["delta"];
      if (typeof delta !== "number" || !Number.isInteger(delta) || delta === 0) {
        throw new BridgeError("delta must be a non-zero integer");
      }
      return { id, command: { type, index: readIndex(command["index"], "index"), delta } };
    }
    case "undo":
    case "redo":
    case "save":
      return { id, command: { type } };
    default:
      throw new BridgeError(`unknown command type: ${type}`);
  }
}

export interface BridgeOptions {
  onSave?: (sceneText: string) => void;
}

export class SceneBridge {
  private readonly document: SceneDocument;
  private readonly options: BridgeOptions;
  private selected = -1;

  constructor(document: SceneDocument, options: BridgeOptions = {}) {
    this.document = document;
    this.options = options;
    const first = document.entities[0];
    this.selected = first === undefined ? -1 : 0;
  }

  get selection(): number {
    return this.selected;
  }

  get scene(): SceneDocument {
    return this.document;
  }

  /** Handles one JSON request. Never throws: failures become `ok: false`. */
  handle(json: string): string {
    let request: PanelRequest | null = null;
    try {
      let parsed: unknown;
      try {
        parsed = JSON.parse(json);
      } catch (error) {
        throw new BridgeError(`invalid JSON: ${(error as Error).message}`);
      }
      request = decodeRequest(parsed);
      const response = this.dispatch(request);
      return JSON.stringify(response);
    } catch (error) {
      const response: PanelResponse = {
        id: request?.id ?? 0,
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      };
      return JSON.stringify(response);
    }
  }

  private dispatch(request: PanelRequest): PanelResponse {
    const command = request.command;
    switch (command.type) {
      case "select": {
        const entity = this.document.entities[command.index];
        if (entity === undefined) {
          throw new SceneError([`entity index out of range: ${command.index}`]);
        }
        this.selected = command.index;
        return { id: request.id, ok: true, event: { kind: "selectionChanged", index: command.index, name: entity.name } };
      }
      case "addEntity": {
        const index = this.document.addEntity(command.name === undefined ? undefined : {
          name: command.name,
          transform: { position: [0, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1] },
        } as Entity);
        this.selected = index;
        return { id: request.id, ok: true, event: this.sceneEvent() };
      }
      case "removeEntity": {
        this.document.removeEntity(command.index);
        if (this.selected >= this.document.entities.length) {
          this.selected = this.document.entities.length - 1;
        }
        return { id: request.id, ok: true, event: this.sceneEvent() };
      }
      case "renameEntity":
        this.document.renameEntity(command.index, command.name);
        return { id: request.id, ok: true, event: this.sceneEvent() };
      case "setTransform":
        this.document.setTransform(command.index, {
          position: command.position,
          rotation: command.rotation,
          scale: command.scale,
        });
        return { id: request.id, ok: true, event: this.sceneEvent() };
      case "moveEntity":
        this.selected = this.document.moveEntity(command.index, command.delta);
        return { id: request.id, ok: true, event: this.sceneEvent() };
      case "undo": {
        const label = this.document.undo();
        return { id: request.id, ok: true, event: this.historyEvent(label === null ? "nothing to undo" : label) };
      }
      case "redo": {
        const label = this.document.redo();
        return { id: request.id, ok: true, event: this.historyEvent(label === null ? "nothing to redo" : label) };
      }
      case "save": {
        const text = this.document.toText();
        this.document.markSaved();
        this.options.onSave?.(text);
        return { id: request.id, ok: true, event: { kind: "saved", revision: this.document.currentRevision } };
      }
      default:
        throw new BridgeError("unreachable command");
    }
  }

  private sceneEvent(): PanelEvent {
    const selectedEntity = this.document.entities[this.selected];
    return {
      kind: "sceneChanged",
      revision: this.document.currentRevision,
      dirty: this.document.isDirty,
      entityNames: this.document.entities.map((entity) => entity.name),
      selected: selectedEntity === undefined ? -1 : this.selected,
    };
  }

  private historyEvent(label: string): PanelEvent {
    void label;
    return {
      kind: "undoState",
      canUndo: this.document.undoStack.canUndo,
      canRedo: this.document.undoStack.canRedo,
      undoLabel: this.document.undoStack.undoLabel,
      redoLabel: this.document.undoStack.redoLabel,
    };
  }
}
