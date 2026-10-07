# BackReverse

**BackReverse** by Circuit Drift Labs lets you listen to, and perform with, audio played backwards in controllable chunks. It ships as a **Standalone app**, a **VST3** plug-in and a **CLAP** plug-in, all built from one shared DSP engine ([`src/core`](src/core)).

The core idea is that BackReverse does *not* reverse the order of chunks. It reverses the samples **inside** each chronological chunk. A 120 s song with 10 s chunks plays `10->0, 20->10, ... 120->110`. Chunk order, speed, stretch, gating, stereo and phase, and effects are all independent layers on top of that. See [`specs/`](specs/README.md) for the full specification.

## Download & install (Windows x64)

1. Open the **Releases** page of this repository and pick **v0.0.1-beta**.
2. Choose a download:
   - **Installer**: run `BackReverse-v0.0.1-beta-Windows-Setup.exe` and tick Standalone, VST3 and/or CLAP. VST3 goes to `C:\Program Files\Common Files\VST3`, CLAP goes to `C:\Program Files\Common Files\CLAP`, and the app goes to `C:\Program Files\Circuit Drift Labs\BackReverse`.
   - **Portable**: unzip `...-Portable.zip` anywhere and run `BackReverse.exe`.
   - **VST3 only**: unzip and copy the `BackReverse.vst3` folder into `C:\Program Files\Common Files\VST3`.
   - **CLAP only**: unzip and copy `BackReverse.clap` into `C:\Program Files\Common Files\CLAP`.
3. In your DAW, rescan plug-ins and insert **BackReverse** on an audio track as a stereo effect.
4. In the standalone app, click **Options** (top-left) to choose the audio input/output device, channels, sample rate and buffer size.

## Using it (quick start)
1. In the header, set **Source** to *Live Input* to reverse what's coming in, or *File* to use a loaded file (**Open**, drag-and-drop, or **Capture** live input).
2. On the **Chunks** tab, set **Chunk Length**. In live mode, the header shows the unavoidable reverse delay.
3. Use the **Time** tab for 1/4x to 3x speed, Rate vs Time-Stretch and per-chunk speed lanes. **Gates** is the gate sequencer, **Pan/Phase** handles stereo, **FX Rack** has Stutter, Delay and Echo, and **Scratch** has the turntable.
4. **Random** randomizes the domains you pick, using the seed. Every edit is undoable (Ctrl+Z).
5. The full manual is the **Help** tab inside the app ([`src/plugin/HelpText.h`](src/plugin/HelpText.h)).

## Build from source
Requirements: CMake 3.22 or newer and a C++17 compiler (Visual Studio 2022 on Windows). JUCE 8.0.4 and clap-juce-extensions are fetched automatically.

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64          # Windows
cmake --build build --config Release --target BackReverse_VST3 BackReverse_CLAP BackReverse_Standalone
ctest --test-dir build -C Release --output-on-failure      # core DSP + state tests
```
To run only the DSP tests without JUCE, use `cmake -B build -DBR_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build`. On Linux the plug-in also builds once the ALSA, X11, Xrandr, Xinerama, Xcursor, freetype and fontconfig development packages are installed.

CI ([`.github/workflows/build.yml`](.github/workflows/build.yml)) does the following on every push:
- runs the DSP tests on Linux;
- builds all three Windows targets and runs the tests;
- validates the VST3 with pluginval at strictness 5;
- runs clap-validator on the CLAP;
- packages the installer (Inno Setup, [`installer/BackReverse.iss`](installer/BackReverse.iss)) and the zips.

Pushing a `v*` tag, or running the workflow with a `release_tag`, publishes a GitHub release.

## Layout
| Path | Contents |
|---|---|
| `src/core/Types.h` | enums, parameter snapshot, pattern state, seeded hash PRNG, note table, lock-free seqlock |
| `src/core/Engine.*` | capture ring, chunk scheduler, reverse reader, rate/sinc interpolation, granular stretch, tape stop, scrub physics, pan/polarity/phase, gate, FX routing, output safety |
| `src/core/Fx.h` | Stutter, Delay, Echo, Hilbert phase rotator |
| `src/plugin/Params.h` | the stable host parameter table (IDs never change) |
| `src/plugin/PluginProcessor.*`, `Presets.cpp` | format adapter: state, lanes, presets, file/capture, render/record, A/B, randomize, copy/paste |
| `src/plugin/UI.h`, `PluginEditor.*`, `HelpText.h` | the editor |
| `tests/` | `core_tests.cpp` (spec 06 DSP acceptance tests) and `state_test.cpp` (state round trip) |

## Licensing notes
The source code is MIT ([LICENSE](LICENSE)). The names BackReverse™ and Circuit Drift Labs™ are covered in [COPYRIGHT-TRADEMARK.md](COPYRIGHT-TRADEMARK.md). The binaries link **JUCE 8**, which you can use under its free *Starter* licence (under USD 50k revenue) or under AGPLv3. They also include the Steinberg VST3 SDK and the MIT-licensed CLAP SDK. If you distribute binaries, check that your use fits those terms.
