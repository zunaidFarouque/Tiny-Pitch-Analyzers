# AI agents: build and test

**Purpose:** While you implement features, use this file to **compile** and **run unit tests**.  
**Not covered here:** Installing toolchains, cloning the repo, or first-time project setup — that is [BUILD.md](BUILD.md) for humans when `build/` does not exist or is broken.

**Assumption:** A configured **`build/`** directory usually already exists (CMake was run before). Your job is to **build** and **ctest**, not to redesign the dev environment.

---

## Working directory

**Repository root:** the folder containing `CMakeLists.txt`, `JUCE/`, `Source/`, `Tests/`.

---

## Build

```bash
cmake --build build --config Debug
```

- **Multi-config rule:** always pass **`--config Debug`** (this repo uses a Visual Studio–style generator).
- **Faster, tests only:** `cmake --build build --config Debug --target PitchLabEngineTests`
- **App only:** `cmake --build build --config Debug --target TinyPitchHost`

**If CMake errors because there is no usable `build/`** (no cache, wrong generator, user deleted `build/`):

```bash
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

If **Visual Studio 18 2026** is not installed, use `-G "Visual Studio 17 2022" -A x64` instead.

On failure: fix **this repo’s** `Source/`, `Tests/`, or top-level `CMakeLists.txt` — **do not** change [JUCE/](../JUCE/) for application behavior.

---

## Test

```bash
cmake --build build --config Debug --target PitchLabEngineTests
ctest --test-dir build --build-config Debug --output-on-failure
```

- **Always** use **`--build-config Debug`** with `ctest` here; omitting it often breaks test discovery on Windows multi-config builds.
- **Direct run (Windows):** `build\Tests\Debug\PitchLabEngineTests.exe`
- Do not ignore failing tests without fixing code or stating a clear **environment** limitation.

---

## Adding a new test file (when your task includes tests)

Add the `.cpp` under `Tests/` and append it to `add_executable(PitchLabEngineTests …)` in [Tests/CMakeLists.txt](../Tests/CMakeLists.txt), then build and `ctest` as above. Framework: GoogleTest, `GTest::gtest_main`, `gtest_discover_tests`.

---

## Other rules (implementation, not build)

- **Architecture:** [ARCHITECTURE.md](ARCHITECTURE.md) — Shell (`Source/App/`) vs Engine (`Source/Engine/`); no DSP in UI handlers.
- **Roadmap / fidelity:** [My analysis/New Plan.md](../My%20analysis/New%20Plan.md)
- **C++20**, minimal diffs.
- **Planning** a feature: map to Shell vs Engine, cite the roadmap, note new tests if relevant.

If JavaScript tooling is added later, the owner prefers **Bun** over npm.
