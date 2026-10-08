## BackReverse v0.0.2 beta (Windows x64 and Linux x86_64)

Beta release of BackReverse, the chunked-reverse audio manipulator by Circuit Drift Labs. This release adds Linux builds and fixes the audio, gate and interface issues found in the 2026-10-08 audit.

### Downloads
| File | What it is |
|---|---|
| `BackReverse-v0.0.2-beta-Windows-Setup.exe` | Installer: standalone app (Program Files), VST3 (`C:\Program Files\Common Files\VST3`) and CLAP (`C:\Program Files\Common Files\CLAP`). Pick components during setup. |
| `BackReverse-v0.0.2-beta-Windows-Portable.zip` | Portable standalone `BackReverse.exe`; unzip anywhere and run, no install. |
| `BackReverse-v0.0.2-beta-Windows-VST3.zip` | `BackReverse.vst3` bundle; copy the folder to `C:\Program Files\Common Files\VST3`. |
| `BackReverse-v0.0.2-beta-Windows-CLAP.zip` | `BackReverse.clap`; copy to `C:\Program Files\Common Files\CLAP`. |
| `BackReverse-v0.0.2-beta-Linux-x86_64-Standalone.tar.gz` | `BackReverse` standalone executable. Extract and run `./BackReverse`. |
| `BackReverse-v0.0.2-beta-Linux-x86_64-VST3.tar.gz` | `BackReverse.vst3` bundle. Extract to `~/.vst3/` (or `/usr/lib/vst3/`). |
| `BackReverse-v0.0.2-beta-Linux-x86_64-CLAP.tar.gz` | `BackReverse.clap`. Extract to `~/.clap/` (or `/usr/lib/clap/`). |
| `BackReverse-v0.0.2-beta-Linux-x86_64-SHA256SUMS.txt` | SHA-256 checksums of the three Linux archives. |

### Linux runtime requirements
The standalone and plug-ins need `libasound2` (ALSA), `libfreetype6` and `libfontconfig1`, plus the X11 client libraries (`libx11-6`, `libxext6`, `libxrandr2`, `libxinerama1`, `libxcursor1`) and OpenGL (`libgl1`). Install them with your package manager, for example `sudo apt install libasound2 libfreetype6 libfontconfig1 libx11-6 libxext6 libxrandr2 libxinerama1 libxcursor1 libgl1`. JACK is optional. A running X11 session or XWayland is required for the window.

### Highlights
- Sequential chunk reverse (reverses inside each chunk, keeps order), whole-source reverse, reordered chunks (11 order modes plus a user pattern language), forward chunks and Free Scrub.
- Per-chunk Rate and Time-Stretch at 1/4x, 1/2x, 1x, 2x, 3x or any ratio from 0.05x to 8x, with cycle, random, weighted, gate-follow and pattern-follow speed lanes.
- Visual gate sequencer (2 to 64 steps) with per-gate forward/reverse, nine shapes plus a custom curve, gap modes and per-gate FX.
- Stutter, Delay and Echo in a drag-to-reorder chain with Global, Alternating, Chunk Lane, Random and Gate Lane scopes.
- Pan and stereo patterns, polarity patterns and all-pass phase rotation.
- Scrub and vinyl scratch on the waveform, with Latch, Spring and Continue release modes.
- Seeded, deterministic randomness: offline renders match realtime.
- A/B compare, render to WAV, record output, turntable platter and Tape Stop (from 0.0.1).

### Fixed since 0.0.1
- **Gates**: in Silence gaps the gate envelope was applied twice, so shaped gates were squared (for example a Linear In gate at mid-cell gave half the intended level). It is now applied once; silent gaps close the FX tail through a separate 0.5 ms gate.
- **Phase**: the phase rotator is verified to be flat in magnitude at 200 Hz, 1 kHz and 5 kHz, and the left/right offset is exact. Its behaviour at 0° is documented (see Known limitations).
- **Audio file loading**: the resampler no longer reads past the end of the input when a file's sample rate differs from the device rate.
- **Host block sizes**: host blocks longer than the size prepared at start-up are processed in engine-sized slices, keeping the reverse scheduler on the host timeline.
- **Interface**: Variation Amount on the Patterns tab is no longer clipped, long knob labels scale instead of truncating, the curve-editor caption no longer overlaps the curve, and the randomiser can choose the Custom gate shape.
- **Performance**: the preset folder is scanned about every two seconds instead of 30 times a second, and the Delay and Echo filters no longer recalculate coefficients every sample.
- **Look**: tabs have a clear active state (accent underline) and the header and panels are aligned.

### Known limitations (beta)
- Phase Rotation is all-pass based. At 0° it applies a fixed, frequency-dependent all-pass colouration rather than being bit-transparent; set Phase Rotation to Off to bypass it. Left/right offset is exact. Details: [spec 00 §9](../specs/00-product-overview.md).
- Live reverse is limited to a 16 s buffer (a 48 s ring). Settings that need more are clamped, and the header shows CLAMPED when that happens.
- Formant preservation and transient-sensitive stretching are not implemented yet.
- Parallel FX buses and duplicate effect instances are not implemented yet. See [spec 08](../specs/08-implementation-status.md) for the full status list.
- macOS builds are not published yet.

### Testing in this release
- DSP acceptance tests: 114 checks pass (reverse order, chunk boundaries, fractional chunks, block sizes, rate and stretch, patterns, determinism, gates, pan and phase, FX, scrub, latency).
- State round trip test passes.
- Linux VST3: pluginval strictness 5 passes (`--skip-gui-tests`). Linux CLAP: clap-validator 0.3.2 runs 21 tests, 16 pass, 0 fail, 5 skipped. Windows validation runs in CI on the release build.
- Linux standalone starts and runs under a virtual display. Host-DAW testing (insert, save/reopen, automation, tempo change, transport seek, loop, offline render, freeze/bounce) has **not** been completed for this beta. Please report host-specific problems in the issue tracker.
