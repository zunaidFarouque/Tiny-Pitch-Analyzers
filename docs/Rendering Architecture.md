# Rendering Architecture (Web + Android, GPU-first)

This document defines a practical rendering architecture for this project with:

- **GPU-first** runtime behavior (primary path on Web and Android)
- **CPU fallback** when GPU context/shader setup fails
- **Consistent visuals** across backends via shared render logic
- **TDD-friendly contracts** and acceptance criteria for AI-driven development

The target is **perceptual parity**, not strict bit-perfect parity.

---

## 1) Product goals and constraints

### Priority platforms

- **Web** (modern browser engines, wide device range)
- **Android** (low-end to high-end phones/tablets)

### Runtime policy

- Assume most current devices have usable GPU.
- Prefer GPU path by default.
- Keep CPU fallback available but lower priority.
- When CPU fallback is active, draw visible status text: **`CPU Mode`**.

### Non-goals

- Do not enforce byte-identical GPU vs CPU frames in real time.
- Do not duplicate full visualization logic separately per backend.

---

## 2) Current project baseline (important)

Current state in this repository:

- Realtime app UI uses `OpenGLVisualizerHost` (`Source/App/OpenGLVisualizerHost.*`).
- `MainComponent` wires analysis outputs directly to OpenGL host (`Source/App/MainComponent.*`).
- CPU renderer exists as `VizCpuRenderer` but is currently used by offline export tooling (`Source/Tools/VizExport/*`), not realtime app fallback.

Therefore, the right next step is to **introduce a shared render model + backend abstraction**, then plug both GPU and CPU hosts into it.

---

## 3) Core architecture

### 3.1 Layering

Use three render layers:

1. **Render model layer (shared logic)**
   - Converts engine state and mode into backend-agnostic draw commands.
   - Owns coordinate transforms, color mapping, mode-specific composition rules.
2. **GPU backend**
   - Executes commands on OpenGL/WebGL (or platform GPU API abstraction).
3. **CPU backend**
   - Executes same commands into software image buffer.

This is the key to keeping behavior aligned while allowing backend-specific raster details.

### 3.2 Backend interface

Define one host-facing interface used by app shell:

- `setWaveformFeed(...)`
- `setEngineStatePtr(...)`
- `setStaticTablesPtr(...)`
- `setMode(...)`
- `pushWaterfallRow(...)`
- `setBounds(...)` / `paint(...)` / `tick(...)` (depending on UI framework integration)

The shell (`MainComponent`) should talk only to this interface, not directly to OpenGL classes.

### 3.3 Shared frame data contract

All backends consume the same frame inputs:

- Waveform snapshot (2048 samples)
- Chroma row (384 bins)
- `tuningError`, `strobePhase`
- Chord matrix probabilities
- Waterfall ring write index
- Optional quality tier settings

Never allow backend-specific data preprocessing in app shell.

---

## 4) Waterfall and temporal state (most critical parity point)

For stable parity, centralize the waterfall temporal model:

- Keep a shared **film-reel ring buffer model** (logical texture/history store).
- On each analysis update:
  - write incoming 384-bin row at `writeY`
  - increment modulo `historyHeight`
- Sample with the same wrap semantics in both backends.

Do not keep separate “history rules” in GPU and CPU code, or visuals will drift.

---

## 5) GPU-first with CPU fallback behavior

### 5.1 Startup

1. Attempt GPU backend initialization.
2. Validate context + program creation (shader compile/link, required texture formats).
3. If successful, run GPU backend.
4. If failed, switch to CPU backend and show **`CPU Mode`** overlay text.

### 5.2 Runtime failure handling

If GPU backend later reports fatal rendering/resource failure:

- Tear down GPU backend safely.
- Swap to CPU backend without stopping analysis pipeline.
- Keep last known mode and render state.
- Show **`CPU Mode`** indicator.

### 5.3 Optional developer/user controls

- `Force GPU` (debug)
- `Force CPU` (debug and test)
- `Auto` (default policy)

These controls are useful for deterministic test coverage.

