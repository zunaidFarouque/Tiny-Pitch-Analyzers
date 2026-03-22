This is **Part 2 of the Report: The Audio Pipeline**.

To address your skepticism: You are right to question if "standard" FFT logic can produce such high-quality results. The secret we discovered during the deep dive is not _more_ logic, but **smarter** logic.

PitchLab does not use complex, heavy algorithms like Autocorrelation (YIN) or Cepstral analysis usually found in offline tools. Instead, it uses **Hyper-Optimized Signal Conditioning**.

1.  **The AGC (Auto-Gain):** It artificially boosts the signal to maximum integer resolution _before_ processing.
2.  **The Sub-Bin Folding:** It doesn't just "bin" the frequencies; it maps them into a high-resolution (32-slice) chromatic grid.

The complexity isn't in the _steps_, it is in the **Data Tables**.

---

# Section 3: The Audio Pipeline (DSP Architecture)

## 3.0 Overview

The Audio Pipeline is a high-priority, synchronous loop driven by the incoming audio stream from Android. It operates on a **"Pull"** architecture, decoupled from the rendering frame rate. The engine minimizes runtime math by relying heavily on pre-calculated Look-Up Tables (LUTs) generated during the initialization phase.

**The Pipeline Flow:**
`Raw PCM (Java)` $\rightarrow$ `Circular Buffer` $\rightarrow$ `AGC Normalization` $\rightarrow$ `Windowing` $\rightarrow$ `FFT` $\rightarrow$ `Chromatic Mapping (Folding)` $\rightarrow$ `Texture Memory`.

---

## 3.1 Data Intake: The Circular Buffer Strategy

**Function:** `Java_com_symbolic_pitchlab_PitchLabNative_processStreamedSamples`
**Memory Address:** `DAT_00062ccc` (Buffer Start), `DAT_00062cd8` (Write Head)

The Java layer pushes raw 16-bit PCM data (likely 44.1kHz or 48kHz mono) via JNI. The C++ engine does not process this array immediately. Instead, it acts as a "Time-Shifter."

- **The Mechanism:** A fixed-size Ring Buffer is maintained in the heap.
- **The Wrap-Around Logic:** The code explicitly checks if the incoming chunk size exceeds the remaining space at the end of the buffer.
  - _Case A (Fits):_ Simple `memcpy`.
  - _Case B (Overflow):_ A Split `memcpy`. The first part fills the end of the buffer; the remainder wraps around to index 0.
- **Significance:** This guarantees that the FFT always has access to a contiguous block of history (e.g., 4096 samples) without requiring expensive memory reallocation or array shifting every frame.

---

## 3.2 Signal Conditioning: The "Analysis Sensitivity" Engine

**Function:** `FUN_0003c4b0`
**Strategic Importance:** This is the primary reason PitchLab feels so "sensitive" compared to other tuners. It implements a destructive **Auto-Gain Control (AGC)** / Normalizer.

Before any frequency analysis occurs, the engine maximizes the **Dynamic Range**:

1.  **Peak Detection:** The function iterates through the current audio window using optimized bitwise absolute value math (`(val ^ (val >> 31)) - (val >> 31)`) to find the single loudest sample ($A_{max}$).
2.  **Scale Factor Calculation:** It calculates a multiplier scalar $S$:
    $$ S = \frac{32767}{A\_{max}} $$
3.  **Destructive Boost:** It iterates through the window again, multiplying every sample by $S$.

**The Result:** Even a faint whisper uses the full 16-bit integer range (`-32768` to `+32767`). This eliminates "quantization noise" in the FFT and allows the visualizer to show clear structures for extremely quiet signals.

---

## 3.3 The Transform Engine: Windowing & FFT

**Function:** `FUN_0002a2e0` (Lines 108–330)
**Key Data:** `local_12fc` (Window Table Pointer)

Once the signal is normalized, it must be prepared for the Frequency Domain. PitchLab supports two distinct mathematical modes, switched by the User Settings.

### A. The Windowing Function

The raw audio is element-wise multiplied by a pre-calculated float array.

1.  **Hanning Mode (Default/Pitch Focus):**
    - **Generator:** `FUN_000265dc`.
    - **Math:** Uses `sinf` and `cosf` to create a standard Bell curve.
    - **Effect:** Reduces spectral leakage, making the "Needle" tuner more stable.
2.  **Gaussian Mode (Time Focus):**
    - **Generator:** `FUN_00019470`.
    - **Math:** Uses `exp` ($e^{-x^2}$) to create a narrower bell curve.
    - **Effect:** Reduces time-smearing, making the "Waterfall" spectrogram clearer for fast passages.

### B. The Fixed-Point Optimization

The window coefficients are not stored as floats. They are converted to **Q24 Fixed-Point Integers** via `FUN_00025550` (Multiplier `0x4b800000`). This allows the multiplication loop to run using fast integer arithmetic on the ARM CPU.

### C. The FFT (Fast Fourier Transform)

The code calls an internal function (via pointer) to perform the transform. Based on the loop structures in `FUN_0002a2e0` (unrolled loops of 8), this is a highly optimized, possibly "Real-Valued" FFT algorithm (likely a variant of **KissFFT** or a custom NDK implementation) typically size 4096 or 8192.

---

## 3.4 The Harmonic Analyzer: Sub-Bin Chromatic Folding

**Function:** `FUN_0002a2e0` (Lines 333–End)
**Key Data:** `local_5c` (The Chroma Map)

