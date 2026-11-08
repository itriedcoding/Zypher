#!/usr/bin/env python3
"""Print the PE subsystem of a Windows executable.

2 means Windows GUI (no console window when double-clicked), 3 means console.
Release verification uses this to prove that zypher_editor.exe opens a window
instead of a terminal.

Usage:
    python3 tools/scripts/pe_subsystem.py path/to/app.exe
"""

from __future__ import annotations

import struct
import sys

GUI_SUBSYSTEM = 2


def pe_subsystem(path: str) -> int:
    with open(path, "rb") as stream:
        data = stream.read()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise SystemExit(f"{path}: not a PE file (missing MZ header)")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise SystemExit(f"{path}: not a PE file (missing PE signature)")
    magic = struct.unpack_from("<H", data, pe_offset + 0x18)[0]
    if magic not in (0x20B, 0x10B):
        raise SystemExit(f"{path}: unknown optional header magic 0x{magic:x}")
    # Subsystem sits at the same offset in both PE32 and PE32+ optional headers,
    # because PE32+ drops BaseOfData and widens ImageBase by the same amount.
    return struct.unpack_from("<H", data, pe_offset + 0x18 + 0x44)[0]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: pe_subsystem.py <executable>", file=sys.stderr)
        return 2
    print(pe_subsystem(sys.argv[1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
