#!/usr/bin/env python3
"""Validate Zypher project assets before cooking.

Checks that every asset listed in a project manifest exists, has a supported
extension, is non-empty and carries a valid header for its type. Exits non-zero
when a problem is found so it can gate CI.

Usage:
    python3 tools/scripts/validate_assets.py samples/demo/zypher.project.json
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
JPEG_SIGNATURE = b"\xff\xd8"
WEBP_SIGNATURE = b"RIFF"
KTX2_IDENTIFIER = bytes.fromhex("ab4b5458203230bb0d0a1a0a")
EXR_SIGNATURE = bytes.fromhex("762f3101")
MESH_MAGIC = b"ZYMESH01"

MESH_EXTENSIONS = {".obj", ".gltf", ".glb", ".fbx"}
TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp", ".exr", ".ktx2", ".ktx"}
AUDIO_EXTENSIONS = {".wav", ".ogg", ".opus", ".flac"}
SUPPORTED = MESH_EXTENSIONS | TEXTURE_EXTENSIONS | AUDIO_EXTENSIONS | {
    ".json",
    ".zscene",
    ".ttf",
    ".otf",
}


class Validator:
    def __init__(self, project_path: Path) -> None:
        self.project_path = project_path
        self.errors: list[str] = []
        self.warnings: list[str] = []

    @property
    def project_dir(self) -> Path:
        return self.project_path.parent

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def check_texture(self, path: Path, data: bytes) -> None:
        suffix = path.suffix.lower()
        if suffix == ".png" and not data.startswith(PNG_SIGNATURE):
            self.error(f"{path}: not a PNG file (bad signature)")
        elif suffix in {".jpg", ".jpeg"} and not data.startswith(JPEG_SIGNATURE):
            self.error(f"{path}: not a JPEG file (bad signature)")
        elif suffix == ".webp" and not (data.startswith(WEBP_SIGNATURE) and data[8:12] == b"WEBP"):
            self.error(f"{path}: not a WebP file (bad header)")
        elif suffix == ".ktx2" and not data.startswith(KTX2_IDENTIFIER):
            self.error(f"{path}: not a KTX2 file (bad identifier)")
        elif suffix == ".exr" and not data.startswith(EXR_SIGNATURE):
            self.error(f"{path}: not an OpenEXR file (bad magic)")

    def check_audio(self, path: Path, data: bytes) -> None:
        suffix = path.suffix.lower()
        expected = {
            ".wav": (b"RIFF", b"WAVE"),
            ".flac": (b"fLaC", b""),
            ".ogg": (b"OggS", b""),
            ".opus": (b"OggS", b""),
        }[suffix]
        if not data.startswith(expected[0]):
            self.error(f"{path}: bad {suffix} signature")
        elif expected[1] and data[8:12] != expected[1]:
            self.error(f"{path}: RIFF container is not WAVE")

    def check_mesh(self, path: Path, data: bytes) -> None:
        suffix = path.suffix.lower()
        if suffix == ".obj":
            text = data.decode("utf-8", errors="replace")
            if not any(line.lstrip().startswith("v ") for line in text.splitlines()):
                self.error(f"{path}: OBJ has no vertices")
            if not any(line.lstrip().startswith("f ") for line in text.splitlines()):
                self.error(f"{path}: OBJ has no faces")
        elif suffix in {".gltf", ".glb"}:
            if suffix == ".gltf":
                try:
                    json.loads(data.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as error:
                    self.error(f"{path}: invalid glTF JSON ({error})")
            else:
                if len(data) < 12 or struct.unpack_from("<I", data, 0)[0] != 0x46546C67:
                    self.error(f"{path}: not a GLB file (bad magic)")
        elif suffix == ".fbx":
            if not data.startswith(b"Kaydara FBX Binary"):
                self.warn(f"{path}: FBX is not the binary variant; Assimp is required to cook it")
        else:
            if not data.startswith(MESH_MAGIC):
                self.warn(f"{path}: unrecognised mesh container")

    def validate(self) -> bool:
        if not self.project_path.exists():
            self.error(f"project file not found: {self.project_path}")
            return False

        try:
            project = json.loads(self.project_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            self.error(f"{self.project_path}: invalid JSON ({error})")
            return False

        asset_root = self.project_dir / project.get("assetRoot", "assets")
        if not asset_root.is_dir():
            self.error(f"asset root is missing: {asset_root}")
            return False

        assets = project.get("assets", [])
        if not assets:
            self.error("project lists no assets")
            return False

        for index, entry in enumerate(assets):
            relative = entry if isinstance(entry, str) else entry.get("path")
            if not relative:
                self.error(f"asset {index} has no path")
                continue
            relative = relative.replace("\\", "/").lstrip("./")
            path = asset_root / relative
            if not path.is_file():
                self.error(f"{path}: file does not exist")
                continue
            data = path.read_bytes()
            if not data:
                self.error(f"{path}: file is empty")
                continue
            if path.suffix.lower() not in SUPPORTED:
                self.warn(f"{path}: extension '{path.suffix}' is not known, it will be packed as raw data")
                continue
            if path.suffix.lower() in TEXTURE_EXTENSIONS:
                self.check_texture(path, data)
            elif path.suffix.lower() in AUDIO_EXTENSIONS:
                self.check_audio(path, data)
            elif path.suffix.lower() in MESH_EXTENSIONS:
                self.check_mesh(path, data)
            elif path.suffix.lower() in {".json", ".zscene"}:
                try:
                    json.loads(data.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as error:
                    self.error(f"{path}: invalid JSON ({error})")

        scene = project.get("scene")
        if scene:
            scene_path = self.project_dir / scene
            if not scene_path.is_file():
                self.error(f"scene file is missing: {scene_path}")
            else:
                try:
                    document = json.loads(scene_path.read_text(encoding="utf-8"))
                    if "entities" not in document:
                        self.error(f"{scene_path}: scene has no 'entities' array")
                except json.JSONDecodeError as error:
                    self.error(f"{scene_path}: invalid JSON ({error})")

        return not self.errors

    def report(self) -> None:
        for warning in self.warnings:
            print(f"warning: {warning}")
        for error in self.errors:
            print(f"error: {error}", file=sys.stderr)
        print(f"validated {self.project_path}: "
              f"{len(self.errors)} error(s), {len(self.warnings)} warning(s)")


def main(argv: list[str]) -> int:
    if len(argv) < 2 or argv[1] in {"-h", "--help"}:
        print(__doc__)
        return 0 if len(argv) > 1 else 2

    failed = False
    for argument in argv[1:]:
        validator = Validator(Path(argument).resolve())
        if not validator.validate():
            failed = True
        validator.report()
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
