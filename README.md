# BackReverse

**BackReverse** by Circuit Drift Labs lets you listen to, and perform with, audio played backwards in controllable chunks. It ships as a **Standalone app**, a **VST3** plug-in and a **CLAP** plug-in, all built from one shared DSP engine ([`src/core`](src/core)).

The core idea is that BackReverse does *not* reverse the order of chunks. It reverses the samples **inside** each chronological chunk. A 120 s song with 10 s chunks plays `10->0, 20->10, ... 120->110`. Chunk order, speed, stretch, gating, stereo and phase, and effects are all independent layers on top of that. See [`specs/`](specs/README.md) for the full specification.

## Download & install

Open the [Releases page](https://github.com/djshellshoxxx/backreverse/releases) and choose the package for your platform. The current CI release workflow publishes Windows x64 and universal macOS packages for Standalone, VST3 and CLAP.

### Windows x64

- **Installer**: run the `Windows-Setup.exe` package and select Standalone, VST3 and/or CLAP. VST3 goes to `C:\Program Files\Common Files\VST3`, CLAP goes to `C:\Program Files\Common Files\CLAP`, and the app goes to `C:\Program Files\Circuit Drift Labs\BackReverse`.
- **Portable**: unzip the `Windows-Portable.zip` package anywhere and run `BackReverse.exe`.
- **VST3 only**: unzip the `Windows-VST3.zip` package and copy `BackReverse.vst3` into `C:\Program Files\Common Files\VST3`.
- **CLAP only**: unzip the `Windows-CLAP.zip` package and copy `BackReverse.clap` into `C:\Program Files\Common Files\CLAP`.

### macOS Universal (Apple Silicon and Intel)

- **Standalone**: unzip `macOS-Universal-Standalone.zip`, then move `BackReverse.app` to Applications.
- **VST3**: unzip `macOS-Universal-VST3.zip` and copy `BackReverse.vst3` to `~/Library/Audio/Plug-Ins/VST3`.
- **CLAP**: unzip `macOS-Universal-CLAP.zip` and copy `BackReverse.clap` to `~/Library/Audio/Plug-Ins/CLAP`.

After installing a plug-in, rescan plug-ins in your DAW and insert BackReverse on an audio track as a stereo effect.

In the standalone app, use **Options** (top-left) to choose the audio device, input/output channels, sample rate and buffer size.

## Using it (quick start)
1. In the header, set **Source** to *Live Input* to reverse what's coming in, or *File* to use a loaded file (**Open**, drag-and-drop, or **Capture** live input).
2. On the **Chunks** tab, set **Chunk Length**. In live mode, the header shows the unavoidable reverse delay.
3. Use the **Time** tab for 1/4x to 3x speed, Rate vs Time-Stretch and per-chunk speed lanes. **Gates** is the gate sequencer, **Pan/Phase** handles stereo, **FX Rack** has Stutter, Delay and Echo, and **Scratch** has the turntable.
4. The **MIDI** tab offers three optional performance mappings: note-on pattern/file trigger, note velocity to wet mix, and mod wheel to speed. They are off by default.
5. **Random** randomizes the domains you pick, using the seed. Every edit is undoable (Ctrl+Z).
6. The full manual is the **Help** tab inside the app ([`src/plugin/HelpText.h`](src/plugin/HelpText.h)).

## Build from source
Requirements: CMake 3.22 or newer and a C++17 compiler (Visual Studio 2022 on Windows). JUCE 8.0.4 and clap-juce-extensions are fetched automatically.

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64          # Windows
cmake --build build --config Release --target BackReverse_VST3 BackReverse_CLAP BackReverse_Standalone
ctest --test-dir build -C Release --output-on-failure      # core DSP + state tests
```
To run only the DSP tests without JUCE, use `cmake -B build -DBR_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build`. On Linux the plug-in also builds once the ALSA, X11, Xrandr, Xinerama, Xcursor, freetype and fontconfig development packages are installed.

CI ([`.github/workflows/build.yml`](.github/workflows/build.yml)) does the following on every push and pull request:
- runs the DSP tests on Linux;
- builds Windows x64 Standalone, VST3 and CLAP, runs tests, and validates VST3/CLAP;
- builds universal macOS Standalone, VST3 and CLAP for arm64 and x86_64 and runs tests;
- validates the VST3 with pluginval at strictness 5;
- runs clap-validator on the CLAP;
- packages the Windows installer and standalone/VST3/CLAP archives plus separate universal macOS archives.

Merging to `main`, pushing a `v*` tag, or dispatching the workflow with a `release_tag` publishes a prerelease after all required builds pass.

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
