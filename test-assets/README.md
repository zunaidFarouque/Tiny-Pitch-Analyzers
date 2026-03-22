# Test assets

Place **small, committable** audio and reference files used by automated tests here.

## Conventions (suggested)

- **WAV:** short clips (e.g. 440 Hz sine, a few hundred ms), documented sample rate and channel count in the test or a sibling `.md` file.
- **Golden images:** if used for waterfall/chromagram regression, name by scenario (e.g. `sine440_chromagram_512.jpg`) and note generator parameters.

## Large files

Do not commit full songs or huge wave files. Keep them local and add ignore rules if needed, or download from a known URL in CI with caching.

See [docs/TESTING.md](../docs/TESTING.md) for how tests consume these files.
