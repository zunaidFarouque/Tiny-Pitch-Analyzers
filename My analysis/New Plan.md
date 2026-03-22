# **Chronological Master Implementation Roadmap: Re-Engineering PitchLab Pro (v1.0.18) for Modern Desktop Architecture**

## **1\. Architectural Deconstruction and Strategic Overview**

### **1.1 Project Objective and Fidelity Mandate**

This document establishes the definitive engineering roadmap for the reconstruction of PitchLab Pro (v1.0.18), specifically targeting a migration from its legacy Android ARMv7 binary origin to a robust, high-performance C++20 desktop application utilizing the JUCE 8 framework. The primary technical directive governing this reconstruction is **strict algorithmic fidelity**. The original application, widely regarded for its exceptional sensitivity and visual stability, relies on a specific set of integer-based Digital Signal Processing (DSP) techniques, bitwise optimizations, and fixed-point arithmetic that deviate significantly from modern floating-point standards.

Re-engineering this system requires a forensic approach. We are not merely porting code; we are preserving a specific "computational character." The analysis of the source binary (libPitchLab.so) reveals a "Black Box" architecture where a monolithic C++ Singleton manages all logic and rendering via a raw OpenGL ES 1.1 immediate-mode pipeline.1 This engine bypasses standard abstractions to achieve 60 FPS on 2012-era hardware. To replicate the "3-cent zoom" precision of the Spectrogram and the rock-solid stability of the Strobe Tuner, we must faithfully reimplement these low-level behaviors—including their quantization noise and rounding characteristics—within the context of a modern, cross-platform desktop environment.

The transition to JUCE 8 presents specific challenges, notably the deprecation of legacy OpenGL functionality and the introduction of the Direct2D renderer on Windows, which necessitates a hybrid rendering approach.2 This roadmap addresses these architectural collisions through a chronological, phased implementation plan that isolates the legacy "Physics Engine" from the modern "Host Shell."

### **1.2 The "Black Box" Singleton Architecture**

The core of the legacy system operates on a strictly defined memory model that acts as a singular, cohesive unit—a "Singleton" engine. This architecture is diametrically opposed to the object-oriented, component-based design patterns typical of modern JUCE applications.

In the original binary, the entire application logic resides in a single C++ Object instance, managed via a Virtual Function Table (VTable) located at memory address 0x000582e0.1 This object is allocated as a contiguous block of **2904 bytes** (0xB58).1 This memory block acts as the central nervous system of the application, holding everything from the FFT history buffers to the UI scroll offsets and texture IDs.

The interaction model is strictly unidirectional:

1. **The Shell (Java/JUCE):** Acts solely as an I/O controller. It has zero knowledge of music theory, frequencies, or visuals. It simply pumps raw byte streams into the engine and provides a surface for the engine to draw upon.
2. **The Engine (C++):** A high-performance kernel that handles all DSP, Logic, and OpenGL Rendering. It uses heavily optimized integer math and pre-calculated look-up tables (LUTs) to minimize CPU usage.

To maintain fidelity, the C++20 implementation must mirror this architecture. We will define a central PitchLabCore class that encapsulates the 2904-byte state structure, ensuring that "hot" variables—such as the circular buffer write head (DAT_00062cd8) and the current detected Hertz (Offset 0xB04)—maintain their spatial locality to preserve cache coherency, which was a critical optimization in the original design.1

### **1.3 The VTable Offset Map**

The interface to this Singleton is defined by a Virtual Function Table. Our C++20 implementation will replicate this interface to ensure a clean separation between the host application and the core logic. The critical entry points identified in the binary analysis are:

| Offset   | Function Address | Function Name    | Strategic Role                                                          |
| :------- | :--------------- | :--------------- | :---------------------------------------------------------------------- |
| **0x00** | 0002df88         | Destructor       | Handles cleanup and deallocation of internal buffers.                   |
| **0x04** | 0002edd0         | Reset            | Clears history buffers and resets the circular buffer pointers.         |
| **0x08** | 00026f58         | SetViewport      | Handles screen resize events, rotation, and recalculates UI scalars.    |
| **0x0C** | 0002a2e0         | **ProcessAudio** | The high-priority DSP entry point. Triggers the analysis pipeline.      |
| **0x10** | 000390fc         | **DrawFrame**    | The rendering entry point. Triggers the View Dispatcher (FUN_0003162c). |

