# PitchLab Pro: System Architecture Specification

## 1. High-Level Overview

PitchLab Pro operates on a **"Black Box" Architecture**.

- **The Shell (Java):** Acts solely as an I/O controller. It has zero knowledge of music theory, frequencies, or visuals. It simply passes raw byte streams and screen coordinates to the Native layer.
- **The Engine (C++):** A monolithic, high-performance Singleton that handles all DSP (Digital Signal Processing), Logic, and OpenGL Rendering. It uses heavily optimized integer math and pre-calculated look-up tables (LUTs) to avoid expensive CPU operations during runtime.

---

## 2. The "Singleton" Core (The Engine)

The entire application logic resides in a single C++ Object instance managed via a VTable (Virtual Function Table).

- **Memory Location:** The VTable is located at `0x000582e0`.
- **Initialization:** Created in `FUN_0003063c` -> `FUN_0002f6b8`.
- **The "Brain" Offset Map:**
  - **Offset 0x0C:** **Audio Processing Entry Point** (`FUN_0002a2e0`). Triggers the DSP pipeline.
  - **Offset 0x10:** **Rendering Entry Point** (`FUN_000390fc` -> `FUN_0003162c`). Triggers the OpenGL draw calls.
  - **Offset 0x08:** **Texture Manager**. Stores the texture IDs (Spectrogram, Strobe, etc.).

---

## 3. The Audio Pipeline (DSP Architecture)

This is a "Pull" system decoupled from the rendering frame rate.

### Step A: The Intake Valve (Java -> C++)

- **Function:** `PitchLabNative.processStreamedSamples`
- **Mechanism:** Java captures raw PCM data (16-bit, likely 44.1kHz or 48kHz) and passes it via JNI.
- **Buffer Logic:** The C++ layer uses a **Circular Buffer** (`DAT_00062ccc`).
  - If the new audio chunk fits at the end, it copies it.
  - If it overflows, it splits the chunk: Part A fills the end, Part B wraps to the start.
  - _Result:_ A seamless, infinite stream of audio data without memory allocation overhead.

### Step B: Signal Conditioning (The "Sensitivity" Secret)

Before any FFT happens, the signal is normalized.

- **Function:** `FUN_0003c4b0` (The AGC).
- **Logic:**
  1.  Scan the buffer for the **Maximum Peak Amplitude**.
  2.  Calculate a scaling factor to boost this peak to the maximum integer range (`32767`).
  3.  Multiply _every_ sample by this factor.
- **Why:** This allows the app to detect whispers and loud guitars with equal precision, maximizing the dynamic range of the subsequent Fixed-Point math.

### Step C: Windowing & Transform

- **Windowing:** The raw audio is multiplied by a pre-calculated curve (Lookup Table).
  - **Gaussian:** Generated via `FUN_00019470` (Uses `exp` math). Better for time precision (Spectrogram).
  - **Hanning:** Generated via `FUN_000265dc` (Uses `sin/cos` math). Better for pitch precision (Needle Tuner).
- **Fixed-Point Math:** The window coefficients are not Floats; they are converted to **Q24 Fixed Point Integers** (`0x4b800000` multiplier) to speed up multiplication on ARMv7 CPUs.

### Step D: The "Sub-Bin" Analysis (The 3-Cent Zoom)

This is the core innovation for the Spectrogram.

- **The Map:** The engine does not simply output 0-22kHz. It uses a calculated **"Chroma Map"** (`local_5c`).
- **The Folding:**
  1.  The frequency spectrum is mapped to **12 Notes**.
  2.  Each Note is subdivided into **32 Slices** (High Resolution).
  3.  **Harmonic Summation:** Energy from C2, C3, C4, C5... is summed into the specific "C" bin.
- **Result:** A 384-pixel wide data array representing one "Master Octave" with ~3 cents of precision per pixel.

---

## 4. The Visualization Pipeline (Render Architecture)

The rendering system is a "Passive Consumer." It does not calculate audio; it only visualizes the state of the Texture Memory.

### Step A: The "Film Reel" (Texture Memory)

- **Texture ID 8:** Stores the Spectrogram History.
- **The Update Loop:**
  - Every audio frame, the DSP engine calculates the 384-pixel intensity array.
  - It calls `FUN_00027168` to upload this single row to the GPU using `glTexSubImage2D`.
  - It colors the pixels using the **Color LUT** (`00056d00`).

### Step B: The Viewport Manager

- **Function:** `FUN_00026f58`.
- **Coordinate System:** **Top-Left Origin (0,0)**.
  - Width/Height are raw pixels (not DIPs).
  - UI elements (Lines, Text) are scaled mathematically based on screen width (approx. `Width / 300`).

### Step C: The View Dispatcher

- **Function:** `FUN_0003162c`.
- **Logic:** A massive state machine using `param_2` (View Mode) to decide what to draw.
  - **Mode 2:** Instrument Tuner (Needle).
  - **Mode 3:** Pitch Spectrogram (The "Waterfall").
  - **Mode 4:** Chord Matrix.
  - **Mode 6:** Polyphonic Tuner.
  - **Mode 7:** Stage Tuner (Big Hz Display).

---

## 5. Critical Data Tables (The "Static" DNA)

The system relies on these immutable assets extracted from the binary:

1.  **Color Palette (`00056d00`):** 12 RGB triplets defining the "Heat Map" colors (A=Red, C=Green).
2.  **Decibel Curve (`DAT_000b2d64`):** A pre-calculated `log10` lookup table mapping Integer Amplitude (0-32767) to Visual Brightness.
3.  **Strobe Gradient:** A procedurally generated Sawtooth-Squared texture (generated in `FUN_0002f6b8`) to simulate the spinning wheel.
