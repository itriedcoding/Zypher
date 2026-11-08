# Contributing to Zypher

Thanks for helping. Zypher targets macOS and Windows; Linux is used for headless
CI verification of the portable core.

## Before you start

- Read [docs/architecture.md](docs/architecture.md) and
  [docs/roadmap.md](docs/roadmap.md).
- For anything larger than a bug fix, open an issue describing the change first.
- Do not add a required third-party dependency to the core; put integrations
  behind a `ZYPHER_WITH_*` option and keep the dependency-free path working.

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DZYPHER_BUILD_TESTS=ON -DZYPHER_BUILD_EDITOR=ON
cmake --build build
python3 tools/scripts/run_checks.py
```

`run_checks.py` runs the engine tests, Rust cooker, asset validation, runtime
render, editor TypeScript tests and C# scripting tests that the host supports.
Missing toolchains are reported as skipped, not as passes.

## Style

- C++23, 4-space indent, `zy::` namespace, fixed-width integer aliases.
- Public headers are self-contained and documented; prefer `Result<T>` over
  exceptions in engine code.
- Keep data layouts stable and document any format change in `docs/formats.md`.
- C# follows the existing `Zypher.Scripting` API; no `UnityEngine` or other
  engine dependencies.
- Rust: no external crates in the cooker (it must build offline in CI).
- TypeScript: strict mode, ESM with `.js` import extensions.
- Python: standard library only for scripts under `tools/scripts`.

## Tests

- Engine tests use `tests/TestFramework.h` (`ZY_TEST`, `EXPECT_*`). Use extra
  parentheses for template commas: `EXPECT_EQ((registry.count<Transform>()), 4U)`.
- Editor core tests live in `editor/tests` and run as `zypher_editor_tests`.
- Rust cooker tests use `cargo test`.
- C# behaviour tests use the fake native context in `tools/Zypher.Scripting.Tests`.
- TypeScript panel tests use `node:test`.
- Every fix should come with the test that would have caught it.

## Commits and pull requests

- One logical change per commit, with a message explaining *why*.
- Describe verification in the PR: which commands ran, what passed, what could
  not run (and why).
- Update `docs/` when behaviour, formats or options change.
- Do not commit build output, `artifacts/`, `dist/`, `node_modules/` or local
  toolchains; `.gitignore` covers them.

## Licence

By contributing you agree that your contributions are licensed under the MIT
license in [LICENSE](LICENSE).