Table 1: The Virtual Function Table (VTable) definitions derived from reverse engineering analysis.

## ---

**2\. Phase 1: Architectural Foundation and Memory Model**

**Objective:** Establish the static memory structures, the central Engine Singleton, and the thread-safe data ingress pipeline using C++20 standards while mirroring the memory layout of the original binary.

### **2.1 The Engine State Structure (Memory Reconstruction)**

The first implementation task is to define the PitchLabEngine structure. While modern C++ encourages encapsulation, this system relies on the raw availability of state variables to multiple subsystems (Audio, Render, UI). We will implement a struct EngineState that explicitly aligns with the offsets discovered in the reverse engineering analysis.

This struct acts as the shared context for the entire application. It must contain:

- **Texture Management:** Offsets for textureID_Main (0x004) and textureID_Spectrogram (0x008). The Spectrogram texture is particularly critical as it serves as the data bridge between the DSP thread and the Rendering thread.
- **DSP Configuration:** Variables for fftSize (0x00C), sampleRate (0x010), and audioBufferSize (0x014).
- **Visual State:** Variables for viewScrollX (0x080) which drives the waterfall animation, viewHeight (0x084), and strobePhase (0x088) which drives the strobe rotation.
- **Analysis Results:** The rendering thread reads analysis results directly from this struct, specifically currentHz (0x004) and tuningError (0xB08).
- **Synchronization:** A "Dirty Flag" at offset 0xB00 is used to signal the rendering thread that new analysis data is available. This lock-free synchronization mechanism is vital for decoupling the frame rate from the analysis rate.

In C++20, we will replace the raw void\* pointers found in the original struct with smart pointers (std::unique*ptr) and std::vector for dynamic buffers, but the \_logical* flow must remain a "pull" system where the rendering thread queries this state struct, completely decoupled from the audio processing thread.

### **2.2 The Circular Buffer (Data Ingress)**

The ingress of audio data is the heartbeat of the system. The original implementation utilizes a robust "Impedance Matcher" strategy to handle the irregular arrival of audio packets from the Android OS. The function PitchLabNative.processStreamedSamples feeds a circular buffer located at DAT_00062ccc.

**The Implementation Logic:**

The buffer logic is specifically designed to avoid memory allocation during runtime, which would trigger garbage collection or heap contention.

1. **Buffer Allocation:** A fixed-size buffer (e.g., 16,384 samples) is allocated at startup.
2. **Pointer Management:** The system maintains a writeHead index (DAT_00062cd8).
3. **The Split-Write Strategy:** When new data arrives, the code calculates the remaining space at the end of the linear buffer.
   - _Case A (Fits):_ A single memcpy copies the data to buffer \+ writeHead.
   - _Case B (Overflow):_ The data is split. The first chunk fills the buffer to the end. The remaining chunk wraps around and is written to buffer.
   - _Pointer Update:_ The writeHead is updated using conditional subtraction (if (head \>= size) head \-= size) rather than the slower modulo operator %, a specific optimization for ARM processors that we will preserve.1

**JUCE 8 Adaptation:**

In the JUCE framework, audio arrives via processBlock(AudioBuffer\<float\>& buffer).

- **Format Conversion:** The original DSP operates strictly on 16-bit signed integers. To maintain fidelity, we must convert the incoming floating-point audio (range \-1.0 to 1.0) to int16*t (range \-32768 to 32767\) \_immediately* at the ingress point.
- **Scaling:** Multiply samples by 32767.0f and cast to short. This quantization step introduces the specific noise floor characteristics of the original app, which affects how the AGC subsequently behaves on "silence".

### **2.3 Static Data Table Generation (The "Static DNA")**

PitchLab Pro relies heavily on pre-calculated lookup tables (LUTs) to avoid expensive mathematical operations (like log10, sin, exp) during the realtime loop. These tables constitute the "Static DNA" of the application and must be generated during the initialization sequence (FUN_0002f6b8).

