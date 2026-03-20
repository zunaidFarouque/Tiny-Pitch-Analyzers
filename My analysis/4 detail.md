# Section 4: Visualization Specification

## 4.1 Rendering Architecture Overview

The PitchLab rendering engine operates as a highly optimized, immediate-mode OpenGL ES 1.1 pipeline. It is strictly decoupled from the audio processing thread.

- **Entry Point:** `FUN_000390fc` (Frame Manager) $\rightarrow$ `FUN_0003162c` (View Dispatcher).
- **Role:** The Renderer is a "Passive Consumer." It does not calculate frequencies or phases. It simply visualizes the data structures and textures that were updated by the Audio Engine in the background.
- **State Machine:** The rendering loop is a giant State Machine controlled by the `ViewMode` integer (0-11). It switches drawing routines instantly without re-initializing the OpenGL context, allowing for the app's signature smooth swiping transitions.

## 4.2 The "Shared Texture" Data Bridge

The primary method of communication between the C++ Audio Engine (DSP) and the C++ Rendering Engine (GPU) is **Texture ID 8**.

- **Structure:** A 2D Circular Buffer Texture.
- **Dimensions:** Width = 384 pixels (representing the chromatic scale), Height = Viewport Height (dynamic).
- **Format:**
  - **High Quality:** `GL_RGBA` (32-bit).
  - **Low Quality:** `GL_RGB565` (16-bit packed).
- **The Data Flow:**
  1.  **DSP Write:** The Audio Engine calculates energy for the current time slice and uses `glTexSubImage2D` to update _only_ the specific row index corresponding to `current_time % height`.
  2.  **GPU Read:** The Renderer binds this texture (`glBindTexture`) and generates geometry (Quads) to display it.
- **Benefit:** This architecture allows the complex "Waterfall" history to be stored entirely in GPU memory (VRAM). The CPU never needs to shift/copy the full history buffer, saving massive memory bandwidth.

## 4.3 The Coordinate System & Viewport

**Function:** `FUN_00026f58`
PitchLab utilizes a non-standard OpenGL coordinate system to align with Android's native touch input system.

- **Projection:** Orthographic (`glOrthof`).
- **Origin (0,0):** **Top-Left Corner**.
  - Positive X $\rightarrow$ Right.
  - Positive Y $\rightarrow$ Down.
- **Units:** Raw Pixels (1.0f = 1 Physical Pixel).
- **Scaling:** To ensure visuals look consistent across devices (320px wide phones vs 2560px tablets), the engine calculates three global scalars on startup:
  - `uiScaleGlobal` (Offset `0xb1c`): Base scalar derived from screen width.
  - `uiScaleLine` (Offset `0xb18`): Thickness for grid lines and needles.
  - `uiScaleFont` (Offset `0xb14`): Scaling factor for FreeType text rendering.

## 4.4 The View Dispatcher Logic

**Function:** `FUN_0003162c`
The heart of the renderer is a large `switch` statement (implemented as `if/else if` chains in the binary) that routes execution to specific geometry generators based on `param_2` (View Mode).

**The Render Loop Sequence:**

1.  **Clear Screen:** `glClear(GL_COLOR_BUFFER_BIT)`.
2.  **Apply Scroll:** If the user is swiping between views, `FUN_00038d4c` applies a global `glTranslatef(scroll_x, 0, 0)`.
3.  **Scissor Test:** `glScissor` is used to clip drawing to the visible area, preventing "Overdraw" (wasted GPU cycles) on views that are partially off-screen during a swipe.
4.  **Mode Execution:** The specific logic for the active mode (Spectrogram, Strobe, etc.) is executed.
5.  **Overlay:** Common UI elements (Menu buttons, Lock indicators) are drawn on top.

## 4.5 Shared Graphical Assets

The engine relies on three immutable assets extracted from the binary to maintain visual consistency across all modes.

1.  **The Color LUT (`00056d00`):**
    A 12-entry RGB table defining the app's color theory.
    - **Usage:** Used in Spectrogram, Polyphonic Tuner, and Chord Matrix to color-code notes (C=Lime, A=Red).
