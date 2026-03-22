# Architecture

This repository follows the **Shell vs Engine** split described in the master roadmap: [My analysis/New Plan.md](../My%20analysis/New%20Plan.md).

## Shell (`Source/App/`)

The JUCE application is an **I/O host** only at the architectural level:

- Windowing, menus, file dialogs, transport UI (play / pause / seek).
- Audio device and routing: default output, file playback, microphone input.
- Feeding time-domain audio into the engine and presenting surfaces for visualization.

The Shell **must not** own music-theory logic, chromagram folding, FFT details, or fixed-point DSP policy. Keep that code in the engine.

## Engine (`Source/Engine/`)

The **PitchLabEngine** static library holds analysis and (later) rendering-oriented data structures that should be **unit-testable without launching the GUI**.

- Narrow public API: e.g. process blocks of samples, reset state, read results for drawing or for offline image export.
- Fidelity-sensitive code paths (integer ingress, Q24 windowing, AGC, etc.) live here per the roadmap—not in UI handlers.

## Test-driven development

- Prefer **small translation units** under `Source/Engine/` with tests in `Tests/`.
- Do **not** put FFT or block processing in button callbacks; the Shell calls into the Engine from the audio callback or a dedicated offline loop.

## Audio sources (future work)

Use one conceptual **audio source** in the Shell that delivers samples into the same Engine `process` path:

- **File + transport:** `AudioTransportSource` (or equivalent) so the visualizer always reflects what the transport is playing.
- **Microphone:** live input from `AudioDeviceManager`; switching file vs mic is a Shell concern only.

The Engine stays **agnostic** of whether samples came from disk or hardware.

## Realtime vs offline export (future work)

- **Realtime:** device/audio callback → Engine process → UI reads Engine state for visualization.
- **Offline:** decode or pull *n* seconds of audio without wall-clock pressure, run the same Engine process path deterministically, then snapshot a raster (e.g. waterfall chromagram) and encode to an image.

**Image format policy:** Prefer **JPEG** for large golden/reference outputs unless lossless comparison is required; otherwise **PNG** is acceptable. Document the choice in the feature that adds export.

## Forbidden moves (for agents and contributors)

- Do not **merge** `Source/App/` and `Source/Engine/` into a single unstructured folder.
- Do not **replace** fidelity-critical integer / fixed-point paths with “simpler” floating-point equivalents without an explicit, reviewed migration plan tied to the roadmap.
- Do not **edit** the vendored [JUCE/](../JUCE/) tree for product fixes; patch upstream or wrap in project code.
- Do not implement heavy DSP **inline in the UI**; keep the Engine testable.

## Data flow (target state)

```mermaid
flowchart LR
  subgraph shell [Shell_JUCE_UI]
    FilePlayer[File_transport]
    MicInput[Mic_device]
    Router[Audio_source_router]
  end
  subgraph engine [Engine_core]
    Process[Process_audio]
    State[Analysis_state]
  end
  FilePlayer --> Router
  MicInput --> Router
  Router --> Process
  Process --> State
```

## OpenGL / JUCE 8 (when rendering lands)

The roadmap calls for a custom OpenGL path and notes **Direct2D vs OpenGL** interactions on Windows. When that work starts, follow [My analysis/New Plan.md](../My%20analysis/New%20Plan.md) and enable the commented OpenGL linkage in the root `CMakeLists.txt` as appropriate.

## Build and test (verification)

AI agents: use [AI_AGENT.md](AI_AGENT.md) for **build** and **test** commands during work.  
Humans / broken `build/` tree: [BUILD.md](BUILD.md); test framework notes: [TESTING.md](TESTING.md).