This is the algorithm that powers the unique **Octave-Folded Spectrogram**. It transforms the linear FFT output (0Hz – 22kHz) into the musical "Note vs. Cents" domain.

**The "32-Slice" Architecture:**
The visualizer does not just track 12 notes. It tracks **384 discrete pitch buckets**.
$$ 12 \text{ Notes} \times 32 \text{ Slices per Note} = 384 \text{ Bins} $$

- **Resolution:** Each "Slice" represents approx. **3.125 Cents**. This is why the visualizer can show tiny pitch drifts.

**The Folding Algorithm:**

1.  **Preparation:** The Chroma Map (`local_5c`) contains pre-calculated start indices for each note based on the sample rate.
2.  **Harmonic Summation:**
    - The engine iterates through the 384 target buckets.
    - For each bucket (e.g., "Note A, Slice 16"), it calculates the corresponding FFT bin indices for **all available octaves** (A1, A2, A3, A4...).
    - It sums the magnitude energy from all these octaves into the single target bucket.
3.  **Peak Tracking:** While summing, it tracks which octave contributed the most energy. This integer is stored separately to allow the UI to label the note (e.g., "A2" vs "A4") even though the visual is folded.

**Output:**
The result is a 384-pixel wide array of intensity values, which is immediately handed off to the Texture Uploader (`FUN_00027168`).

---

===

---

This is the detailed expansion of **Section 3.1**. This specific mechanism is the foundation of the app's stability. Without this robust buffering strategy, the visualization would stutter or drift whenever the Android OS audio scheduler hiccuped (which was common on Android 4.x/5.x).

---

# 3.1 Data Intake: The Circular Buffer Strategy

## 3.1.1 Overview

The entry point for all acoustic data is the JNI function `processStreamedSamples`. This function acts as an **Impedance Matcher** between the Android Audio Hardware (which pushes small, irregular chunks of data) and the PitchLab DSP Engine (which requires large, overlapping, contiguous blocks of history).

Instead of allocating new memory for every incoming audio packet (which would cause Garbage Collection stutter), the engine maintains a persistent **Ring Buffer** (Circular Buffer) in the Native Heap.

## 3.1.2 Memory Architecture

Based on the disassembly of `FUN_0002a2e0` and the JNI function, the buffer state is managed by four global variables in the Engine's memory struct:

| Variable (Offset) | Type     | Name                 | Description                                                                |
| :---------------- | :------- | :------------------- | :------------------------------------------------------------------------- |
| `DAT_00062ccc`    | `short*` | **`pMainBuffer`**    | Pointer to the allocated memory block (The "Tape Loop").                   |
| `DAT_00062cd0`    | `int`    | **`bufferCapacity`** | Total size of the buffer in samples (e.g., 16384).                         |
| `DAT_00062cd8`    | `int`    | **`writeHead`**      | The current index (0 to Capacity) where the _next_ sample will be written. |
| `DAT_00062cd4`    | `int`    | **`availableCount`** | Tracks how many new samples have accumulated since the last FFT run.       |

## 3.1.3 The Intake Logic Flow

Every time the Java `AudioRecord` thread receives data, it calls this C++ function. The execution flow is strictly optimized for speed:

1.  **Global Environment Update:**

    - The function immediately updates the global variable `DAT_00058ab8` with the `sampleRate` passed from Java.
    - _Strategic Note:_ This allows the engine to adapt instantly if the OS switches the sample rate (e.g., connecting a Bluetooth headset typically forces a switch from 48kHz to 8kHz or 16kHz). The DSP is not hardcoded to 44100Hz.

2.  **JNI Heap Locking:**

    - It calls `GetShortArrayElements` (Offset `0x2e8` on `JNIEnv`).
    - This "pins" the Java array in memory, giving C++ direct access to the raw PCM data without performing an expensive copy operation at this stage.

3.  **The Wrap-Around Calculation:**
    The engine determines if the incoming chunk (`size`) will fit linearly at the end of the buffer or if it needs to wrap around to the beginning.

    - **Condition:** `if (writeHead + size > bufferCapacity)`

    **Scenario A: The Split Write (Overflow)**
    If the chunk hits the end of the buffer:

    1.  **Calculate Remainder:** `rem = bufferCapacity - writeHead`
    2.  **Copy Part 1:** `memcpy` the first `rem` samples to the _end_ of `pMainBuffer`.
    3.  **Copy Part 2:** `memcpy` the remaining `size - rem` samples to the _start_ (`index 0`) of `pMainBuffer`.

    **Scenario B: The Linear Write (No Overflow)**
    If the chunk fits:

    1.  **Copy:** `memcpy` the entire `size` chunk to `pMainBuffer + writeHead`.

    _Technical Detail:_ All `memcpy` operations use `size * 2` because the data is **16-bit Signed Integers** (2 bytes per sample).

4.  **Pointer Update:**

    - The `writeHead` is advanced by `size`.
    - Crucially, logic handles the index wrap: `if (writeHead >= bufferCapacity) writeHead -= bufferCapacity;`.
    - The `availableCount` is incremented, signaling the DSP engine that fresh data is ready for processing.

5.  **JNI Release:**
    - Finally, `ReleaseShortArrayElements` is called to unpin the Java array, allowing the Garbage Collector to clean it up.

## 3.1.4 Strategic Significance

This implementation solves two critical problems in audio programming:

