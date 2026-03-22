This is **Section 5 of the Report: Critical Data Tables**.

This section documents the static data extracted from the binary. Unlike code logic, these are the "Magic Numbers" and "Assets" that define the specific look, feel, and musical intelligence of the application. Re-implementing the algorithms without these exact tables would result in an app that works but "feels wrong."

---

# Section 5: Critical Data Tables (The "Static" DNA)

## 5.1 The Spectral Color Palette (LUT)

**Source Address:** `0x00056d00` inside `libPitchLab.so`.
**Usage:** Used by Spectrogram, Chord Matrix, Polyphonic Tuner, and Radial Strobe to color-code notes.

The app uses a 12-entry Look-Up Table (LUT) defining the RGB values for the Chromatic Scale starting at C.

| Index  | Note   | Hex Value (R G B) | Color Description         |
| :----- | :----- | :---------------- | :------------------------ |
| **0**  | **C**  | `B2 FF 66`        | **Lime Green**            |
| **1**  | **C#** | `66 FF 66`        | **Pure Green**            |
| **2**  | **D**  | `66 FF B2`        | **Spring Green**          |
| **3**  | **D#** | `66 FF FF`        | **Cyan**                  |
| **4**  | **E**  | `66 B3 FF`        | **Sky Blue**              |
| **5**  | **F**  | `66 66 FF`        | **Periwinkle Blue**       |
| **6**  | **F#** | `B2 66 FF`        | **Violet**                |
| **7**  | **G**  | `FF 66 FF`        | **Magenta**               |
| **8**  | **G#** | `FF 66 B2`        | **Hot Pink**              |
| **9**  | **A**  | `FF 66 66`        | **Red** (Reference Pitch) |
| **10** | **A#** | `FF B2 66`        | **Orange**                |
| **11** | **B**  | `FF FF 66`        | **Yellow**                |

- **Implementation Note:** The Alpha channel is **not** stored here. It is calculated dynamically based on signal strength (see Section 4.5.13).

## 5.2 The Polyphonic Interval Ratios

**Source Function:** `FUN_0003f63c` (Lines 1100+).
**Usage:** Used by Mode 6 (Polyphonic) and Mode 4 (Chord) to identify harmonic relationships between strings without using expensive division.

These constants represent the reciprocal ratios of standard string intervals in **Fixed-Point Format**.

| Hex Constant | Decimal Approx | Ratio | Musical Interval                                 |
| :----------- | :------------- | :---- | :----------------------------------------------- |
| `0x92492492` | 0.5714         | 4/7   | Related to **Fourth** (4:3 inverted/scaled)      |
| `0xB6DB6DB6` | 0.7142         | 5/7   | Related to **Major Third** (5:4 inverted/scaled) |
| `0xDB6DB6DB` | 0.8571         | 6/7   | Harmonic Scalar                                  |
| `0x3FE24924` | 0.249...       | 1/4   | 2 Octaves Down                                   |

- **Optimization Secret:** These are likely coefficients for a **Comb Filter** or harmonic product spectrum calculation used to separate the mixed audio signal into "String Candidates."

## 5.3 The Decibel (dB) Brightness Curve

**Source:** Generated at runtime by `FUN_0003f63c`.
**Storage:** `DAT_000b2d64` (Pointer to 32KB buffer).
**Usage:** Mapping linear FFT magnitude to screen brightness/alpha.

To avoid calculating `log10(x)` for every pixel (which would be too slow), the app generates this table on startup.

**The Generation Formula:**
For every integer Amplitude `i` from `0` to `32767`:
$$ \text{Table}[i] = \text{Clamp}_{0..255} \left( K_1 \cdot \log_{10}(i \cdot K_2) + \text{Offset} \right) $$

- **Logic:** It maps the 16-bit integer range to a standard Decibel scale, then maps that dB scale to 8-bit brightness (0-255).
- **Result:** This ensures that a signal at 50% amplitude looks only slightly dimmer than 100%, whereas a linear map would make it look half as bright (which appears too dark to the human eye).

## 5.4 The Strobe Gradient Formula

**Source:** Generated at runtime by `FUN_0002f6b8`.
**Storage:** Texture ID (uploaded to GPU).
**Usage:** The "Spinning Wheel" texture for Mode 5, 6, and 9.

PitchLab does not load a PNG for the strobe. It calculates a mathematically perfect gradient to ensure smooth rotation at any speed.

**The Math (Pseudo-code):**

```python
# Width = 4096 (High Resolution)
Buffer = new float[4096]
for i in range(0, 4096):
    x = i / 4096.0  # Normalized 0.0 to 1.0

    # The "Sawtooth Squared" Wave
    # Creates a sharp edge that fades smoothly
    val = ((x + 0.5) * SCALAR - 1.0)
    val = val * val  # Square it

    Buffer[i] = val
```

- **Visual Result:** A pattern of black-to-white gradients that repeats. When the texture coordinate shifts (Phase accumulation), the squared curve creates the illusion of a 3D metallic cylinder rotating.

## 5.5 Fixed-Point DSP Coefficients (Q24)

**Source:** `FUN_00025550`.
**Usage:** Fast multiplication in the Audio Engine.

The app hardcodes the conversion factor for all float-to-int math in the signal chain.

- **Scalar:** `0x4b800000`
- **Value:** $16,777,216.0$ ($2^{24}$)
- **Interpretation:**
  - `1.0` (Float) $\rightarrow$ `16,777,216` (Int).
  - `0.5` (Float) $\rightarrow$ `8,388,608` (Int).
- **Significance:** When re-implementing on modern CPUs (ARM64/x64), this table is **obsolete**. You should use standard `float` arrays and NEON/AVX instructions instead. This table is a specific artifact of 2012-era ARMv7 optimization.