**Table 1: The Decibel Brightness Curve**

- **Source:** FUN_0003f63c / DAT_000b2d64.
- **Function:** Maps linear integer amplitude (0–32767) to visual brightness (0–255).
- **Algorithm:** The generator loop calculates 20 \* log10(amplitude / 32767.0) and maps the resulting decibel range (e.g., \-100dB to 0dB) to an 8-bit alpha value. This ensures the visualization matches the logarithmic response of the human ear. A linear mapping would result in a visualizer that looks "black" for most musical signals.

**Table 2: The Spectral Color Palette**

- **Source:** 0x00056d00 inside libPitchLab.so.
- **Function:** Defines the visual identity of the app.
- **Implementation:** We must hardcode the 12 extracted RGB triplets. Using a generic color picker would fail to match the specific "Neon" aesthetic.
  - C: Lime Green (0xB2FF66)
  - E: Sky Blue (0x66B3FF)
  - A: Red (0xFF6666)
  - (See full table in S_B5).

**Table 3: The Strobe Gradient**

- **Source:** Generated procedurally in FUN_0002f6b8 (Lines 173-260).
- **Function:** This texture is used for the Strobe Tuner (Mode 6, 9). It is not a static image file but a mathematically generated waveform.
- **Algorithm:** The code generates a "Sawtooth-Squared" wave.
  - Iterate i from 0 to 4096 (Texture Width).
  - x \= i / 4096.0.
  - val \= ((x \+ 0.5) \* SCALAR \- 1.0).
  - output \= val \* val.
  - This squared function creates a sharp-edged gradient that simulates the physical cutouts of a mechanical strobe disc, providing the visual illusion of 3D depth when it rotates.

**Table 4: Window Functions (Q24 Fixed Point)**

- **Source:** FUN_00019470 (Gaussian) and FUN_000265dc (Hanning).
- **Function:** Preparing audio buffers for FFT.
- **Implementation:** These functions generate float curves (exp and sin/cos) but immediately convert them to **Q24 Fixed Point Integers**. The multiplier is 0x4b800000 (2^23 or 2^24 depending on sign convention). We must generate these tables at startup and store them in the Engine Struct at offsets 0xB44 and 0xB50.1

## ---

**3\. Phase 2: The DSP Core (Audio Pipeline)**

**Objective:** Implement the signal processing chain defined in FUN_0002a2e0, strictly adhering to the destructive integer math and Q24 fixed-point precision. This is the "Physics Engine" of the application.

### **3.1 Signal Conditioning: The Bitwise AGC**

**Function ID:** FUN*0003c4b0 **Strategic Importance:** This function is the primary reason PitchLab feels "more sensitive" than other tuners. It implements a destructive **Auto-Gain Control (AGC)** / Normalizer that runs \_before* the FFT.1

**The Algorithm:**

1. **Peak Detection (Branchless):** The function iterates through the analysis window (e.g., 4096 samples) to find the maximum absolute amplitude (A_max). Critically, it uses optimized bitwise math to calculate absolute values without conditional branching (if (x\<0)), which would stall the ARMv7 pipeline.
   - mask \= val \>\> 31 (All 1s if negative, 0 if positive).
   - abs \= (val ^ mask) \- mask.
   - This logic effectively performs Two's Complement negation for negative numbers and identity for positive numbers in constant CPU cycles.1
2. **Scale Factor Calculation:** It calculates a multiplier scalar S:
   ```
   S = 32767 / A_max
   ```
   This calculates how much headroom is available in the 16-bit integer range.
3. **Destructive Boost:** It iterates through the window again, multiplying _every_ sample by S.
   - **Result:** The quietest whisper is amplified until its peak hits 32767\. The loudest scream is attenuated (or untouched) so its peak hits 32767\.
   - **Implication:** The FFT always receives a fully saturated signal. This maximizes the dynamic range usage of the subsequent fixed-point FFT math, effectively eliminating quantization noise for low-level signals. Re-implementing this using standard floating-point normalization (0.0 to 1.0) would alter the noise floor characteristics and must be avoided to maintain fidelity.

