# Section 2: The Core Engine (Singleton Architecture)

## 2.1 Overview

The core logic resides in a monolithic C++ class (which we will define as `PitchLabEngine`) managed as a **Singleton**. The Java layer interacts with this object through opaque pointers, treating it as a black box.

- **Instance Accessor:** `FUN_0003a018`
- **Global Pointer:** `DAT_00062cc0` (Holds the address of the active engine instance).
- **Memory Allocation:** `0xB58` bytes (**2904 bytes**) per instance.
- **VTable Address:** `0x000582e0` (ARM32).

## 2.2 The Initialization Sequence

When the app launches, `PitchLabNative.initialiseNative()` calls the accessor. If `DAT_00062cc0` is null, the bootstrap sequence begins via `FUN_0003063c` -> `FUN_0002f6b8`.

### Step-by-Step Startup Logic:

1.  **Allocation:** A block of **2904 bytes** is allocated via `operator.new`.
2.  **VTable Injection:** The address `0x000582e0` is written to the first 4 bytes (Offset `0x00`). This binds the methods (Audio, Render, etc.) to the data.
3.  **State Reset:** Large blocks of the struct are explicitly zeroed out (`memset`).
    - Offsets `0x1D` to `0x29` (Flags).
    - Offsets `0x104` to `0x284` (Likely previous frame history buffers).
4.  **Subsystem Spin-up:**
    - **Texture Generation:** Calls `FUN_000227ec` to generate OpenGL texture handles.
    - **Helper Object Creation:** Allocates specific helper objects at Offsets `0xB3` (Window Generator) and `0xB9` (Frequency Tracker).
5.  **Strobe Pre-Calculation:** The constructor executes a complex floating-point loop (analyzed in `FUN_0002f6b8`, lines 173-260) to procedurally generate the "Sawtooth-Squared" gradient texture used for the Strobe Tuner visualization. This texture is uploaded to the GPU immediately during startup.

## 2.3 The VTable (Method Map)

The Virtual Function Table at `0x000582e0` defines the public interface of the Engine. This is how JNI triggers actions.

| Offset   | Address (Hex)  | Function           | Description                                           |
| :------- | :------------- | :----------------- | :---------------------------------------------------- |
| **0x00** | `0002df88`     | `Destructor`       | Cleanup and deallocation.                             |
| **0x04** | `0002edd0`     | `Reset`            | Clears buffers/history.                               |
| **0x08** | `00026f58`     | `SetViewport`      | Handles screen resize/rotation.                       |
| **0x0C** | **`0002a2e0`** | **`ProcessAudio`** | **The DSP Entry Point.** Takes raw PCM, runs FFT.     |
| **0x10** | **`000390fc`** | **`DrawFrame`**    | **The Render Entry Point.** Takes ViewMode, draws UI. |

## 2.4 The Object Memory Map (Struct Definition)

Based on the disassembly of `drawFrame`, `processAudio`, and the constructor, we have reconstructed the memory layout of the `PitchLabEngine` struct.

**Size:** 2904 Bytes (`0xB58`)

```cpp
struct PitchLabEngine {
    /* 0x000 */ void* vtable;              // Pointer to function table (0x582e0)
    /* 0x004 */ GLuint textureID_Main;      // Main UI Texture
    /* 0x008 */ GLuint textureID_Spectrogram; // The "Waterfall" Texture (Target of Rendering)
    /* 0x00C */ int  fftSize;               // Configured FFT Size (e.g., 4096)
    /* 0x010 */ float sampleRate;           // Current Audio Sample Rate
    /* 0x014 */ int  audioBufferSize;       // Size of circular buffer

    // ... gaps ...

    /* 0x080 */ float viewScrollX;          // Global scroll offset for animations
    /* 0x084 */ float viewHeight;           // Screen Height (in pixels)
    /* 0x088 */ float strobePhase;          // Current rotation angle of the strobe

    // ... gaps ...

    /* 0x2C4 */ void* configObject;         // Pointer to configuration settings
    /* 0x2D5 */ void* fontTexture;          // Pointer to FreeType/Font atlas

    // ... gaps ...

    /* 0xB00 */ bool isRendering;           // Flag: 1 if drawing, 0 if idle
    /* 0xB04 */ float currentHz;            // Detected fundamental frequency (for display)
    /* 0xB08 */ float tuningError;          // Deviation in cents (for Needle)
    /* 0xB0C */ int targetViewMode;         // The ViewMode to switch to (for transitions)

    /* 0xB14 */ float uiScaleFont;          // Calculated font size scalar
    /* 0xB18 */ float uiScaleLine;          // Calculated line width scalar
    /* 0xB1C */ float uiScaleGlobal;        // Master UI scale factor (based on width)

    // ... gaps ...

    /* 0xB28 */ void* jniEnv;               // Cached JNI Environment pointer
    /* 0xB44 */ void* gaussianWindow;       // Pointer to pre-calc Gaussian table
    /* 0xB50 */ void* hanningWindow;        // Pointer to pre-calc Hanning table
};
```

## 2.5 Critical Sub-Components

The Engine is not just code; it contains embedded "Physics Engines" initialized at `0x0002f6b8`.

1.  **The Window Generator (Offset `0xB3`):**

    - Points to the object using VTable `0x00058258`.
    - Responsible for generating the Hanning (`sin`/`cos`) and Gaussian (`exp`) windowing arrays.
    - These arrays are generated once on startup (or config change) and stored in `0xB44`/`0xB50` to avoid re-calculation per frame.

2.  **The dB Table Generator (Global `DAT_000b2d64`):**
    - Generated in `FUN_0003f63c`.
    - A lookup table of 32767 entries converting linear integer amplitude to logarithmic decibels.
    - This is "attached" to the engine indirectly via the rendering pipeline.

## 2.6 The JNI Bridge Mechanics

The connection between Java and this Core is uniquely optimized for low latency:

1.  **Shared Context:** The `PitchLabNative` Java class holds no state. It blindly passes the `processStreamedSamples` and `drawFrame` calls to the C++ Engine.
2.  **No Return Values:** The C++ Engine almost never returns data to Java.
    - Visuals are drawn directly to the OpenGL Surface.
    - Audio is consumed into internal buffers.
3.  **Synchronization:** The functions are marked `synchronized` in Java, but the C++ Engine implements its own internal circular buffering (`DAT_00062ccc`) to decouple the audio thread (high priority) from the video thread (vsync locked). This prevents audio dropouts even if the UI lags.