2.  **The dB Brightness Curve (`DAT_000b2d64`):**
    A pre-calculated Lookup Table mapping signal amplitude to pixel alpha/brightness.
    - **Usage:** Ensures the visualization matches the logarithmic response of the human ear.
3.  **The Strobe Gradient Texture:**
    A procedurally generated texture (created in `FUN_0002f6b8`) containing a mathematical "Sawtooth-Squared" pattern.
    - **Usage:** Used exclusively by the Strobe Tuner modes to simulate the mechanical rotation of a strobe disk.

---

This is the **Deep Analysis of the Visualization Logic** for Mode 3 (Pitch Spectrogram).

We are dissecting the code block inside `FUN_0003162c` where `param_2 == 3` (Starting around Line 284 in the large paste).

### 1. The Core Concept: "Rasterizing the Chroma"

The visualizer does not draw a standard frequency graph. It performs a geometric transformation I call **"Strip Slicing."**

- **The Data in Memory:** The Audio Engine (`FUN_0002a2e0`) and Texture Formatter (`FUN_00027168`) produced **Texture #8**. Conceptually, this texture is a single, tall cylinder of time-history, where the X-axis (width 384 pixels) represents the Chromatic Scale (C $\to$ B) linearly.
- **The Visual on Screen:** The user sees 12 distinct horizontal lanes stacked vertically.

**The Render Loop (Lines 306-330):**

```c
do {
    // ... Math to calculate strip position ...
    FUN_000249b8(...); // Draw Quad
    iVar17 = iVar17 + 1;
} while (iVar17 != 0xc); // Loop 12 times
```