### **3.2 Windowing and Q24 Fixed-Point Math**

**Function ID:** FUN_0002a2e0 (Lines 108–330)

The engine supports two mathematical modes for the transform, selectable by the user but architecturally distinct.

**Window Selection:**

- **Hanning Mode (Pitch Focus):** Generated via FUN_000265dc. Uses sin/cos math to create a Bell curve. This reduces spectral leakage, making the "Needle" tuner more stable and precise.
- **Gaussian Mode (Time Focus):** Generated via FUN_00019470. Uses exp(-x^2) math. This reduces time-smearing, making the "Waterfall" spectrogram clearer for fast passages (notes changing quickly).

**The Fixed-Point Optimization:**

The window coefficients are not applied as floats. They are fetched from the Q24 LUTs generated in Phase 1\.

- **Operation:** output_sample \= (input_sample \* window_coeff_Q24) \>\> 24\.
- **Fidelity Note:** This bit-shift operation truncates the lower bits of the result differently than a floating-point multiplication would round them. While subtle, this accumulation of rounding errors constitutes the "digital fingerprint" of the engine. To achieve a "Null Test" match with the original, we must use int64_t accumulators for the multiplication and then right-shift, mimicking the ARM SMULL instruction behavior.1

### **3.3 The FFT Implementation**

The original binary utilizes a highly optimized internal FFT routine, likely a variant of **KissFFT** (Keep It Simple Stupid FFT), which is popular in embedded audio for its integer-friendly structure.

**Implementation Strategy:**

- **Decision:** While JUCE provides juce::dsp::FFT (which wraps heavy implementations like FFTW or vDSP), sticking to a lightweight C-based FFT like KissFFT is recommended for bit-exact reproduction.
- **Data Type:** Ensure the FFT implementation is configured for int16_t or int32_t input/output. If using a float-based FFT for convenience, one must carefully manage the scaling factors to match the integer dynamic range established by the AGC.
- **Size:** The FFT size is dynamic, stored at offset 0x00C in the engine struct. Common values are 4096 (Standard) and 8192 (High Precision).

### **3.4 Sub-Bin Chromatic Folding (The "32-Slice" Analyzer)**

**Function ID:** FUN_0002a2e0 (Lines 333–End)

This is the "crown jewel" algorithm of PitchLab Pro. It transforms the linear frequency data (0Hz -> 22kHz) into the "Folded" musical domain.

**The "32-Slice" Architecture:**

The analyzer does not simply output 12 notes. It subdivides the chromatic scale into high-resolution micro-bins.

- **Math:** 12 Note x 32 Slices = 384 Total Bins.
- **Resolution:** Each slice represents ~3.125 cents. This resolution allows the spectrogram to show smooth "drifting" of pitch (vibrato) rather than jumpy note changes.

**The Folding Algorithm:**

1. **Preparation:** The engine uses the **Chroma Map** (local_5c), a LUT containing the start FFT bin index for each note.
2. **Harmonic Summation:** The engine iterates through the 384 target slices. For each slice (e.g., "Note A, Slice 10"), it calculates the fundamental frequency f.
   - It sums the magnitude energy from the FFT at f.
   - It adds energy from 2f (2nd Harmonic / \+1 Octave).
   - It adds energy from 4f (3rd Harmonic / \+2 Octaves).
   - This summation continues up to the Nyquist limit.
   - **Result:** Energy from A2, A3, A4, and A5 is all summed into the single "A" bin. This allows the tuner to detect the "Note Class" robustly, even if the fundamental frequency is missing (a common phenomenon in bass guitars or pianos).
3. **Octave Tracking:** While summing, the loop tracks _which_ harmonic contributed the max energy. This index (dominant_octave) is stored in a parallel buffer (OctaveBuffer at Offset 0x8a0). This metadata is crucial for the UI to display text labels like "A2" or "A4" on top of the folded visualization.

## ---

**4\. Phase 3: The Visualization Pipeline (Rendering Architecture)**

