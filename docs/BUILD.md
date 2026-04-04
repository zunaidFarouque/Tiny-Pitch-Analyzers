# Build — setup and troubleshooting (humans / broken tree)

**AI agents doing routine compile or test:** use [AI_AGENT.md](AI_AGENT.md) only — it has the exact **`cmake --build`** and **`ctest`** lines.  
This file is for **initial environment**, **first configure**, **IDE alignment**, and **when `build/` must be recreated**.

---

## Routine compile (reference — duplicated in AI_AGENT)

From repo root, once `build/` is configured:

```bash
cmake --build build --config Debug
```

Release: `--config Release`.

## Run (after a successful build)

- **Unit tests** (from `build/`, multi-config generators need **`-C`** matching your build):

  ```bash
  cd build
  ctest -C Debug --output-on-failure
  ```

- **Desktop app:** run the executable under `build/TinyPitchHost_artefacts/Debug/` or `.../Release/` (see table below). On Windows the name is **`Tiny Pitch Analyzer.exe`**.

---

## Prerequisites (machine setup)

- **CMake** 3.22+ ([cmake.org](https://cmake.org/download/)). After installing, restart the terminal (or IDE) so `cmake` is on `PATH`, or call the full path to `cmake.exe`.
- **C++20:** Windows — **Visual Studio 2022** or **Build Tools 2022** with **Desktop development with C++** (MSVC, Windows SDK), **x64**. Newer installs may use **Visual Studio 2026** / generator `Visual Studio 18 2026` instead — see below. macOS — Xcode; Linux — GCC/Clang + JUCE deps.
- **JUCE** in [JUCE/](../JUCE/) — [licence](https://juce.com/juce-8-licence/).
- **Network** on first configure (GoogleTest ZIP via `FetchContent`).

### Windows: MSVC on `PATH`

CMake must see **MSVC** (`cl.exe`). If configure fails with “no CMAKE_CXX_COMPILER”, open **x64 Native Tools Command Prompt for VS 2022** (or run `vcvars64.bat` from your VS install’s `VC\Auxiliary\Build\`), then run `cmake` from the repo root. VS Code CMake Tools usually locates the kit automatically once a VS install is present.

## First-time or clean configure (Windows, default)

Use **Visual Studio 17 2022** if you have VS 2022 or Build Tools 2022 (common winget install):

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

If you have **Visual Studio 2026** / generator support for **Visual Studio 18 2026**:

```bash
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

**Note:** First run may build **juceaide** (can take a couple of minutes).

### Presets

```bash
cmake --preset windows-msvc
cmake --build build --config Debug
```

[CMakePresets.json](../CMakePresets.json): `windows-msvc` = **Visual Studio 17 2022**, x64, `build/`. For VS 2026 only, configure manually with `-G "Visual Studio 18 2026"` or add a second preset locally.

## Visual Studio Code

[.vscode/settings.json](../.vscode/settings.json): `build/`, **Visual Studio 17 2022**, x64, `compile_commands.json` (with VS 2026 as a fallback preferred generator).  
[.vscode/tasks.json](../.vscode/tasks.json): default task builds **Debug**.

If CMake Tools picks a **single-config** generator, `--config Debug` may not match expectations — delete `build/` and reconfigure with a Visual Studio multi-config generator (or preset `windows-msvc`).

## Multi-config vs single-config

| Generator | Build |
|-----------|--------|
| Visual Studio, Ninja Multi-Config | `cmake --build build --config Debug` |
| Makefiles, single-config Ninja | `cmake --build build` (config fixed at configure) |

This repo’s standard is **multi-config** + **Debug** for dev.

## Output locations (Windows, typical)

| Artifact | Path |
|----------|------|
| App (Debug) | `build/TinyPitchHost_artefacts/Debug/Tiny Pitch Analyzer.exe` |
| App (Release) | `build/TinyPitchHost_artefacts/Release/Tiny Pitch Analyzer.exe` |
| Tests | `build/Tests/Debug/PitchLabEngineTests.exe` (or `.../Release/...` for Release builds) |
| Engine lib | `build/Debug/PitchLabEngine.lib` (or `build/Release/...`) |

## Clean rebuild

```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Use after generator changes, corrupted cache, or stubborn `FetchContent` / `_deps` errors.

## Troubleshooting

- **Generator mismatch:** delete entire `build/`, reconfigure.
- **FetchContent / `_deps`:** delete `build/` or `build/_deps`, network on, reconfigure.
- **`CMAKE_GENERATOR_PLATFORM` MSBuild warning:** unset env var if you want a clean log; often harmless.

## OpenGL

The app already links **`juce_opengl`** and on Windows **`opengl32`** in the root [CMakeLists.txt](../CMakeLists.txt). No extra CMake step is required for the stock GPU visualizer path.

## See also

- [AI_AGENT.md](AI_AGENT.md) — **build/test commands for AI during implementation**
- [TESTING.md](TESTING.md) — test layout and framework notes
- [ARCHITECTURE.md](ARCHITECTURE.md)
- `JUCE/docs/CMake API.md`
