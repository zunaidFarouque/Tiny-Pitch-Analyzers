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

---

## Prerequisites (machine setup)

- **CMake** 3.22+ ([cmake.org](https://cmake.org/download/)).
- **C++20:** Windows — VS Build Tools 18 / VS 2026 or VS 2022, **Desktop C++**, **x64**; macOS — Xcode; Linux — GCC/Clang + JUCE deps.
- **JUCE** in [JUCE/](../JUCE/) — [licence](https://juce.com/juce-8-licence/).
- **Network** on first configure (GoogleTest ZIP via `FetchContent`).

## First-time or clean configure (Windows, default)

```bash
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Only **VS 2022** available: `-G "Visual Studio 17 2022" -A x64` and set [.vscode/settings.json](../.vscode/settings.json) `cmake.generator` to match.

**Note:** First run may build **juceaide** (can take a couple of minutes).

### Presets

```bash
cmake --preset windows-msvc
cmake --build build --config Debug
```

[CMakePresets.json](../CMakePresets.json): `windows-msvc` = VS 18 2026, x64, `build/`.

## Visual Studio Code

[.vscode/settings.json](../.vscode/settings.json): `build/`, VS generator, x64, `compile_commands.json`.  
[.vscode/tasks.json](../.vscode/tasks.json): default task builds **Debug**.

If CMake Tools picks a **single-config** generator, `--config Debug` may not match expectations — delete `build/` and reconfigure with Visual Studio (or preset `windows-msvc`).

## Multi-config vs single-config

| Generator | Build |
|-----------|--------|
| Visual Studio, Ninja Multi-Config | `cmake --build build --config Debug` |
| Makefiles, single-config Ninja | `cmake --build build` (config fixed at configure) |

This repo’s standard is **multi-config** + **Debug** for dev.

## Output locations (Windows, typical)

| Artifact | Path |
|----------|------|
| App | `build/TinyPitchHost_artefacts/Debug/Tiny Pitch Analyzer.exe` |
| Tests | `build/Tests/Debug/PitchLabEngineTests.exe` |
| Engine lib | `build/Debug/PitchLabEngine.lib` |

## Clean rebuild

```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -B build -S . -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

Use after generator changes, corrupted cache, or stubborn `FetchContent` / `_deps` errors.

## Troubleshooting

- **Generator mismatch:** delete entire `build/`, reconfigure.
- **FetchContent / `_deps`:** delete `build/` or `build/_deps`, network on, reconfigure.
- **`CMAKE_GENERATOR_PLATFORM` MSBuild warning:** unset env var if you want a clean log; often harmless.

## OpenGL (future)

Uncomment the OpenGL block in root [CMakeLists.txt](../CMakeLists.txt) when implementing per [New Plan](../My%20analysis/New%20Plan.md).

## See also

- [AI_AGENT.md](AI_AGENT.md) — **build/test commands for AI during implementation**
- [TESTING.md](TESTING.md) — test layout and framework notes
- [ARCHITECTURE.md](ARCHITECTURE.md)
- `JUCE/docs/CMake API.md`