---

## 6) Visual parity strategy (practical, testable)

### 6.1 Definition of parity

Parity target is:

- same scene composition
- same transforms and timing rules
- same color mapping formulae
- acceptable rasterization differences under threshold

### 6.2 Why not bit-perfect

GPU and CPU rasterization differ due to:

- interpolation precision
- filtering differences
- antialiasing and line raster rules
- browser/driver/device behavior

Do not burn engineering time chasing unavoidable subpixel differences.

### 6.3 Acceptance metrics

For each visualization mode and fixed inputs:

- Mean Absolute Difference (MAD) per channel <= configured threshold
- 99th percentile pixel error <= configured threshold
- Optional SSIM lower bound for complex modes

Thresholds should be mode-specific and documented in tests.

---

## 7) Performance policy by backend

### 7.1 GPU backend defaults

- Keep GPU as primary on Web/Android.
- Prefer stable frame pacing over maximum effects.
- Upload only changed analysis rows (dirty-row updates).
- Reuse buffers/textures; avoid per-frame resource creation.

### 7.2 CPU fallback defaults

CPU mode should stay usable on weak devices:

- lower internal resolution option
- lower redraw cadence (e.g. 30 Hz)
- reduced effect complexity on heavy modes (especially strobe/waterfall)
- dirty-region updates where practical
- precomputed/static geometry cache

Always keep UI responsive and mode-switch reliable even in CPU mode.

---

## 8) TDD plan for rendering architecture

Adopt these test classes in order:

1. **Render model unit tests**
   - Given fixed frame data, verify generated draw command list and ordering.
2. **Waterfall ring model tests**
   - Write/scroll/wrap correctness over long sequences.
3. **Backend conformance tests**
   - Render same synthetic scenes via GPU and CPU backends; compare metrics.
4. **Fallback behavior tests**
   - Simulate GPU init failure -> CPU mode active and `CPU Mode` text visible.
5. **Performance guard tests**
   - CPU fallback frame generation stays under budget for low-tier settings.

### Deterministic fixtures

Use deterministic analysis fixtures:

- fixed waveform arrays
- fixed chroma rows
- fixed strobe phase/cents/chord matrices

Avoid live microphone/file nondeterminism in unit-level rendering tests.

---

## 9) AI-driven implementation protocol

When implementing with AI agents, enforce this workflow:

1. **Contract first**
   - update docs/spec for one mode (inputs -> commands -> expected image traits)
2. **Write failing tests**
   - render model + parity metric tests for that mode
3. **Implement minimal code**
   - shared logic first, then backend adapters
4. **Pass tests**
   - unit + conformance + fallback behavior
5. **Refactor**
   - no behavior changes without keeping tests green

### PR checklist (mandatory)

- Shared logic added/updated before backend-specific behavior.
- No duplicated mode math between GPU and CPU backends.
- `CPU Mode` indicator shown only when CPU backend active.
- New/changed mode has parity test thresholds documented.
- No per-frame allocation hotspots introduced in hot render path.

---

## 10) Recommended mode-by-mode rollout

Implement incrementally:

1. Waveform
2. Waterfall
3. Needle
4. Chord matrix
5. Strobe radial

Waterfall should be treated as the reference architecture for temporal/ring behavior and parity testing.

---

## 11) Operational definitions (for this project)

- **GPU-first:** GPU backend is selected automatically when available and healthy.
- **CPU fallback:** activated only when GPU path cannot initialize or fails at runtime.
- **Perceptual parity:** visual output differences are below agreed thresholds and not user-noticeable in normal use.
- **Usable CPU mode:** maintains interaction and understandable visuals on low-end devices, even if effects are simplified.

---

## 12) Summary decision

For Web + Android priorities in this project:

- Keep **GPU as the primary realtime renderer**.
- Keep **CPU fallback** for resilience, with explicit **`CPU Mode`** indicator.
- Enforce **shared render model + backend conformance tests** to keep outputs aligned.
- Optimize CPU fallback for usability and determinism, not perfect pixel identity.