1.  **The "Jitter" Problem:** Android audio callbacks are not perfectly timed. One callback might come after 10ms, the next after 30ms. By dumping everything into a ring buffer, the DSP engine (which runs on the rendering thread) can just "look back" into the buffer for the last $N$ milliseconds of audio, ignoring the irregularity of the input arrival times.
2.  **The "Contiguity" Problem:** FFT algorithms require contiguous arrays (e.g., `Sample[0]` to `Sample[4095]`). In a circular buffer, data might wrap around the edge.
    - _Implementation Detail:_ Later in the DSP pipeline (`FUN_0002a2e0`), the code likely performs a "Linearize" step (reading from the Ring Buffer into a temporary linear array) before the FFT to ensure the math functions don't have to handle the wrap-around logic themselves.

## 3.1.5 Re-Implementation Requirements

To replicate this behavior in a modern environment:

- **Buffer Size:** Must be large enough to hold at least 2x the largest FFT size (e.g., if FFT is 8192, Buffer should be 16384 or 32768) to allow for history overlap.
- **Thread Safety:** Since `processStreamedSamples` (Writer) and `ProcessAudio` (Reader) run on different threads, the `writeHead` variable must be atomic or protected by a mutex to prevent race conditions (though PitchLab relies on the simplistic integer write atomicity of ARM CPUs).

---

===

---

This is the detailed technical specification for **Section 3.2**.

This section analyzes `FUN_0003c4b0`, which serves as the "Pre-Amp" for the entire mathematical engine. Its existence explains why PitchLab is able to visualize extremely quiet sounds (like a finger rubbing against a guitar body) with the same clarity as a loud strum.

---

# 3.2 Signal Conditioning: The Auto-Gain Normalizer

## 3.2.1 Overview

**Function:** `FUN_0003c4b0`
**Input:** `short* sourceBuffer`, `int sampleCount`
**Output:** Normalized Audio Buffer (In-Place or Copy)

Before the audio signal touches the FFT (Fast Fourier Transform), it undergoes a rigorous **Signal Conditioning** process. The engine implements a destructive **Auto-Gain Control (AGC)** / Normalization filter.

Unlike standard tuners which might simply threshold low-volume signals (Noise Gate), PitchLab takes the opposite approach: it amplifies _everything_ to the maximum possible integer resolution.

## 3.2.2 Phase 1: High-Speed Peak Detection

The first pass over the audio buffer determines the "Headroom" available in the signal.

- **The Algorithm:** It iterates through the entire sample window to find the sample with the maximum absolute amplitude ($A_{max}$).
- **Optimization (Bitwise ABS):**
  Instead of using a conditional branching `abs()` function (which slows down pipelined CPUs), it uses the standard 2's complement bitwise method:

  ```c
  int mask = sample >> 31;       // All 1s if negative, 0 if positive
  int abs  = (sample ^ mask) - mask;
  ```

  This executes in constant CPU cycles, regardless of the signal's sign.

- **Optimization (Loop Unrolling):**
  The disassembly reveals aggressive manual unrolling. The code processes **16 samples per loop iteration** (Lines 30-100).
  - _Nuance:_ This reduces the loop overhead (incrementing `i` and checking `i < size`) by a factor of 16. This was a critical optimization for the ARMv7 architectures prevalent in 2012.

## 3.2.3 Phase 2: The Gain Scalar Calculation

Once $A_{max}$ is found, the engine calculates how much it can amplify the signal before clipping.

- **Target:** The maximum positive value for a 16-bit signed integer (`SHRT_MAX` = `32767` or `0x7FFF`).
- **The Formula:**
  It calculates a fixed-point multiplier `Gain`.
  ```c
  // Line 135: __aeabi_ldivmod(0xffffffff, 0x7fff, iVar11, ...)
  uint64_t numerator = 0x7FFFFFFF7FFFFFFF; // Effectively "Huge Number"
  int peak = iVar11;
  uint64_t gainFactor = numerator / peak;
  ```
- **Silence Handling:**
  If $A_{max} == 0$ (Digital Silence), the calculation is skipped to avoid a Divide-By-Zero error. The buffer is simply `memset` to 0.

## 3.2.4 Phase 3: Destructive Amplification

The second pass applies the calculated `Gain` to every sample.

- **The Math:**
  It performs a 64-bit multiplication and shift to scale the integers.
  $$ \text{Sample}_{new} = (\text{Sample}_{old} \times \text{Gain}) \gg 32 $$
- **Result:**
  The loudest peak in the signal is now exactly `32767`.
  - If the input was a loud guitar (`Peak = 20000`), it applies a small boost ($\times 1.6$).
  - If the input was a whisper (`Peak = 100`), it applies a massive boost ($\times 327$).

## 3.2.5 Strategic Significance

This architecture has two profound effects on the user experience:

1.  **"Infinite" Sensitivity:** By maximizing the integer range usage, the subsequent FFT (which likely uses integer math) operates with optimal bit-depth. Quantization noise is minimized for quiet signals.
2.  **Visual Consistency:** The "Waterfall" visualizer always looks full and rich, regardless of input volume. The user does not need to manually adjust a "Microphone Gain" slider; the math ensures the spectrum is always fully saturated.

**Re-Implementation Note:**
When rebuilding this, standard floating-point normalization (`sample / peak`) is acceptable on modern CPUs. However, to strictly replicate the _exact_ noise floor behavior of PitchLab, one should stick to the Integer Normalization method, as floating-point precision differences might alter the appearance of background noise in the spectrogram.