- **The Logic:** The renderer breaks the single 384-pixel wide texture into **12 chunks** of 32 pixels each.
- **The Mapping:**
  - **Iteration 0 (Bottom):** Draws Texture columns 0–31 (Note C).
  - **Iteration 1:** Draws Texture columns 32–63 (Note C#).
  - **...**
  - **Iteration 11 (Top):** Draws Texture columns 352–383 (Note B).

### 2. The "Infinite Scroll" Mechanism

The spectrogram falls downwards (or moves sideways in landscape). The app does **not** move pixel data in memory to achieve this.

- **Variable:** `uVar5` (Line 285) reads from `param_1 + 0x80`.
  - This is the global **Time Accumulator**. It increments constantly as audio is processed.
- **The Math:**
  Inside the loop, the Texture Coordinates (UVs) are calculated by adding this accumulator to the base coordinates.
  ```c
  local_230 = __addsf3(uVar13, uVar6); // V_coord + Scroll_Offset
  ```
- **The Result:** The geometry stays fixed on screen (the 12 strips don't move). The _texture_ slides through the strips. When the V-coordinate exceeds 1.0, the GPU's `GL_REPEAT` wrap mode (or manual modulo math seen in lines 310-320) handles the seamless looping.

### 3. The Octave Labeling System (The "Smart Overlay")

You asked previously how it distinguishes "C2" from "C3" if the data is folded. The analysis reveals the answer lies in a secondary data stream.

- **The Data Source:** `local_650` points to `param_1 + 0x8a0`.
  - This is the **Octave Index Buffer** populated by the Audio Engine. It contains one integer per "Slice" (0-383) indicating the dominant octave (e.g., 2, 3, 4) for that specific micro-pitch.
- **The Logic (Lines 360-390):**
  The renderer iterates through the active strips. It checks the energy level.
  ```c
  if (*local_650 < 0) { ... } // Check if valid octave exists
  ```
  If a valid peak is detected in the energy texture, it reads the integer from the Octave Buffer (e.g., `3`).
- **The Draw Call:**
  It then calls `FUN_000261a4` (Text Renderer) to draw the string "C3" (constructed dynamically) at the exact horizontal position of the peak.
- **Significance:** The "Heat Map" provides the _intensity_, but this separate integer buffer provides the _metadata_.

### 4. The Grid Generation

The horizontal lines separating the notes are not part of the texture. They are drawn as separate vector geometry.

- **Lines 354-358:**
  ```c
  FUN_00024a84(..., 0xff000000); // Draw Black Line (Shadow)
  FUN_00024a84(..., 0xffffffff); // Draw White Line (Highlight)
  ```
- This creates a 1-pixel "embossed" look for the grid lines, making the strips look like physical grooves.

### 5. Summary of Mode 3 Visualization Logic

To recreate the Pitch Spectrogram, the rendering pipeline must:

1.  **Bind Texture 8** (The 384xHeight History Buffer).
2.  **Loop 0 to 11 (Notes):**
    - Calculate Screen Y coordinates for the current strip (Height / 12).
    - Calculate Texture U coordinates: `Start = NoteIndex / 12.0`, `End = (NoteIndex + 1) / 12.0`.
    - Calculate Texture V coordinates: `Start = Time`, `End = Time + ViewHeight`.
    - **Draw Quad.**
3.  **Loop 0 to 11 (Labels):**
    - Scan the "Octave Buffer" for the current note.
    - If a value > -1 is found (meaning significant energy exists), determine the text label (e.g., "A" + "4").
    - **Draw Text** overlay at that position.

This confirms that the visual complexity is achieved by **deconstructing** a single linear texture into a 2D stacked grid on the GPU.

---

===

---

This is **Part 6 of the Report: Visualization Detail (Pitch Spectrogram)**.

This section deconstructs the specific logic used to render **Mode 3: Pitch Spectrogram**. This is the app's signature view, often described as "Octave Folded."

The analysis reveals a clever disconnect between the Data Structure (Linear) and the Visual Structure (Stacked).

---

# 4.6 Visualization Detail: Mode 3 (Pitch Spectrogram)

## 4.6.1 Concept: The "Folded" Projection

Standard spectrograms map Frequency ($X$) vs. Time ($Y$). PitchLab maps **Note Class** ($Y$) vs. **Time** ($X$), with "Cents" deviation determining the vertical position within the note's band.

- **The Data Reality:** The texture contains a single, long horizontal line of 384 pixels representing the chromatic scale from Low C to High B linearly.
- **The Visual Trick:** The renderer slices this long texture into 12 equal segments and stacks them vertically. This makes all "C" notes (C2, C3, C4) appear in the same horizontal lane ("Folded"), while the intensity (brightness) tells you if the note is present.

## 4.6.2 The Geometry Engine

**Function:** `FUN_0003162c` (Lines 284–440)

The rendering loop iterates exactly **12 times** (once for each chromatic note C...B). For every iteration, it generates a Quad (Rectangle) with specific texture coordinates.

### 1. The Screen Coordinates (Vertices)

The screen is divided vertically into 12 bands.

- **Strip Height:** `ViewHeight / 12`.
- **Y-Position:** Calculated as `Index * StripHeight`.
- **X-Position:** Spans the full width of the screen (`0` to `ViewWidth`).

### 2. The Texture Coordinates (UVs)

This is where the "Folding" happens. The renderer maps specific slices of the source texture to the screen strips.

- **Source Texture Width:** 384 Pixels (Logical 1.0).
- **Slice Width:** $1.0 / 12 \approx 0.0833$.
- **Mapping:**
  - **Strip 0 (Note C):** Maps Texture $U=0.0$ to $U=0.0833$.
  - **Strip 1 (Note C#):** Maps Texture $U=0.0833$ to $U=0.1666$.
  - **...**
  - **Strip 11 (Note B):** Maps Texture $U=0.9166$ to $U=1.0$.

**Impact:** This forces the linear array `[C, C#, D ... A#, B]` stored in VRAM to appear as a vertical stack.

## 4.6.3 The "Waterfall" Scrolling Logic

The texture is a **Circular Buffer**. The pixel data does not move; the "Camera" moves.

- **Variable:** `param_1 + 0x80` (Scroll Y Offset).
- **Logic:**
  - The Audio Engine writes new lines to the texture at `WriteHead`.
  - The Renderer calculates the Vertical Texture Coordinate ($V$) based on time:
    $$ V*{\text{start}} = \text{Time} \times \text{SpeedScalar} $$
        $$ V*{\text{end}} = V\_{\text{start}} + 1.0 $$
- **OpenGL Wrap:** The texture parameter `GL_TEXTURE_WRAP_T` is likely set to `GL_REPEAT`. This allows the renderer to request $V=1.5$ and have the GPU automatically fetch from $V=0.5$, creating an infinite scrolling illusion with zero CPU overhead.

## 4.6.4 The Sub-Bin Resolution (Visualizing Cents)

Inside each strip, there are 32 vertical pixels (derived from the 32 sub-slices in the DSP engine).

- **Center Pitch:** Pixel 16 represents the perfect note frequency.
- **Sharp/Flat:** Energy at Pixel 20 indicates the note is Sharp. Energy at Pixel 12 indicates Flat.
- **Visual Result:** A note that is vibrato-ing (pitch bending) appears as a wavy line moving up and down _within_ the note's strip.

## 4.6.5 The Octave Overlay (Text Labeling)

While the visual folding hides the octave (C3 looks like C4), the text overlay restores this information.

- **Data Source:** The integer array at `local_12e8` (populated in the DSP engine).
- **Logic:**
  1.  The renderer loops through the 12 strips.
  2.  For each strip (e.g., "A"), it checks the "Dominant Octave Index" stored by the DSP engine.
  3.  **Threshold Check:** If the total energy for that note is below a "Silence Threshold" (controlled by the Sensitivity setting), no text is drawn.
  4.  **Drawing:** If energy is sufficient, it calculates the note name (e.g., "A") and appends the octave index (e.g., "4") to form string "A4".
  5.  **Placement:** The text is drawn via `FUN_000277a0` (FreeType Wrapper) at the right edge of the strip.

## 4.6.6 Re-Implementation Summary

To recreate this mode:

1.  **Shader/Geometry:** You do not need a complex shader. Use a simple Textured Quad shader.
2.  **Mesh:** Generate a mesh with 12 separate horizontal bands.
3.  **UV Mapping:** Hardcode the U-coordinates of each band to sample 1/12th of the texture width.
4.  **Animation:** Uniformly increment the V-offset of all bands based on `deltaTime`.
5.  **Data:** Bind the texture generated by the Audio Engine (which contains the 12x32 packed chromatic data).

---

===

---

# 4.7 Visualization Detail: Mode 4 (Chord Matrix)

## 4.7.1 Concept: The Probability Grid

The Chord Matrix visualizes the harmonic relationship between detected frequencies. It displays a Grid where:

- **Y-Axis (Rows):** The 12 Chromatic Root Notes (C, C#, D...).
- **X-Axis (Columns):** Chord Qualities (Major, Minor, etc.).
- **Cells:** A lit cell indicates a high probability that this specific chord is being played.

## 4.7.2 The Data Source: The "Winner" Buffer

**Function:** `FUN_0003162c` (Lines 293–440)
**Memory Read:** `param_1 + 0xB00` (iterating backwards)

The rendering logic does _not_ calculate chords. It reads a pre-processed "State Array" populated by the Audio Engine's post-processing routine (likely `FUN_00028bc8`).

- **Pointer Logic:**
  ```c
  local_698 = (uint *)(param_1 + 0xb00); // Start at offset 0xB00
  // Inside the loop:
  local_698 = local_698 + -1; // Walk backwards in memory
  uVar40 = *local_698;        // Load Chord Probability/Status
  ```
- **Data Structure:** The memory region `0xAC0` to `0xB00` contains integer flags representing the confidence level of potential chords.

## 4.7.3 The Rendering Loop

The visual is constructed using a fixed loop of 12 iterations (corresponding to the 12 Root Notes).

### 1. The Grid Geometry

The renderer draws the matrix using immediate-mode Quad generation.

- **Vertical Spacing:** `ViewHeight / 12`.
- **Horizontal Spacing:** Dynamic based on the number of detected chord types active.
- **Function:** `FUN_00024a84` is called repeatedly to draw the rectangular cells.

### 2. The Color Coding (Root-Based)

The specific color of a lit cell is determined by its **Row (Root Note)**, not the chord type.

- **Lookup:** It uses the same **Color LUT** (`00056d00`) as the Spectrogram.
- **Example:**
  - If **C Major** is detected, the cell is **Lime Green** (C's color).
  - If **A Minor** is detected, the cell is **Red** (A's color).
- **Transparency:** The code uses colors like `0x60000000` and `0x60ffffff`.
  - `0x60` (Alpha) $\approx$ 37% Opacity.
  - This creates a "Glassy" look where the grid is semi-transparent until lit.

### 3. The Activation Logic

The code allows for multiple chords to be "Active" but with different visual weights.

- **Logic:**
  ```c
  iVar9 = __aeabi_fcmpgt(uVar40 & 0x7fffffff, 0); // Check if State > 0
  if (iVar9 != 0) {
      // Draw Highlighted Cell
  }
  ```
- This confirms the Audio Engine filters the chords and outputs a value $> 0$ only for the candidates that pass the logic threshold. The renderer simply lights them up.

### 4. Text Labels (The Dual-Axis)

The text rendering (Lines 370-430) is split into two passes:

1.  **Row Labels:** Draws "C", "C#", "D"... on the left/right edge.
    - Calculated via `0xb - iVar45` (Drawing bottom-to-top).
2.  **Column Labels:** Draws "Maj", "Min", "7" headers.
    - These labels are likely stored in the string table referenced by `piVar11`.

## 4.7.4 Re-Implementation Summary

To recreate the Chord Matrix:

1.  **Data:** You need a "Chord Detector" algorithm (separate from the FFT) that outputs a list of `(Root, Quality, Probability)`.
2.  **Grid:** Render a 12-row grid.
3.  **Coloring:**
    - Compute Color = `ColorTable[RootIndex]`.
    - If `Probability > Threshold`, render Cell with `Alpha = 1.0`.
    - Else, render Cell with `Alpha = 0.2` (Dim/Inactive).
4.  **Text:** Overlay standard font textures for the grid headers.

---

===

---

This is **Part 9 of the Report: Visualization Detail (Instrument Tuner)**.

This section details **Mode 2: The Instrument Tuner**. Unlike the Spectrogram (which renders data) or the Chord Matrix (which renders probability), this mode renders **Vectors**.

It simulates an analog needle with physics-based damping and high-quality anti-aliasing, achieved not by texture sprites, but by procedural geometry generation on the CPU.

---

# 4.8 Visualization Detail: Mode 2 (Instrument Tuner)

## 4.8.1 Concept: The Analog Simulation

The Instrument Tuner provides the classic "Needle" experience.

- **Visual Logic:** A needle rotates around a bottom-center pivot.
- **Physics:** The needle does not jump instantly to the new pitch. It uses a "History Trail" to simulate mechanical inertia and motion blur.
- **Precision:** The dial is zoomed to show fine deviation ($\pm 50$ Cents range).

## 4.8.2 The Geometry Engine: Vector Math

**Function:** `FUN_0003162c` (Lines 448–726) calling `FUN_00026ba4` (Line Drawer).

The needle is not a rotated image. It is a dynamically generated polygon.

### A. The Angle Calculation

The renderer calculates the needle's angle ($\theta$) based on the "Cents" deviation stored in the Engine state.

- **Source:** `param_1 + 0xB08` (Current Pitch Error).
- **Scaling Constant:** `0x3cab92a6` ($\approx 0.020944$).
  $$ \theta = \text{Cents} \times 0.020944 $$
- **Conversion:** $0.020944 \text{ radians} \approx 1.2^{\circ}$.
- **Range:** A 50-cent error results in a $60^{\circ}$ rotation. This visual amplification makes fine-tuning easier.

### B. The "Motion Blur" Loop

The dispatcher loop iterates through a history buffer stored at `param_1 + 0x104`.

- **Logic:** It draws the needle multiple times in a single frame.
- **Fading:** Older entries in the history buffer are drawn with lower Alpha (Transparency).
- **Result:** This creates a "Ghosting" or "Trail" effect behind the needle, smoothing out jittery pitch detection without introducing lag in the actual "Current" value.

## 4.8.3 The Line Renderer (`FUN_00026ba4`)

This helper function is a **Custom Vector Rasterizer**. Standard OpenGL 1.1 `glLineWidth` is notoriously inconsistent across Android devices. PitchLab solves this by building lines out of triangles manually.

**The Algorithm:**

1.  **Input:** Start Point $(x_1, y_1)$, End Point $(x_2, y_2)$, Width $w$, Color $C$.
2.  **Vector Calculation:**
    - Direction Vector $\vec{D} = (x_2-x_1, y_2-y_1)$.
    - Normalize $\vec{D}$ to length 1.0.
    - Calculate Normal Vector $\vec{N} = (-D_y, D_x)$.
3.  **Vertex Generation:**
    It extrudes the line into a rectangle (Quad) by offsetting vertices along the Normal Vector.
    $$ V*{\text{left}} = P + (\vec{N} \times w) $$
    $$ V*{\text{right}} = P - (\vec{N} \times w) $$

## 4.8.4 The "Multi-Pass" Visual Polish

`FUN_00026ba4` does not just draw the quad once. It performs **3 Passes** to create a high-quality, "Physical" look:

1.  **Pass 1: The Shadow (Line 48-50)**
    - **Color:** Hardcoded `0xFF000000` (Black).
    - **Offset:** The vertices are slightly offset.
    - _Effect:_ Creates a drop-shadow, lifting the needle off the background.
2.  **Pass 2: The Core (Line 52-54)**
    - **Color:** The input color (Green/Red based on pitch) with **Alpha 255** (Opaque).
    - _Effect:_ The solid body of the needle.
3.  **Pass 3: The Glow/AA (Line 55)**
    - **Color:** Input color with **Alpha 128** (Semi-Transparent).
    - **Width:** Slightly wider than the core.
    - _Effect:_ Acts as manual Anti-Aliasing (AA) and gives the needle a glowing LCD appearance.

## 4.8.5 Text Labels

The text "IN TUNE", "TUNE UP", "TUNE DOWN" is handled by simple string logic (Lines 661+) and rendered using the shared FreeType font engine (`FUN_000277a0`).

---

===

---

This is **Part 10 of the Report: Visualization Detail (Polyphonic Tuner)**.

This section details **Mode 6: Polyphonic Tuner**. This is one of the most computationally impressive features, as it effectively runs 6 separate strobe tuners simultaneously in real-time, spatially separating the audio signal into distinct "String Lanes."

---

===

---

Here is the **Combined and Definitive Section 4.9**.

I have merged the structural overview with the deep-dive harmonic analysis into a single, comprehensive specification. Use **this version** for the Final Report.

---

# 4.9 Visualization Detail: Mode 6 (Polyphonic Tuner)

## 4.9.1 Concept: The Multi-Lane Harmonic Strobe

The Polyphonic Tuner visually isolates up to 6 strings simultaneously. Unlike simple tuners that show a single pitch, this mode implements a **Harmonic Strobe Stack**.

Each "String Lane" is subdivided vertically into multiple **Frequency Bands** (Fundamental, 2nd Harmonic, 3rd Harmonic). This allows the user to visualize **Inharmonicity** (physical imperfections in the string) and **Timbre** (brightness of the tone) in real-time.

- **Lane Logic:** Represents a physical string (E, A, D, G, B, E).
- **Band Logic:**
  - **Bottom Band:** Driven by the Fundamental ($f_0$).
  - **Middle Band:** Driven by the 2nd Harmonic ($2f_0$).
  - **Top Band:** Driven by the 3rd Harmonic ($3f_0$).
- **Visual Feedback:** If the string is in tune, the pattern freezes. If sharp, it scrolls up; if flat, it scrolls down. If the intonation is bad, the bottom band might freeze while the top bands drift.

## 4.9.2 The Layout Engine (Spatial Separation)

**Function:** `FUN_0003162c` (Lines 729–770)

The screen is partitioned into vertical slices defined by hardcoded coordinate arrays on the stack (`local_ec`, `local_13c`, `local_114`).

1.  **Activity Gating:**

    - The renderer iterates through string indices (0 to 5).
    - It calls `FUN_00021894` to check an "Activity Flag" from the Audio Engine.
    - If a string is not vibrating (Amplitude < Threshold), its lane is rendered dark or skipped entirely to reduce visual clutter.

2.  **Geometry Generation:**
    - **X-Axis:** Determined by the lane index and screen width.
    - **Y-Axis:** Inside each lane, the renderer generates a stack of **Quads** (rectangles), one for each monitored harmonic.

## 4.9.3 The Strobe Math (Per-Harmonic Physics)

**Function:** `FUN_0003162c` (Lines 775–840)

The "spinning" effect is a texture coordinate manipulation driven by a **Phase Accumulator**.

**A. Data Source (The Phase Array):**
The rendering loop reads a buffer of **16-bit Signed Integers** at `param_1 + (StringIndex * 52) + 0x30C`.

- `val[0]`: Accumulated Phase Error of Fundamental.
- `val[1]`: Accumulated Phase Error of 2nd Harmonic.
- `val[2]`: Accumulated Phase Error of 3rd Harmonic.

**B. Phase Normalization & Texture Shift:**
The integer phase is converted to a Texture V-Coordinate to scroll the "Sawtooth" gradient.

```c
// Line 795
float raw_phase = (float)*(short*)(buffer_ptr + harmonic_index * 2);
float v_coord   = raw_phase * 3.66e-5; // (0x3819999a)
```

- **Math:** $32767 \times 3.66 \times 10^{-5} \approx 1.2$.
- **Result:** The texture coordinate wraps smoothly from 0.0 to 1.0 (and slightly beyond for alignment), creating an infinite loop effect as the integer overflows.

**C. Alpha Blending (Timbre Visualization):**
The brightness of each band is not static. It is modulated by the signal strength of that specific harmonic.

```c
// Line 815
int alpha = (int)signal_strength;
if (alpha > 254) alpha = 255;
```

- **Pluck Physics:**
  - Plucking near the bridge excites higher harmonics $\rightarrow$ Top bands glow brighter.
  - Plucking near the neck excites the fundamental $\rightarrow$ Bottom band glows brighter.

## 4.9.4 Rendering Implementation

1.  **Texture Binding:** The Strobe Gradient (Sawtooth-Squared) is bound.
2.  **Blend Mode:** `GL_MODULATE` (Multiply).
3.  **Draw Order:**
    - Loop Strings (0..5):
      - Loop Harmonics (0..2):
        - Calculate UVs based on Phase.
        - Calculate Alpha based on Energy.
        - Draw Quad via `FUN_00024a84`.
4.  **Overlay:** Note names (E, A, D...) are drawn at the bottom using the FreeType font engine (`FUN_000268f0`).

## 4.9.5 Re-Implementation Summary

To replicate Mode 6, the DSP engine must output a vector of **(Phase, Energy)** tuples for specific harmonic series, not just a single pitch value. The Renderer then visualizes this vector as a stacked, scrolling heatmap.

---

===

---

This is **Part 11 of the Report: Visualization Detail (Radial Strobe)**.

This section analyzes **Mode 9: The Radial Strobe**. This is the most geometrically complex mode in the application. While the Spectrogram and Polyphonic modes rely on rectangular grids, this mode must map linear textures onto a polar (circular) coordinate system using an OpenGL engine that lacks native polar mapping features.

---

# 4.10 Visualization Detail: Mode 9 (Radial Strobe)

## 4.10.1 Concept: The Virtual Mechanical Strobe

This mode simulates a physical strobe tuner disc.

- **Visuals:** A spinning donut-shaped wheel.
- **Behavior:** The wheel spins clockwise (Sharp) or counter-clockwise (Flat). The speed of rotation is locked to the frequency difference.
- **Texture:** It uses the same "Sawtooth-Squared" gradient as the Polyphonic mode, but wraps it into a ring.

## 4.10.2 The Geometry Engine: "Segmented Ring" Construction

**Function:** `FUN_0003162c` (Lines 1020–1180)

Since OpenGL ES 1.1 cannot easily map a rectangular texture to a circle in the pixel shader, PitchLab constructs the circle geometrically on the CPU. It builds the "Donut" out of many small, rotated Rectangles (Quads).

### A. The Loop Structure

The rendering loop iterates to build the ring segment-by-segment.

- **Resolution:** The loop runs from `0` to roughly `0x80` (128 segments).
- **Logic:** For each segment:
  1.  Calculate current Angle $\theta$.
  2.  Calculate Inner Vertex: $P_{in} = (R_{in} \cos \theta, R_{in} \sin \theta)$.
  3.  Calculate Outer Vertex: $P_{out} = (R_{out} \cos \theta, R_{out} \sin \theta)$.
  4.  Draw a Quad connecting current $P$ to the previous $P$.

### B. The Texture Mapping (The "Spin")

Crucially, the geometry is static (it's always a circle), but the **Texture Coordinates** move.

- **Phase Accumulator:** The engine reads the current phase of the fundamental frequency ($f_0$).
- **Mapping:**
  - $\theta_{visual} = \theta_{geometric} + \Phi_{audio}$.
  - The texture $U$ coordinate is mapped to the angle.
  - As the audio phase ($\Phi$) drifts, the starting $U$ coordinate shifts.
- **Result:** The texture appears to rotate around the ring, even though the vertices are fixed.

## 4.10.3 The Outer Tick Marks (Cents Deviation)

Surrounding the main strobe wheel is a ring of static "Tick Marks" that act as a scale.

**Lines 1115–1160:**

- **Loop:** Iterates from `-50` to `+50` (representing Cents).
- **Math:**
  - Constant: `0x3c64c389` ($\approx 0.0139$).
  - Formula: $\text{Angle} = \text{CentIndex} \times 0.0139$.
  - Conversion: $0.0139 \text{ radians} \approx 0.8^{\circ}$.
- **Rendering:**
  It draws small tick lines at these calculated angles.
  - **Major Ticks:** Every 10 cents (Longer line).
  - **Minor Ticks:** Every 5 cents (Shorter line).

## 4.10.4 The "Lock" Indicator

The visual color of the ring changes based on tuning status.

- **Color Source:** Uses the **Color LUT** based on the detected note (e.g., C# = Green).
- **Blending:**
  - **Ring:** `GL_MODULATE` (Texture $\times$ Color).
  - **Overlay:** A "Highlight" arc is drawn on top when the pitch is within the "In-Tune Threshold" (set in config). This creates the glowing green segment effect seen at the top of the screenshot.

## 4.10.5 Text Overlay

The center of the donut is empty. The engine utilizes this space to draw the large Note Name and Cents deviation.

- **Function:** `FUN_000268f0` (Font Engine).
- **Centering:** The text position is calculated relative to the viewport center `(Width/2, Height/2)`.

## 4.10.6 Re-Implementation Summary

To recreate Mode 9:

1.  **Mesh Generation:** Procedurally generate a "Triangle Strip" or a ring of Quads.
    - Vertices: `(cos(t)*r1, sin(t)*r1)` and `(cos(t)*r2, sin(t)*r2)`.
2.  **Shader:**
    - Pass `PhaseOffset` as a Uniform.
    - Calculate `UV.x = (atan2(y, x) / 2PI) + PhaseOffset`.
    - Sample the Strobe Texture.
3.  **Optimization:** Do not regenerate the mesh every frame. Generate it once on startup, and only update the Rotation Uniform or Texture Matrix.
