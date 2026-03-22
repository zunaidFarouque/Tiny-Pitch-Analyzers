# Testing — framework notes

**Running tests during implementation:** commands are in [AI_AGENT.md](AI_AGENT.md) (`cmake --build … --target PitchLabEngineTests`, `ctest --build-config Debug …`). Read that first.

This page explains **what** the test setup is, for when you add or change tests.

---

## Framework

- **GoogleTest** — pulled by root [CMakeLists.txt](../CMakeLists.txt) (`FetchContent`, ZIP).
- **Target:** `PitchLabEngineTests` — links `GTest::gtest_main`.
- **CTest:** `gtest_discover_tests` registers each `TEST(Suite, Case)` (e.g. `PitchLabEngine.VersionStringIsNonEmpty`).

## Layout

- Sources: `Tests/*.cpp` — unit tests for **PitchLabEngine** / `Source/Engine/` (no GUI required).
- **New file:** add `.cpp` under `Tests/` and add it to `add_executable(PitchLabEngineTests …)` in [Tests/CMakeLists.txt](../Tests/CMakeLists.txt), then use [AI_AGENT.md](AI_AGENT.md) to build and run tests.

## Why `--build-config Debug` on ctest

The default Windows generator is **multi-config**. Without `--build-config Debug`, CTest may look in the wrong output folder and show **no tests** or missing executables.

## Optional

```bash
ctest --test-dir build --build-config Debug --show-only
```

## TDD workflow (implementation hint)

Red → green → refactor using `TEST(...)` in `Tests/`, logic in `Source/Engine/`. Prefer public engine APIs.

## Scope

| Area | Where |
|------|--------|
| DSP / engine logic | Unit tests in `Tests/` |
| File / mic / UI | Integration or manual (later) |
| Golden images / WAVs | [test-assets/](../test-assets/) |

## Naming

- `TEST(SuiteName, CaseName)` → CTest name `SuiteName.CaseName`.

## References

- [AI_AGENT.md](AI_AGENT.md) — **operational test commands**
- [BUILD.md](BUILD.md) — configure / broken `build/`
- [My analysis/New Plan.md](../My%20analysis/New%20Plan.md)