---

===

---

This is **Part 4 of the Report: The Harmonic Analyzer**.

This section details the most sophisticated algorithm in PitchLab Pro. This is the logic that allows the Spectrogram to display a "Folded" view (where all C notes, regardless of octave, align vertically) while retaining high-precision pitch tracking (showing deviations of ~3 cents).

---

# 3.4 The Harmonic Analyzer: Sub-Bin Chromatic Folding

## 3.4.1 Overview

**Function:** `FUN_0002a2e0` (Lines 333 – End)
**Key Data Structures:** `local_5c` (Chroma Map), `local_12e4` (Energy Buffer), `local_12e8` (Octave Tracker).

Standard FFT output is linear ($0Hz \rightarrow 22kHz$). Musical pitch is logarithmic. Most tuners use a simple "Peak Picking" algorithm to find the fundamental frequency. PitchLab does something different: it creates a **Harmonic Fingerprint**.

It transforms the linear FFT data into a **Chromatically Aligned Buffer** of size **384**.

## 3.4.2 The "32-Slice" Resolution Architecture

The engine does not simply bin energy into "12 Notes." It subdivides every semitone into high-resolution slices.

- **The Math:**
  $$ 12 \text{ Semitones} \times 32 \text{ Slices/Semitone} = 384 \text{ Total Bins} $$
- **The Precision:**
  Since a semitone is 100 Cents:
  $$ \text{Resolution} = \frac{100 \text{ Cents}}{32 \text{ Slices}} \approx \mathbf{3.125 \text{ Cents per Pixel}} $$
- **Significance:** This explains why the visualizer appears to "drift" smoothly when a string is slightly sharp or flat. The energy peak physically moves to adjacent slices within the 32-slice block.

## 3.4.3 Step A: The Chroma Map Generation

Before the loop runs, the engine calculates a **Lookup Table (LUT)** to map FFT bins to the 32-slice grid.

- **Variable:** `local_5c` (Array of 11 Integers).
- **Calculation:** Uses the constant `0x3e23d70a` ($\approx 0.16$).
- **Logic:**
  It iterates through the 12 notes (C to B). For each note, it calculates the **FFT Bin Index** corresponding to that pitch in the _lowest_ tracked octave.
  - This acts as the "Base Offset" for the summation loop.

## 3.4.4 Step B: The "Folding" Algorithm (Harmonic Summation)

This is the core loop of the function. It iterates 384 times (12 notes $\times$ 32 slices).

**The Algorithm:**

1.  **Target Selection:** Pick the current slice (e.g., "Note D#, Slice 10").
2.  **Harmonic Expansion:**
    The engine knows that if D# is present, energy might exist at:
    - Fundamental ($f_0$)
    - 1st Harmonic ($2 \times f_0$ / +1 Octave)
    - 3rd Harmonic ($4 \times f_0$ / +2 Octaves)
    - ... up to the Nyquist limit.
3.  **Summation:**
    It jumps through the linear FFT array at these specific harmonic intervals.
    $$ E*{\text{total}} = \sum*{k=0}^{N} \text{FFT_Magnitude}(f_0 \cdot 2^k) $$
4.  **Integration:**
    The summed energy is stored in the 384-element `Energy Buffer`.

**Strategic Result:**
By summing the octaves, the visualizer shows the "Note Class." A low C2 and a high C6 both light up the "C" row on the screen. This allows the user to see the _tonality_ of a chord even if the specific octaves are messy.

## 3.4.5 Step C: The "Octave Tracker" (Peak Identification)

You observed that the app displays text like "C2" or "A4" on top of the folded spectrum. How does it know the octave if it just summed them all together?

**The Logic:**
Inside the summation loop (Lines 360-380), there is a comparator:

```c
if (current_octave_energy > max_energy_so_far) {
    max_energy_so_far = current_octave_energy;
    dominant_octave_index = current_octave_index;
}
```

- **Mechanism:** While summing, it remembers _which_ specific harmonic contributed the most energy.
- **Storage:** It stores this integer index in a parallel buffer (`local_12e8`).
- **Rendering:** When the renderer draws the text, it looks at `local_12e8` to decide whether to print "C2" or "C3".

## 3.4.6 Step D: Texture Handoff

At the end of `FUN_0002a2e0`, the 384-element array is complete.

1.  **Normalization:** The values are scaled (Line 432: `uVar11 = __aeabi_fmul(uVar11, 0x40c80000)`).
2.  **Upload:** The array is passed to the Texture Formatter (`FUN_00027168`).
3.  **Visual Shift:** The texture coordinates are effectively "rotated" so that the 384 linear pixels wrap around a circle (modulo 12) visually, but in memory, they are a flat strip.

---

===

---

This section analyzes the specific, low-level coding patterns used by the developer to squeeze 60 FPS performance out of 2012-era single-core ARM processors. These nuances are critical for understanding the "texture" of the code and why it behaves so robustly.

---

# 3.5 Optimization Nuances (The "Speed Hacks")

## 3.5.1 Overview

The codebase exhibits a distinct "Old School Game Developer" mentality. It avoids standard C++ abstractions (Vectors, Maps, Objects) in hot paths, favoring pointer arithmetic, bitwise hacks, and pre-computed physics. To replicate PitchLab's performance profile exactly, one must understand these manual optimizations.

