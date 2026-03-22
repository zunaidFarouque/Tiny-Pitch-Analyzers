This is the **Implementation Strategy**.

We are transitioning from **Archaeology** (Reverse Engineering) to **Architecture** (Forward Engineering).

You have two paths. Both require preserving the _Logic_ (Sections 3 & 4 of our report) while replacing the _Infrastructure_ (Section 2 - The JNI/Android glue).

Here is the battle plan for both scenarios.

---

# Plan A: The Web Replica (WASM + WebGL)

**Objective:** Run the engine in a browser with 60 FPS performance and native-like latency.
**Constraint:** JavaScript Garbage Collection is the enemy. We must strictly use **WebAssembly (WASM)** for the Core.

### Phase 1: The "Black Box" Port (C++ to WASM)

Do not rewrite the logic in JavaScript. Port the C++ code directly.

1.  **Toolchain:** Use **Emscripten**.
2.  **Memory Model:**
    - Create a single `WASM Heap` (Linear Memory).
    - Recreate the **Main Engine Struct** (Section 2.4) inside this heap.
    - **Optimization:** The "Stack-Based" allocations we analyzed (`local_buffers`) map perfectly to WASM linear memory performance characteristics.
3.  **The Audio Bridge:**
    - **Source:** **AudioWorklet** (Essential). The main JS thread is too jittery.
    - **Implementation:**
      - The `AudioWorkletProcessor` receives audio (Float32).
      - **Crucial Step:** Convert Float32 $\rightarrow$ Int16 (to match the original DSP behavior exactly) or adapt `FUN_0003c4b0` (AGC) to handle Floats.
      - Pass pointers directly to the WASM heap.
      - Call the exported C++ function `processAudio()`.

### Phase 2: The Rendering Adapter (WebGL 1.0)

PitchLab used OpenGL ES 1.1. WebGL is based on ES 2.0. The translation is straightforward but requires a "Batcher" shim.

1.  **The Shader Shim:**
    - Write a generic Vertex/Fragment shader that mimics the "Fixed Function Pipeline."
    - Uniforms: `u_Texture` (Sampler), `u_Color` (Vec4), `u_Matrix` (Mat4).
    - This allows you to keep the C++ logic (`FUN_0003162c`) almost untouched.
2.  **Texture Streaming:**
    - Use `gl.texSubImage2D` to upload the single row of spectrogram data from WASM memory.
    - _Warning:_ Browsers check bounds on every upload. Ensure the WASM `Uint8Array` view is strictly typed to avoid overhead.

### Phase 3: The Threading Model

- **Worker A (Audio):** Runs the DSP code (FFT, Folding). Writes to `SharedArrayBuffer`.
- **Worker B (Main):** Runs the Rendering code. Reads from `SharedArrayBuffer`.
- **Synchronization:** Use `Atomics.store` / `Atomics.load` to replicate the "Dirty Flag" synchronization we found in Section 3.5.18.

---

# Plan B: The Desktop Replica (JUCE / C++)

**Objective:** A high-performance standalone app for Windows.
**Advantage:** JUCE handles the hardware abstraction better than Android JNI.

### Phase 1: Architecture Migration

We will replace the Android "Singleton" with a standard JUCE `AudioProcessor`.

1.  **Math Modernization (The Big Decision):**
    - _Option A (Purist):_ Keep the Q24 Fixed-Point math (`0x4b800000`). It works fine on x64 but is unnecessary.
    - _Option B (Modern):_ Replace `FUN_00025550` (Fixed Point Converter) with standard `float`.
    - **Recommendation:** Go with **Option B**. Modern x64 CPUs handle AVX floats faster than bitwise integer math. Update the AGC and Windowing tables to standard `float`.
2.  **FFT Replacement:**
    - Delete the internal FFT routine found in the binary.
    - Use `juce::dsp::FFT`. It is heavily optimized for x64 (AVX2/AVX-512).
    - _Mapping:_ Ensure the FFT size matches the Config (4096/8192).

### Phase 2: The Graphics Pipeline (OpenGL Context)

Do **not** use `juce::Component::paint()` for the visualizers. It uses a software rasterizer which is too slow for the "Waterfall."

1.  **Setup:** Attach a `juce::OpenGLContext` to your main component.
2.  **The Renderer Class:**
    - Implement `OpenGLRenderer`.
    - Inside `renderOpenGL()`, reimplement the **Batcher** (`FUN_00024a84`).
    - We need a dynamic `std::vector<Vertex>` that clears every frame.
