#!/usr/bin/env python3
"""Run every Zypher verification step that the host can support.

Steps:
  1. configure + build the C++ engine and run the unit tests (ctest)
  2. build and test the Rust asset cooker
  3. cook the demo pack and run the runtime against it
  4. validate the demo assets
  5. typecheck + test the TypeScript editor panels (requires node/npm)
  6. build + test the C# scripting layer (requires the .NET SDK)

Each step is reported individually; the script exits non-zero if any step that
was attempted failed. Missing optional toolchains are reported as skipped, not
as failures.

Usage:
    python3 tools/scripts/run_checks.py [--quick] [--jobs N]
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
DEMO = ROOT / "samples" / "demo"
ARTIFACTS = BUILD / "verification"


class Step:
    def __init__(self, name: str, command: list[str], cwd: Path | None = None, readable: str | None = None) -> None:
        self.name = name
        self.command = command
        self.cwd = cwd or ROOT
        self.readable = readable or " ".join(str(part) for part in command)


def run(step: Step) -> tuple[bool, str]:
    print(f"\n==> {step.name}\n    $ {step.readable}")
    started = time.time()
    try:
        completed = subprocess.run(
            step.command,
            cwd=step.cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    except FileNotFoundError as error:
        return False, f"missing tool: {error.filename}"
    elapsed = time.time() - started
    output = completed.stdout or ""
    tail = "\n".join(output.strip().splitlines()[-25:])
    if tail:
        print(tail)
    if completed.returncode != 0:
        return False, f"exit {completed.returncode} after {elapsed:.1f}s"
    return True, f"ok in {elapsed:.1f}s"


def tool_exists(name: str) -> bool:
    return shutil.which(name) is not None


def runtime_binary() -> Path:
    """Locates the runtime for single- and multi-config generators."""
    name = "zypher_runtime.exe" if os.name == "nt" else "zypher_runtime"
    direct = BUILD / "bin" / name
    if direct.exists():
        return direct
    matches = sorted((BUILD / "bin").glob(f"*/{name}"))
    if matches:
        return matches[-1]
    return direct


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--quick", action="store_true", help="skip the C# and TypeScript steps")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    arguments = parser.parse_args()

    generator = "Ninja" if tool_exists("ninja") else "Unix Makefiles"
    configure = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(BUILD),
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DZYPHER_BUILD_TESTS=ON",
    ]
    if generator == "Ninja":
        configure.extend(["-G", generator])

    steps: list[Step] = [
        Step("configure engine", configure),
        Step("build engine", ["cmake", "--build", str(BUILD), "-j", str(arguments.jobs)]),
        Step("engine tests", ["ctest", "--test-dir", str(BUILD), "--output-on-failure"]),
        Step("cook demo assets", ["cargo", "run", "--quiet", "--manifest-path",
                                  "tools/assetcooker/Cargo.toml", "--", "--verbose"],
             cwd=ROOT),
        Step("asset validation", [sys.executable, "tools/scripts/validate_assets.py",
                                  "samples/demo/zypher.project.json"]),
        Step("runtime with pack", [
            str(runtime_binary()),
            "--scene", str(DEMO / "scene.zscene"),
            "--pack", str(DEMO / "dist" / "demo.zpkg"),
            "--output", str(ARTIFACTS),
            "--width", "640", "--height", "360", "--frames", "2", "--steps", "120", "--dump-frame",
        ]),
    ]

    if not arguments.quick:
        typescript_dir = ROOT / "editor" / "ts"
        if tool_exists("npm") and (typescript_dir / "package.json").exists():
            if not (typescript_dir / "node_modules").exists():
                steps.append(Step("install editor panels", ["npm", "install", "--no-audit", "--no-fund"],
                                  cwd=typescript_dir))
            steps.extend([
                Step("typecheck editor panels", ["npm", "run", "typecheck"], cwd=typescript_dir),
                Step("test editor panels", ["npm", "test"], cwd=typescript_dir),
            ])
        else:
            print("\n==> editor panels: skipped (npm unavailable or project missing)")

        dotnet_tests = ROOT / "tools" / "Zypher.Scripting.Tests"
        if tool_exists("dotnet") and (dotnet_tests / "Zypher.Scripting.Tests.csproj").exists():
            steps.extend([
                Step("build C# scripting", ["dotnet", "build", "-v", "quiet", "--nologo"], cwd=dotnet_tests),
                Step("test C# scripting", ["dotnet", "run", "--no-build"], cwd=dotnet_tests),
            ])
        else:
            print("\n==> C# scripting: skipped (dotnet unavailable)")

    results: list[tuple[str, bool, str]] = []
    for step in steps:
        ok, message = run(step)
        results.append((step.name, ok, message))
        if not ok and step.name in {"configure engine", "build engine", "engine tests"}:
            break

    print("\n=== verification summary ===")
    for name, ok, message in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name:<28} {message}")
    failures = sum(1 for _, ok, _ in results if not ok)
    print(f"\n{len(results) - failures}/{len(results)} step(s) passed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
