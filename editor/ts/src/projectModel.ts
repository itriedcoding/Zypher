// Project manifest handling shared by the editor panels and the build tools.
// The asset id formula must match engine/src/assets/AssetId.cpp and the Rust
// cooker: FNV-1a 64 over the UTF-8 bytes of the normalized relative path.

export type AssetType = "mesh" | "texture" | "audio" | "shader" | "font" | "raw";

export interface ProjectAsset {
  path: string;
  type: AssetType;
}

export interface Project {
  name: string;
  version: string;
  assetRoot: string;
  output: string;
  scene: string;
  textureCompression: string;
  assets: ProjectAsset[];
}

const ASSET_TYPES: ReadonlySet<string> = new Set(["mesh", "texture", "audio", "shader", "font", "raw"]);

export class ProjectError extends Error {
  readonly issues: string[];

  constructor(issues: string[]) {
    super(issues.join("; "));
    this.name = "ProjectError";
    this.issues = issues;
  }
}

/** Normalizes to forward slashes and rejects paths that escape the asset root. */
export function normalizeAssetPath(path: string): string {
  const slashed = path.replace(/\\/g, "/");
  const parts: string[] = [];
  for (const part of slashed.split("/")) {
    if (part === "" || part === ".") {
      continue;
    }
    if (part === "..") {
      if (parts.length === 0) {
        throw new ProjectError([`asset path escapes the project: ${path}`]);
      }
      parts.pop();
      continue;
    }
    parts.push(part);
  }
  if (parts.length === 0) {
    throw new ProjectError([`asset path is empty: ${path}`]);
  }
  return parts.join("/");
}

export function fnv1a64(text: string): bigint {
  const prime = 0x100000001b3n;
  let hash = 0xcbf29ce484222325n;
  for (const byte of new TextEncoder().encode(text)) {
    hash ^= BigInt(byte);
    hash = BigInt.asUintN(64, hash * prime);
  }
  return hash;
}

export function assetId(path: string): bigint {
  return fnv1a64(normalizeAssetPath(path));
}

export function formatAssetId(id: bigint): string {
  return `0x${id.toString(16).padStart(16, "0")}`;
}

function requireString(source: Record<string, unknown>, key: string, fallback: string, issues: string[]): string {
  const value = source[key];
  if (value === undefined) {
    return fallback;
  }
  if (typeof value !== "string" || value.trim() === "") {
    issues.push(`'${key}' must be a non-empty string`);
    return fallback;
  }
  return value;
}

/** Validates a parsed zypher.project.json document. */
export function parseProject(value: unknown): Project {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new ProjectError(["project root must be an object"]);
  }
  const source = value as Record<string, unknown>;
  const issues: string[] = [];
  const project: Project = {
    name: requireString(source, "name", "Untitled Project", issues),
    version: requireString(source, "version", "0.1.1", issues),
    assetRoot: requireString(source, "assetRoot", "assets", issues),
    output: requireString(source, "output", "dist/assets.zpkg", issues),
    scene: requireString(source, "scene", "scene.zscene", issues),
    textureCompression: requireString(source, "textureCompression", "none", issues),
    assets: [],
  };

  const assets = source["assets"];
  if (!Array.isArray(assets) || assets.length === 0) {
    issues.push("'assets' must be a non-empty array");
  } else {
    const seen = new Set<string>();
    assets.forEach((entry: unknown, index: number) => {
      if (typeof entry === "string") {
        issues.push(`asset ${index} must be an object with 'path' and 'type'`);
        return;
      }
      if (typeof entry !== "object" || entry === null) {
        issues.push(`asset ${index} is not an object`);
        return;
      }
      const record = entry as Record<string, unknown>;
      const rawPath = record["path"];
      if (typeof rawPath !== "string" || rawPath.trim() === "") {
        issues.push(`asset ${index} has no path`);
        return;
      }
      const type = typeof record["type"] === "string" ? record["type"] : "raw";
      if (!ASSET_TYPES.has(type)) {
        issues.push(`asset ${index} has unknown type '${type}'`);
        return;
      }
      let normalized: string;
      try {
        normalized = normalizeAssetPath(rawPath);
      } catch (error) {
        issues.push(error instanceof Error ? error.message : String(error));
        return;
      }
      if (seen.has(normalized)) {
        issues.push(`duplicate asset path: ${normalized}`);
        return;
      }
      seen.add(normalized);
      project.assets.push({ path: normalized, type: type as AssetType });
    });
  }

  if (issues.length > 0) {
    throw new ProjectError(issues);
  }
  return project;
}

export function parseProjectText(text: string): Project {
  let document: unknown;
  try {
    document = JSON.parse(text);
  } catch (error) {
    throw new ProjectError([`project is not valid JSON: ${(error as Error).message}`]);
  }
  return parseProject(document);
}

/** Resolves a project-relative path against the directory holding the manifest. */
export function resolveProjectPath(projectDirectory: string, relative: string): string {
  const base = projectDirectory.replace(/[\\/]+$/, "");
  return `${base}/${normalizeAssetPath(relative)}`;
}

export function assetsByType(project: Project): Map<AssetType, ProjectAsset[]> {
  const grouped = new Map<AssetType, ProjectAsset[]>();
  for (const asset of project.assets) {
    const list = grouped.get(asset.type) ?? [];
    list.push(asset);
    grouped.set(asset.type, list);
  }
  return grouped;
}