3.  **Texture Management:**
    - Create a raw `GLuint` texture.
    - Reimplement `FUN_00027168` (The Texture Formatter) inside `renderOpenGL`.
    - Use `glTexSubImage2D` to push the new row.

### Phase 3: Data Logic (The "Exact" Logic)

Copy these algorithms verbatim from our report. Do not use JUCE equivalents, or it won't "feel" like PitchLab.

1.  **The Folding Logic:** Copy the loop from **Section 3.4**. The 32-slice folding is the app's signature.
2.  **The Chord Engine:** Copy the bitwise mask logic from **Section 3.6**.
3.  **The Colors:** Hardcode the **Section 5.1** Color Table. Do not use a color picker; use the hex values.

---

# Summary of Differences

| Feature       | Plan A (WASM/Web)                    | Plan B (JUCE/Win64)             |
| :------------ | :----------------------------------- | :------------------------------ |
| **Language**  | C++ (via Emscripten)                 | C++ (Native)                    |
| **Math**      | Keep Fixed-Point (Native behavior)   | Upgrade to Float (AVX Speed)    |
| **Threading** | `AudioWorklet` + `SharedArrayBuffer` | `processBlock` + `AbstractFifo` |
| **Rendering** | WebGL 1.0 (Canvas)                   | OpenGL 3.0+ (Core Profile)      |
| **Assets**    | Embed Hex Tables in JS/WASM          | Embed Hex Tables in C++ Header  |
| **Input**     | `navigator.getUserMedia`             | `juce::AudioDeviceManager`      |

### Recommendation

**Start with Plan B (JUCE).**

1.  Debugging C++ in Visual Studio is infinitely easier than debugging WASM in Chrome DevTools.
2.  Once the C++ logic is verified and working on Windows, **porting it to WASM is trivial** because JUCE has partial WASM support, or you can just copy the DSP/Render logic files into an Emscripten project.

---

===

---

This is the **Optimized Implementation Strategy**.

You have made a crucial decision: **Fidelity over Modernization.** By choosing to keep the "Destructive" integer math and fixed-point logic, we simplify the porting process significantly. We don't need to reinvent the physics; we just need to provide the container.

Since you want both Web and Windows, the smartest play is a **Unified C++ Core**.

---

# The "Unified Core" Strategy (Write Once)

Do not write separate logic for JS and C++. We will extract the logic we analyzed into a standalone **C++ Library (`PitchLabCore`)** that compiles to both targets.

**The "PitchLabCore" Library must contain:**

1.  **The Structs:** The main Engine struct and the Circular Buffer.
2.  **The Tables:** The Hex dumps of Colors, Ratios, and Windows.
3.  **The Logic:** A direct copy-paste (with minor syntax cleanup) of `FUN_0002a2e0` (DSP), `FUN_0003c4b0` (AGC), and `FUN_0003162c` (Renderer).

---

# 1. Optimized Plan: WASM (Web Technologies)

**Goal:** 60 FPS on a mid-range phone browser.
**Bottleneck:** Memory copying between JS and WASM.

### Step A: The Audio Worklet (The Driver)

We cannot use the main thread (UI Jitter). We must use an `AudioWorklet`.

1.  **Input:** The browser gives `Float32` arrays (range -1.0 to 1.0).
2.  **The "Destructive" Adapter:**
    - Inside the Worklet (JS side), multiply by `32767.0`.
    - Cast to `Int16`.
    - Write directly into the **WASM Heap** (using `Module.HEAP16`).
    - _Optimization:_ Do **not** allocate new arrays. Write to a fixed memory address that the C++ core knows about.

### Step B: The C++ / WASM Core

Compile your `PitchLabCore` using **Emscripten**.

1.  **Keep Fixed Point:** Do not change the `0x4b800000` logic. WASM handles 32-bit integer math faster than floats. It ensures bit-perfect replication of the original app's sensitivity.
2.  **Memory Model:** Use ` -s TOTAL_MEMORY=32MB`. The original app used <3MB. This is plenty.
3.  **Exported Functions:**
    - `init()`
    - `process_audio(ptr, size)`
    - `get_texture_pointer()` (Returns the address of the 384-pixel buffer).

### Step C: The WebGL Renderer

Do not try to run OpenGL ES 1.1 code in WASM (it requires an emulation layer that is slow). Write the "Renderer" logic in **pure C++** but make it output **Vertex Arrays**, not GL calls.

