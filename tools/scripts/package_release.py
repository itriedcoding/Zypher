#!/usr/bin/env python3
"""Package a Zypher build into a release archive with checksums.

Produces reproducible zips (fixed timestamps and permissions) so the same
input tree always yields the same archive hash.

Usage:
    python3 tools/scripts/package_release.py \
        --version 0.1.0 --platform macos-universal \
        --input build/stage --output dist
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import zipfile
from pathlib import Path

FIXED_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
EXECUTABLE_SUFFIXES = {".exe", ".dll", ".dylib", ".so", ""}


def collect_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*") if path.is_file() and not path.is_symlink())


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_archive(name: str, root: Path, destination: Path) -> Path:
    archive_path = destination / f"{name}.zip"
    files = collect_files(root)
    if not files:
        raise SystemExit(f"error: '{root}' contains no files")

    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            relative = path.relative_to(root).as_posix()
            info = zipfile.ZipInfo(filename=f"{name}/{relative}", date_time=FIXED_TIMESTAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o755 if path.suffix in EXECUTABLE_SUFFIXES else 0o644
            info.external_attr = (0o100000 | mode) << 16
            archive.writestr(info, path.read_bytes())

    checksum = sha256_of(archive_path)
    (destination / f"{name}.sha256").write_text(f"{checksum}  {archive_path.name}\n", encoding="utf-8")
    print(f"packaged {len(files)} file(s) -> {archive_path}")
    print(f"sha256 {checksum}")
    return archive_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", required=True, help="release version, e.g. 0.1.0")
    parser.add_argument("--platform", required=True, help="target platform, e.g. macos-universal or windows-x64")
    parser.add_argument("--input", required=True, type=Path, help="staged file tree to package")
    parser.add_argument("--output", default=Path("dist"), type=Path, help="directory for the archive")
    arguments = parser.parse_args()

    if not arguments.input.is_dir():
        print(f"error: input directory '{arguments.input}' does not exist", file=sys.stderr)
        return 1
    arguments.output.mkdir(parents=True, exist_ok=True)

    name = f"zypher-{arguments.version}-{arguments.platform}"
    try:
        build_archive(name, arguments.input, arguments.output)
    except SystemExit as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
