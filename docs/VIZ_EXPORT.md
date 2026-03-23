# Visualizer Offline Export

`PitchLabVizExport` is a headless CPU tool that:

1. loads a WAV file,
2. seeks forward by `--seek-sec`,
3. runs one analysis window through `PitchLabEngine`,
4. renders visualizer mode images to PNG files.

This is meant for quick regression checks without launching the GUI/OpenGL host.

## Build

```bash
cmake --build build --config Release --target PitchLabVizExport
```

## Run

```bash
build/Release/PitchLabVizExport \
  --wav "Source/App/ExampleAudio/Monophonic Vox - Aj sraboner.wav" \
  --seek-sec 2.0 \
  --out "Tests/test-output/manual-run" \
  --modes all
```

## Arguments

- `--wav <path>`: input WAV path (required)
- `--seek-sec <float>`: seconds to skip before analysis (default `0`)
- `--out <dir>`: output folder (default `Tests/test-output/manual`)
- `--modes <csv|all>`: `waveform,waterfall,needle,stroberadial,chordmatrix`
- `--width <int>`: output width (default `1024`)
- `--height <int>`: output height (default `384`)
- `--verify <dir>`: optional golden directory with matching PNG names
- `--max-mean-abs-diff <float>`: per-channel mean absolute diff threshold for verify mode

## Output

The tool writes:

- `waveform.png`
- `waterfall.png`
- `needle.png`
- `stroberadial.png`
- `chordmatrix.png`
- `meta.json`

under the selected output directory.

## Automated smoke test

`Tests/test_viz_export_smoke.cpp` launches `PitchLabVizExport` and checks that PNG files and `meta.json` are created and non-empty.
