## BackReverse v0.0.1 beta (Windows x64)

First public beta of BackReverse, the chunked-reverse audio manipulator by Circuit Drift Labs.

### Downloads
| File | What it is |
|---|---|
| `BackReverse-v0.0.1-beta-Windows-Setup.exe` | Installer: standalone app (Program Files), VST3 (`C:\Program Files\Common Files\VST3`) and CLAP (`C:\Program Files\Common Files\CLAP`). Pick components during setup. |
| `BackReverse-v0.0.1-beta-Windows-Portable.zip` | Portable standalone `BackReverse.exe`; unzip anywhere and run, no install. |
| `BackReverse-v0.0.1-beta-Windows-VST3.zip` | `BackReverse.vst3` bundle; copy the folder to `C:\Program Files\Common Files\VST3`. |
| `BackReverse-v0.0.1-beta-Windows-CLAP.zip` | `BackReverse.clap`; copy to `C:\Program Files\Common Files\CLAP`. |

### Highlights
- Sequential chunk reverse (reverses inside each chunk, keeps order), whole-source reverse, reordered chunks (11 order modes plus a user pattern language), forward chunks and Free Scrub.
- Per-chunk Rate and Time-Stretch at 1/4x, 1/2x, 1x, 2x, 3x or any ratio from 0.05x to 8x, with cycle/random/weighted/gate-follow/pattern-follow speed lanes.
- Visual gate sequencer (2 to 64 steps) with per-gate forward/reverse, nine shapes plus a custom curve, gap modes and per-gate FX.
- Stutter, Delay and Echo in a drag-to-reorder chain with Global, Alternating, Chunk Lane, Random and Gate Lane scopes.
- Pan/stereo patterns, polarity patterns and true all-pass phase rotation.
- Scrub and vinyl scratch on the waveform, with Latch, Spring and Continue release modes.
- Seeded, deterministic randomness: offline renders match realtime.

### New in this beta
- **A/B compare** (usability): two complete settings slots in the header.
- **Render to WAV and Record output** (value): offline bounce of the loaded file, plus live output recording.
- **Turntable platter** (fun): a spinning record on the Scratch tab that you can grab and scratch.
- **Tape Stop** (extra effect): seeded per-chunk tape-stop and spin-up ramps.

### Known limitations (beta)
- Only Windows x64 builds are published. The code is cross-platform, so macOS and Linux builds can come later.
- Live reverse is limited to a 16 s buffer (a 48 s ring). Settings that need more are clamped, and the header shows CLAMPED when that happens.
- Formant preservation and transient-sensitive stretching are not implemented yet.