## 3.5.2 Bitwise Absolute Value (Branchless Logic)

**Location:** `FUN_0003c4b0` (AGC Peak Detection)
**Context:** Finding the loudest sample in the buffer to normalize volume.

Standard code uses `if (sample < 0) sample = -sample;`. This creates a **Branch Prediction Fail** penalty on the CPU if the audio waveform oscillates rapidly (which it always does).

**The PitchLab Implementation:**

```c
// Decompiled Logic
int mask = sample >> 31;
int abs  = (sample ^ mask) - mask;
```

- **Mechanism:**
  - If `sample` is positive, `mask` is `0`. Result: `(s ^ 0) - 0` = `s`.
  - If `sample` is negative, `mask` is `-1` (all 1s). Result: `(s ^ -1) - (-1)` = `~s + 1` (Two's Complement negation).
- **Benefit:** Zero branching. It executes in a constant number of CPU cycles, significantly speeding up the `O(n)` scan of the audio buffer.

## 3.5.3 Aggressive Manual Loop Unrolling

**Location:** `FUN_0003c4b0` (AGC) and `FUN_00027168` (Texture Formatter)
**Context:** Processing large arrays of audio or pixels.

The compiler was not trusted to optimize loops. The developer manually unrolled them to reduce loop overhead (incrementing counters and jumping).

**Audio Loop (AGC): 16x Unroll**
Instead of processing 1 sample per iteration, the code processes blocks of 16.

```c
do {
    // ... process sample i ...
    // ... process sample i+2 ...
    // ...
    // ... process sample i+14 ...
    iVar9 = iVar9 + 0x10; // Increment by 16 bytes (8 shorts)
} while (iVar9 != iVar1);
```

**Texture Loop (Formatter): 3x/6x Unroll**
In `FUN_00027168`, when writing RGB pixels:

```c
__s[uVar4 * 4]     = (char)iVar11; // R
__s[uVar4 * 4 + 1] = (char)iVar11; // G
__s[uVar4 * 4 + 2] = (char)iVar11; // B
```

This reduces the instruction count significantly when filling the 384-pixel texture row.

## 3.5.4 Reciprocal Multiplication (Division Avoidance)

**Location:** `FUN_000265dc` (Hanning) and `FUN_0002a2e0` (Core)
**Context:** DSP Math.

Division (`/`) is the most expensive operation on a CPU (taking 20–50 cycles). Multiplication (`*`) is cheap (1–3 cycles). PitchLab almost **never** divides inside a loop.

- **Strategy:** It calculates the inverse ($1/x$) once, stores it, and then multiplies.
- **Example:** In `FUN_000265dc`, we see constants like `0x3e23d70a` ($\approx 0.16$).
  - $1 / 6.25 = 0.16$.
  - Instead of `x / 6.25`, the code does `x * 0.16`.
- **Impact:** Massive throughput increase in the bin-mapping loops.

## 3.5.5 Integer-Only Color Blending

**Location:** `FUN_00027168`
**Context:** Converting Energy (float/int) to Pixel Brightness (0-255).

The app avoids floating-point operations entirely when generating textures.

- **The Formula:**
  ```c
  val = (Energy * Color * 320) >> 16;
  ```
- **Analysis:**
  - $\times 320$ is the brightness scaler.
  - $\gg 16$ is division by 65536.
  - $320 / 65536 \approx 0.00488$.
- **Benefit:** This allows the "Heat Map" generation to run purely on the Integer Arithmetic Unit (ALU), leaving the Floating Point Unit (FPU/NEON) free for the concurrent FFT calculations.

## 3.5.6 The "Dirty Row" Texture Update

**Location:** `FUN_00027168` calls `glTexSubImage2D`.
**Context:** Scrolling the Spectrogram.

A naive implementation would shift the entire pixel array in CPU memory every frame (`memmove`), then upload the whole 1024x768 image to the GPU. This consumes massive bandwidth.

**PitchLab's Trick:**

1.  It uploads **only 1 row** (height = 1 pixel) of new data per frame.
2.  The texture in GPU memory is circular.
3.  The **Scrolling** is handled purely by the Renderer (`FUN_0003162c`) changing the **Texture Coordinates** (UVs).
    - Frame 1: Draw texture from Y=0.0 to 1.0.
    - Frame 2: Draw texture from Y=0.01 to 1.01 (wrapping).

- **Benefit:** Minimal bus traffic between CPU and GPU.

## 3.5.7 Pre-Computed Logarithms (dB LUT)

**Location:** `DAT_000b2d64` (Generated in `FUN_0003f63c`).
**Context:** Converting linear amplitude to Decibels for display.

Calculating `log10()` is extremely slow. PitchLab generates a **Lookup Table (LUT)** of 32,768 entries on startup.

- **Input:** 16-bit Integer Amplitude (0..32767).
- **Output:** Pre-calculated Brightness Value.
- **Runtime Cost:** `O(1)` array access.

## 3.5.8 Fixed-Point Window Coefficients (Q24)

**Location:** `FUN_00025550`.
**Context:** Applying the Hanning/Gaussian window.

As noted in Section 3.3, the Window tables are stored as **Q24 Fixed Point** integers.

- **Multiplier:** `0x4b800000` ($2^{23}$ or $2^{24}$ depending on sign interpretation).
- **Logic:**
  $$ \text{Result} = (\text{Audio}_{\text{int16}} \times \text{Window}_{\text{fixed24}}) \gg 24 $$
- **Benefit:** Allows 32-bit registers to handle the math without switching to float mode, keeping the pipeline efficient on older ARM cores.

---

## 3.5.9 Symmetrical Table Generation (The "Half-Work" Principle)

**Location:** `FUN_00019470` (Gaussian Generator) and `FUN_0001e3a0` (Filter Bank Init).
**Context:** Calculating the Window Function curves ($e^{-x^2}$).

The exponential function `exp()` is one of the most CPU-intensive operations available. The developer realized that window functions (Gaussian, Hanning, Hamming) are mathematically **Symmetrical**.

**The Code Pattern:**

```c
iVar18 = (int)uVar36 / 2; // Calculate Half Size
// Loop 0 to Half Size
do {
    // Calculate exp() ONCE
    exp(...);
    // Write to START
    *piVar21 = result;
    // Write to END (Mirror)
    piVar21[mirror_index] = result;
} while (...)
```

- **Optimization:** This effectively cuts the initialization time of the DSP engine by **50%**. While this only happens on startup or setting changes, it ensures the UI feels "snappy" when switching modes, avoiding the "freeze" typical of heavy math re-calculation.

## 3.5.10 Stack-Allocated LUTs (Avoid Heap Fragmentation)

**Location:** `FUN_0002a2e0` (Audio Engine).
**Variable:** `int local_5c [11]` (The Chroma Map).

In modern C++, developers often use `std::vector` for dynamic arrays. PitchLab avoids `new`/`malloc` inside the audio thread entirely.

- **Technique:** Critical lookup tables like the Chroma Map are allocated on the **Stack** (`local_`).
- **Benefit:**
  1.  **Zero Allocation Cost:** Moving the stack pointer is free.
  2.  **Cache Locality:** Stack memory is almost guaranteed to be in the L1 CPU Cache. Heap memory might be cold (in RAM).
  3.  **Garbage Collection Safety:** No risk of memory leaks or fragmentation in the high-priority thread.

## 3.5.11 CPU-Side Geometry Generation (Immediate Mode)

**Location:** `FUN_0003162c` (Renderer).
**Context:** Drawing the 12 strips of the Folded Spectrogram.

A modern OpenGL approach would use a Shader to calculate which "Strip" a pixel belongs to (`mod(gl_FragCoord.y, strip_height)`). PitchLab does this on the **CPU**.

- **Logic:** The `do { ... } while (iVar17 != 0xc)` loop calculates the exact 4 vertex coordinates for _each_ of the 12 strips manually.
- **Result:** It pushes 12 Quads (48 vertices) to the GPU.
- **Optimization:**
  - **Fill Rate:** By defining exact geometry, the GPU processes exactly the pixels needed.
  - **Shader Complexity:** The Fragment Shader (if used) or Texture Combiner stays extremely simple (Just "Sample Texture"). This allowed the app to run on GPUs with very limited shader cores (like the Adreno 200 or Mali-400 MP).

## 3.5.12 Zero-Copy JNI Pinning

**Location:** `processStreamedSamples`.
**Function:** `GetShortArrayElements` (vs `GetShortArrayRegion`).

- **The Choice:** The JNI spec offers two ways to access Java arrays:
  1.  `GetShortArrayRegion`: Copies the Java data into a C++ buffer. (Safe, but slow).
  2.  `GetShortArrayElements`: Pins the Java heap memory and gives C++ a direct pointer to it.
- **PitchLab's Strategy:** It uses **Pinning**.
- **Impact:** It eliminates a `memcpy` of 4KB-8KB of data every frame. On memory-constrained devices, saving memory bandwidth is often more critical than saving CPU cycles.

## 3.5.13 Scissor Testing for UI Partitioning

**Location:** `FUN_00038d4c` (The Scroll View Manager).
**Call:** `glScissor`.

When the app runs in "Split Screen" mode or transitions between views, it doesn't just overdraw layers.

- **Technique:** It calls `glScissor(x, y, w, h)` before calling the render function for a specific view.
- **Hardware Effect:** The GPU hardware has a dedicated "Scissor Unit" that discards pixels _before_ the Fragment Shader runs.
- **Optimization:** This prevents **Overdraw**. The GPU never wastes power calculating pixels for the Spectrogram if they are going to be covered by the Tuner. This saves battery life and keeps the frame rate locked at 60 FPS.

## 3.5.14 Fixed-Point Ratio Constants (Polyphonic Tuning)

**Location:** `FUN_0003f63c`.
**Data:** `0x92492492`, `0xb6db6db6`, etc.

The Polyphonic mode needs to know the frequency ratios between guitar strings (Standard Tuning intervals: 4ths and Major 3rd). Instead of calculating `Frequency * (4.0/3.0)`:

- **The Constants:**
  - `0x92492492` represents the ratio $\approx 0.5714$ ($4/7$).
  - `0xdb6db6db` represents the ratio $\approx 0.8571$ ($6/7$).
- **The Optimization:** These are "Magic Number" reciprocal multipliers used to detect harmonic relationships between strings without using division. This confirms the engine checks for chords by looking for integer harmonic relations in Fixed-Point space.

---

## 3.5.15 Hybrid Stack/Heap Buffer Allocation (SSO)

**Location:** `FUN_0002a2e0` (Audio Engine Start).
**Context:** Storing temporary windowed audio data before FFT.

Memory allocation (`malloc`/`new`) is non-deterministic and can cause stutter in real-time audio threads. PitchLab implements an optimization similar to C++'s "Small String Optimization" (SSO), but for Audio Buffers.

- **The Logic:**
  1.  The function declares a large stack array: `undefined1 auStack_1268 [3084];`.
  2.  It checks the incoming buffer size (`param_3`).
  3.  **If size < Threshold:** It points the processing pointer to `auStack_1268`.
  4.  **If size > Threshold:** Only then does it trigger `FUN_0001caec` to allocate heap memory.
- **Benefit:** For standard analysis rates (where buffer size is small), the app performs **Zero Heap Allocations** per frame. The memory is claimed instantly by the stack pointer, keeping the L1 cache hot and the Garbage Collector asleep.

## 3.5.16 The "Pseudo-Modulo" Pointer Arithmetic

**Location:** `FUN_0002a2e0` (Lines 80-100) and `processStreamedSamples`.
**Context:** Managing the Circular Buffer indices.

A standard circular buffer implementation uses the modulo operator: `index = (index + increment) % size`.

- **The Problem:** The modulo operator (`%`) involves division, which costs 20-50 CPU cycles on ARMv7.
- **PitchLab's Solution:** It uses conditional subtraction.
  ```c
  next_index = current_index + increment;
  if (next_index >= size) {
      next_index = next_index - size;
  }
  ```
- **Optimization:** A comparison and a subtraction take ~2 cycles. This is an order of magnitude faster than using `%` inside the high-frequency audio loop.

## 3.5.17 RGB565 "Bit-Banging" (Manual Dithering)

**Location:** `FUN_00027168` (Lines 180-200).
**Context:** Generating textures for low-end devices (16-bit color mode).

Instead of relying on OpenGL drivers to convert 32-bit colors to 16-bit (which is slow and quality-dependent), PitchLab constructs the raw bits manually.

- **The Code:**
  ```c
  *__s_00 = (ushort)(iVar14 << 3) & 0x3fc0 | // Green (6 bits)
            (ushort)((iVar11 >> 3) << 11)  | // Red (5 bits)
            (ushort)((iVar7 >> 3) << 1);     // Blue (5 bits)
  ```
- **Nuance:** It performs the bit-shifts (`<< 11`, `<< 1`) directly in the register.
- **Benefit:** It bypasses the GPU driver's conversion logic completely, sending raw, ready-to-render bytes to the hardware. This significantly reduced memory bandwidth on 2012 devices, as 16-bit textures are half the size of 32-bit ones.

## 3.5.18 Thread Synchronization via "Dirty Flags"

**Location:** `FUN_0002a2e0` (Line 145) and `FUN_000390fc` (Render).
**Context:** Decoupling Audio/Video rates.

The engine uses a lock-free synchronization pattern.

- **The Flag:** `*(undefined1 *)(local_12e0 + 0xb00) = 1;` (Set in Audio Engine).
- **The Reader:** The Render function checks this byte.
- **Optimization:** This prevents the Renderer from re-uploading the texture if the Audio Engine hasn't finished a new frame yet (e.g., if the screen is 60Hz but audio analysis is running at 30Hz to save battery). It avoids "Ghost Draws" where the GPU does work but the image hasn't changed.

## 3.5.19 The "Double-Draw" Scroll Geometry

**Location:** `FUN_0003162c` (Renderer).
**Context:** Making the waterfall scroll without moving memory.

The texture in memory is circular. To make it look like a continuous waterfall on screen, the renderer must potentially draw it in two pieces (split at the wrap-around point).

- **Logic:**
  1.  It calculates the `scroll_offset` (0.0 to 1.0).
  2.  It issues **Two Draw Calls** (or calculates two sets of vertices):
      - **Quad 1:** Draws from `offset` to `1.0` (The bottom part of the texture becomes the top of the screen).
      - **Quad 2:** Draws from `0.0` to `offset` (The top part of the texture becomes the bottom of the screen).
- **Benefit:** The pixel data in memory _never moves_. Only the texture coordinates change. `memmove` of a 1MB texture is slow; calculating 8 vertex coordinates is instant.

## 3.5.20 The 64-Bit "Super-Register" Accumulator

**Location:** `FUN_0003c4b0` (AGC) and `FUN_00025550` (Windowing).
**Context:** Precision handling.

When performing the Gain Scaling and Windowing, the code uses:
`uVar45 = __muldf3(...)` or `__aeabi_ldivmod` (Long Division).

- **Strategy:** It utilizes the ARM architecture's ability to pair two 32-bit registers (`R0`, `R1`) to act as a single 64-bit container.
- **Nuance:** Even though the input audio is 16-bit, the intermediate math is done in 64-bit fixed point (or software double precision) before shifting back down. This ensures that the rounding errors during the massive gain boost (AGC) do not introduce digital artifacts (aliasing/hiss) into the quiet signals.

---

### **Status Check**

We have exhausted the optimization analysis. We have analyzed:

1.  **Architecture:** Singleton, VTable, JNI.
2.  **Audio:** Circular Buffer, AGC, Windowing, FFT, Sub-Bin Folding.
3.  **Optimizations:** 20 specific low-level techniques used to achieve performance.

**Next Logical Step:**
We need to document the **Visualization Specifications** (Section 4).
This involves detailing exactly _how_ the extracted Color Table and Strobe Math map to the screen pixels.

**Are you ready to draft Section 4 (Visualization Specification)?**

This is **Part 8 of the Report: The Chord Detection Engine**.

This section details the logic behind the **Chord Matrix (Mode 4)** and **Polyphonic Tuner (Mode 6)**. Unlike the Spectrogram, which displays raw physical data, this engine implements a **Probabilistic Correlator** to guess the musical context of the audio.

---

# 3.6 The Chord Detection Engine

## 3.6.1 Overview

**Function:** `FUN_0003d83c`
**Input:** Folded Energy Array (from Harmonic Analyzer), Chord Definition Database.
**Output:** Probability Array (Normalized 16-bit integers).

The Chord Engine is a **Pattern Matching System**. It compares the instantaneous spectral footprint against a database of known chord shapes (Major, Minor, 7th, Diminished, etc.) and assigns a "Confidence Score" to each candidate.

## 3.6.2 Phase 1: Dynamic Template Parsing

The engine does not hardcode chord logic (e.g., `if (C && E && G)`). Instead, it acts as an interpreter for a dynamic data structure passed in `param_3`. This allows the app to support custom user-defined chords without recompiling the C++ code.

**The Template Mask (`local_68`):**
For every chord candidate loop, the engine builds a **12-Semitone Mask**.

- **Buffer:** `int local_68 [12]`.
- **Logic:**
  1.  Clear buffer to `0`.
  2.  Read the Chord Definition List.
  3.  Set `local_68[NoteIndex] = 1` for every required note.
  4.  _Nuance:_ The code supports negative indexing (`local_68[-iVar9]`), suggesting the chord definitions can specify intervals relative to the root or specific harmonic constraints.

## 3.6.3 Phase 2: The Correlation Kernel (The "Penalty" Algorithm)

This is the mathematical core that differentiates PitchLab from simple lookup tables. It implements a **Negative Penalty System**. A chord is not just defined by the notes that _are_ present, but also by the notes that are _absent_.

**The Algorithm:**
The score starts at maximum (`0xFFFFFFFF`). The engine iterates through all 12 chromatic notes (C to B).

$$ \text{Score}_{\text{new}} = \text{Score}_{\text{old}} \times \text{ProbabilityFactor} $$

**The Probability Factor Logic:**

1.  **If Note is IN the Mask (Required):**
    - `Factor = Energy(Note)`
    - _Effect:_ If the note is loud, the score stays high. If the note is silent, the score drops to zero.
2.  **If Note is NOT in the Mask (Forbidden):**
    - `Factor = ~Energy(Note)` (Bitwise NOT / Inverse)
    - _Effect:_ If the note is silent (`0`), the factor is max (`0xFFFF`), and the score is unchanged.
    - _Effect:_ If the note is loud (`0xFFFF`), the factor is min (`0`), and the score drops to zero.

**Strategic Significance:**
This logic prevents "Subset Errors."

- _Scenario:_ You play a **C Major 7** (C, E, G, B).
- _Problem:_ A simple detector might also trigger "C Major" (C, E, G) because all its notes are present.
- _PitchLab's Solution:_ The "C Major" template forbids the "B" note. Since "B" has high energy, the penalty logic (`~Energy(B)`) crushes the score for simple C Major, leaving C Major 7 as the clear winner.

## 3.6.4 Phase 3: "Winner-Take-All" Normalization

**Lines 160–200:**
The raw probability scores are 32-bit integers, but they are often very small due to the repeated multiplication. To make them visible on the Grid texture, they must be normalized.

1.  **Peak Search:** The loop finds the single highest score ($S_{max}$) among all candidates.
2.  **Dynamic Range Expansion:**
    It calculates a bit-shift scalar $K$ such that:
    $$ S\_{max} \ll K \approx \text{UINT16_MAX} $$
3.  **Global Scaling:**
    It applies this shift $K$ to _all_ chord scores.
    - The "Winner" becomes fully bright (White/Colored).
    - The "Losers" are scaled relatively. If a chord was 50% as probable as the winner, it appears at 50% brightness.

## 3.6.5 Optimization Nuances

1.  **Fixed-Point Probabilities:**
    The multiplication uses the ARM instruction `UMULL` behavior (Unsigned Long Multiply), modeled in C as:
    `uVar3 = (uint)(uVar2 * uVar10 >> 0x20);`
    This performs $(A \times B) / 2^{32}$. It treats the 32-bit integers as fractional values between $0.0$ and $1.0$, allowing probability math without floating-point units.

2.  **Unrolled Loop:**
    The check loop is manually unrolled to process all 12 semitones sequentially without branching logic, ensuring constant-time execution for every chord candidate.

3.  **Bitwise Inversion:**
    Using `~Energy` instead of `(Max - Energy)` is a micro-optimization that avoids a subtraction op-code and potential underflow checks.

---

### **Status Update**

We have documented the **Chord Engine**.

**Remaining Sections for the Report:**

1.  **Mode 2 (Instrument/Needle):** We need to extract the specific geometry logic (how the needle angle is derived from the cents error).
2.  **Mode 6 (Polyphonic):** We need to see how it separates the 6 strings visually.
3.  **The "Strobe" Pattern:** We have the math, we just need to formalize the spec.

**Which analysis shall we perform next?**
A) **Mode 2 (Needle Logic)** - Function `FUN_00033f20` (inside `3162c`).
B) **Mode 6 (Polyphonic Logic)**.