**Objective:** Establish a high-performance rendering context within the JUCE 8 framework that bridges the gap between the legacy OpenGL ES 1.1 logic and modern driver requirements.

### **4.1 The OpenGL Context Strategy (JUCE 8 Challenges)**

PitchLab Pro's rendering engine is a "Passive Consumer" that visualizes the state of Texture Memory. It relies on immediate mode OpenGL commands (glVertexPointer, glEnableClientState) which are deprecated in modern Core Profiles. Furthermore, JUCE 8 introduced a new Direct2D rendering backend on Windows which has known compatibility issues (black screens, transparency artifacts) when overlaid with OpenGL contexts.

**Configuration Strategy:**

1. **Context Initialization:** Initialize a juce::OpenGLContext.
2. **Profile Selection:** Explicitly call setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2). While the original used ES 1.1, requesting a 3.2 Core Profile ensures compatibility with modern macOS/Windows drivers. However, this necessitates writing a "Shim" to emulate the legacy functions.
3. **Windows Workaround:** To mitigate the Direct2D conflict, we must either disable the D2D renderer for the peer component or attach the OpenGL context to a heavyweight top-level window rather than a lightweight child component.

### **4.2 The "Immediate Mode" Batcher Shim**

To preserve the complex rendering logic of FUN_0003162c without rewriting it entirely for shaders, we will implement a **Batcher Class** that emulates the legacy API.

- **Class Design:** LegacyGLBatcher.
- **Data Structure:** std::vector\<Vertex\> vertexBuffer;
- **API:** addQuad(x, y, w, h, u, v, color)
  - This function effectively replaces the immediate mode glVertexPointer / glDrawArrays loops found in the original binary.
- **Flush Mechanism:** Inside the renderOpenGL() callback, the batcher compiles the accumulated vertices into a modern **Vertex Buffer Object (VBO)** and issues a single draw call.
- **Shader Emulation:** We need a simple "Passthrough" shader to emulate the fixed-function pipeline. It should accept Attribute Position, Attribute UV, and Attribute Color, and perform gl_FragColor \= texture(u_Tex, UV) \* v_Color.1

### **4.3 Texture Management and "The Film Reel"**

**Function ID:** FUN_00027168

Texture management is central to the "Waterfall" visualization. The engine treats **Texture ID 8** as a circular buffer or "Film Reel" of history.

**The "Dirty Row" Optimization:**

A naive implementation would re-upload the entire 1024x384 texture every frame, consuming massive bandwidth. PitchLab uses a "Dirty Row" strategy.

- **Mechanism:**
  1. The DSP engine calculates one new row of 384 pixels (the Chromatic Folding result).
  2. It calls glTexSubImage2D to upload _only_ this single row (height=1) to the GPU at y \= writeHead.
  3. **Scrolling:** To visualize the waterfall scrolling, the engine does _not_ move pixels in memory. Instead, it manipulates the Texture Coordinates (UVs).
  4. **Math:** V_start. The GPU's GL_REPEAT texture wrap mode handles the visual looping seamlessly. This optimization allows for infinite scrolling with zero CPU overhead.1

## ---

**5\. Phase 4: Visualization Mode Implementation**

**Objective:** Port the specific geometry generation logic for each View Mode from the View Dispatcher (FUN_0003162c), utilizing our Batcher Shim.

### **5.1 Mode 3: The Pitch Spectrogram (The Waterfall)**

This is the default view. It visualizes the 384-element folded array stored in Texture ID 8\.

**Geometric Logic:**

The renderer "slices" the single linear texture row into 12 distinct strips.

1. **Loop:** Iterate 12 times (Notes C to B).
2. **Strip Generation:** For each note, generate a Quad.
   - **Position:** Stack them vertically. y \= NoteIndex \* StripHeight.
   - **UV Mapping:** This is where the "Folding" happens.
     - Strip C maps to Texture U = 0.0 -> 0.083.
     - Strip C\# maps to Texture U = 0.083 -> 0.166.
     - ...
     - Strip B maps to Texture U = 0.916 -> 1.0.