1.  **The "Vertex Gen" Pattern:**
    - Run `FUN_0003162c` in WASM.
    - Instead of calling `glDrawArrays`, have it fill a `Float32Array` in the WASM Heap with coordinates: `[x, y, u, v, color, ...]`
2.  **The JS Consumer:**
    - Every frame, JS asks WASM: "How many vertices?"
    - JS creates a view into WASM memory: `new Float32Array(Module.HEAPF32.buffer, ptr, count)`.
    - JS calls `gl.bufferData` **once**.
    - JS calls `gl.texSubImage2D` to upload the Spectrogram row (also from WASM memory).
3.  **Optimization:** This minimizes the "Chatter" between JS and WASM. One big data transfer per frame.

---

# 2. Optimized Plan: JUCE (Windows x64)

**Goal:** Zero latency, <1% CPU usage.
**Bottleneck:** JUCE's default graphics stack (Software Rendering).

### Step A: Bypassing the JUCE Graph

We will use JUCE _only_ for the windowing shell and Audio Device Management. We will **bypass** the standard `paint()` method.

1.  **Inherit from `OpenGLRenderer`:** Attach this to your Main Component.
2.  **The Render Loop:**
    - Inside `renderOpenGL()`, call your ported C++ function `RenderView()`.
    - Since we are in C++, we **can** use raw OpenGL calls.
    - **Translation:** Map the original `glVertexPointer` calls to modern `glVertexAttribPointer`.
    - **Shader:** Write one "Passthrough" shader (Vertex \* Color) to emulate the fixed function pipeline.

### Step B: The "Hot Integer" Audio Chain

JUCE passes `AudioBuffer<float>`.

1.  **The Adapter:**
    ```cpp
    void processBlock(AudioBuffer<float>& buffer) {
        auto* channelData = buffer.getReadPointer(0);
        // SIMD Optimization (AVX2) to convert Float -> Int16
        // Or simple loop (compiler will auto-vectorize):
        for(int i=0; i<numSamples; ++i) {
            short sample = (short)(channelData[i] * 32767.0f);
            ringBuffer.write(sample); // Write to Ring Buffer
        }
    }
    ```
2.  **The DSP Thread:**
    - Do not run the FFT in `processBlock` (it blocks audio).
    - Run the `FUN_0002a2e0` (Core Logic) on a separate `std::thread` or `Timer` that runs at 60Hz. It pulls from the Ring Buffer.

### Step C: Visual Fidelity (The Pixel Perfect Rule)

1.  **Texture Upload:** Use `glTexSubImage2D` exactly as the original did. Do not try to use `juce::Image`.
2.  **Scissor Test:** Keep the `glScissor` logic. It is still the fastest way to handle split-screen on Desktop GPUs.
3.  **V-Sync:** Enable it. The original app's physics (needle damping) are tied to the frame rate. If you run at 500 FPS on Windows, the needle will move too fast. You must lock the logic to 60Hz.

---

# The "Destructive" Comparison

| Feature       | Original (Android ARM)  | Web (WASM) Implementation                         | Windows (JUCE) Implementation                     |
| :------------ | :---------------------- | :------------------------------------------------ | :------------------------------------------------ |
| **Input**     | `short[]` (JNI Pinning) | `Float32` $\rightarrow$ `Int16` (in AudioWorklet) | `Float32` $\rightarrow$ `Int16` (in processBlock) |
| **Gain**      | Bitwise Integer AGC     | **Keep Bitwise Integer AGC**                      | **Keep Bitwise Integer AGC**                      |
| **Windowing** | Fixed Point (Q24)       | **Keep Fixed Point**                              | **Keep Fixed Point**                              |
| **FFT**       | KissFFT (Likely)        | **Use KissFFT (C)**                               | **Use KissFFT (C)**                               |
| **Folding**   | 32-Sub-Bin Sum          | **Exact Copy**                                    | **Exact Copy**                                    |
| **Texture**   | 565 / RGBA              | RGBA (Webgl 1 is strict)                          | RGBA                                              |
| **Drawing**   | Client-Side Arrays      | Vertex Array $\rightarrow$ JS BufferData          | VAO / Client Arrays                               |

**Final Instruction:**
Do not "clean up" the math. If the original code calculates a probability using `~Energy` (bitwise NOT), do not change it to `Max - Energy`. Keep the weirdness. That is where the character of the application lives.