3. **Result:** The linear texture data \`\` is visually stacked, allowing the user to see harmonic relationships vertically.

### **5.2 Mode 6: The Polyphonic Tuner**

This mode is computationally dense. It effectively runs 6 separate strobe tuners simultaneously.

**The Logic:**

1. **Activity Gate (FUN_00021894):** Before drawing, the engine checks the energy level of each string. If a string is not vibrating, its drawing loop is skipped entirely to save GPU cycles.
2. **Strobe Math:**
   - Iterate through 6 strings (E, A, D, G, B, E).
   - For each string, iterate 3 harmonics (Fundamental, 2nd, 3rd).
   - **Phase Accumulation:** Phase \+= FrequencyError.
   - **Texture Shift:** Map the Phase to the V-coordinate of the **Strobe Gradient Texture** (generated in Phase 1). v \= Phase \* 3.66e-5. This makes the texture appear to spin.
3. **Alpha Blending:** The alpha of each harmonic band is modulated by its signal strength. This allows the user to see the "Timbre" of the instrument (e.g., if the 3rd harmonic is brighter, the top band glows more).

### **5.3 Mode 2: The Instrument Tuner (The Needle)**

**Function ID:** FUN_00026ba4 (Line Drawer).

This mode renders vector geometry, not textures.

**Needle Physics:**

- **Data Source:** Read tuningError (Offset 0xB08) from the Engine Struct.
- **Angle Calculation:** $\theta = \text{Cents} \times 0.020944$. This constant scales the $\pm 50$ cent range to a roughly 60-degree arc.
- **Visual Polish:**
  - **Motion Blur:** The engine draws the needle multiple times using a history buffer (Offset 0x104), with older entries drawn at lower alpha. This simulates the persistence of a mechanical needle or CRT.
  - **Manual Anti-Aliasing:** The line drawing routine generates 3 passes for every line:
    1. **Shadow:** Black, slightly offset.
    2. **Core:** The draw color, opaque.
    3. **Glow:** The draw color, wider, semi-transparent. This manual AA gives the needle its distinct "LCD" look.1

### **5.4 Mode 9: The Radial Strobe**

**Logic:** FUN_0003162c (Lines 1020-1180).

This mode requires polar coordinate mapping, which is done on the CPU since ES 1.1 lacks complex fragment shaders.

**Mesh Generation:**

1. **Triangle Strip:** The loop iterates \~128 times to build a ring.
2. **Vertex Calculation:** $x = R \cdot \cos(\theta)$, $y = R \cdot \sin(\theta)$.
3. **Texture Trick:** The critical logic is mapping the Texture U-coordinate to angle \+ phase.
   - As the phase accumulates (due to frequency mismatch), the texture coordinates slide along the ring.
   - Since the Strobe Gradient is a repeating pattern, this creates the illusion of the ring spinning.

### **5.5 Mode 4: The Chord Matrix**

**Function ID:** FUN_0003d83c (Chord Engine).

This mode visualizes probability.

**Rendering Logic:**

- **Grid:** Draw a $12 \times 7$ grid (12 Roots $\times$ 7 Qualities).
- **Data Source:** Read the "Winner" buffer (Offset 0xB00).
- **Logic:**
  - If Probability\[Chord\] \> Threshold: Draw cell with Alpha \= 1.0.
  - Else: Draw cell with Alpha \= 0.2 (Dim).
- **Optimization:** The renderer iterates _backwards_ through the buffer (ptr--) to align with the bottom-up visual layout (C at bottom, B at top).1

## ---

**6\. Phase 5: Critical Optimizations and Modernization**

**Objective:** Adapt the specific micro-optimizations found in the ARMv7 binary for the x64/ARM64 architecture to ensure performance fidelity.

### **6.1 Bitwise Logic & Integer Math**

PitchLab's code is riddled with bitwise hacks.

- **Bitwise ABS:** The AGC uses (x ^ (x \>\> 31)) \- (x \>\> 31). While modern compilers optimize std::abs efficiently, explicitly using this bitwise logic ensures that the handling of the edge case \-32768 (which cannot be negated in 16-bit signed arithmetic) matches the original behavior exactly.
- **Integer Color Blending:** The texture formatter (FUN_00027168) uses val \= (Energy \* Color \* 320\) \>\> 16 to calculate pixel brightness. We must preserve this integer math rather than moving to float (Energy \* Color / 255.0) to avoid conversion overhead and rounding differences.

### **6.2 Stack-Based Allocation**

The original binary avoids heap allocation (malloc/new) inside the render loop to prevent jitter. It uses large stack arrays (local\_) for temporary vertex data.1

- **C++20 Strategy:** We will use std::pmr::monotonic_buffer_resource (Polymorphic Memory Resource) allocated on the stack. This allows us to use convenient containers like std::pmr::vector while keeping the underlying storage on the stack, mimicking the original "zero-allocation" behavior.

### **6.3 The "Anti-Chord" Penalty Logic**

**Function ID:** FUN_0003d83c

The Chord Detection engine uses a specific probabilistic filter.

- **Negative Penalty:** When scoring a chord candidate, the engine multiplies the score not just by the energy of present notes, but by the _inverse_ energy of forbidden notes.
  - Score \*= \~Energy(ForbiddenNote) (Bitwise NOT).
  - **Significance:** This logic is what allows the app to distinguish stable "C Major" from "C Major 7". If the note B is present (high energy), \~Energy(B) becomes small, crushing the score for the simple C Major candidate. This boolean-logic-in-arithmetic must be preserved.1

## ---

**7\. Phase 6: Build System and Deployment**

**Objective:** Configure the build environment to handle the cross-platform dependencies.

### **7.1 CMake Configuration**

A robust CMakeLists.txt is required to manage the JUCE dependencies and platform-specific flags.

\`\`\`CMake

cmake_minimum_required(VERSION 3.22)  
project(PitchLabPro VERSION 2.0.0)

\# Add JUCE as a subdirectory (using CPM or submodule)  
add_subdirectory(JUCE)

juce_add_plugin(PitchLabPro  
 COMPANY_NAME "Symbolic"  
 PLUGIN_CODE 'PLaB'  
 FORMATS Standalone VST3 AU  
 \#...  
)

target_compile_features(PitchLabPro PRIVATE cxx_std_20)

\# Platform Specifics  
if(APPLE)  
 \# Silence deprecation warnings for OpenGL on macOS  
 target_compile_definitions(PitchLabPro PRIVATE GL_SILENCE_DEPRECATION=1)  
 target_link_libraries(PitchLabPro PRIVATE "-framework OpenGL")  
elseif(WIN32)  
 \# Link against standard OpenGL library  
 target_link_libraries(PitchLabPro PRIVATE opengl32)  
endif()  
\`\`\`

### **7.2 Windows Specifics (Direct2D Conflict)**

As noted in the research 3, JUCE 8's Direct2D renderer can conflict with OpenGL attachTo calls.

- **Implementation Note:** In the Main.cpp or PluginEditor.cpp, we must detect the rendering engine. If artifacts appear, we may need to force the top-level peer to use a software renderer, effectively delegating all hardware acceleration duties solely to our manual OpenGL context.

## ---

**8\. Conclusion and Verification**

This roadmap defines a rigorous path to resurrecting PitchLab Pro. The project is defined by its adherence to the specific integer-arithmetic "Physics" of the original engine (FUN*0003c4b0, FUN_0002a2e0). By wrapping this immutable core in a modern C++20/JUCE 8 shell and utilizing a custom OpenGL Batcher Shim to bridge the API gap, we ensure that the re-engineered application will not only run on modern systems but will \_feel* identical to the original.

**Verification Metric (The Null Test):**

To validate the implementation, we will perform a "Null Test."

1. Run the original APK in an Android Emulator.
2. Run the new Desktop Build.
3. Feed a deterministic 440Hz Sine Wave into both.
4. **Success Condition:** The "Needle" in Mode 2 must show exactly 0 cents deviation in both. The Spectrogram in Mode 3 must align the peak to the exact same pixel offset within the "A" strip. Any deviation indicates a failure in the fixed-point math reconstruction.
