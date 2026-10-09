# BackReverse

**BackReverse** by Circuit Drift Labs lets you listen to, and perform with, audio played backwards in controllable chunks. It ships as a **Standalone app**, a **VST3** plug-in and a **CLAP** plug-in, all built from one shared DSP engine ([`src/core`](src/core)). Windows x64 and Linux x86_64 are supported in the current beta.

The core idea is that BackReverse does *not* reverse the order of chunks. It reverses the samples **inside** each chronological chunk. A 120 s song with 10 s chunks plays `10->0, 20->10, ... 120->110`. Chunk order, speed, stretch, gating, stereo and phase, and effects are all independent layers on top of that. See [`specs/`](specs/README.md) for the full specification and [`specs/08-implementation-status.md`](specs/08-implementation-status.md) for what is done.

## Download & install

Open the **Releases** page of this repository and pick **v0.0.2-beta**.

### Windows x64
- **Installer**: run `BackReverse-v0.0.2-beta-Windows-Setup.exe` and tick Standalone, VST3 and/or CLAP. VST3 goes to `C:\Program Files\Common Files\VST3`, CLAP goes to `C:\Program Files\Common Files\CLAP`, and the app goes to `C:\Program Files\Circuit Drift Labs\BackReverse`.
- **Portable**: unzip `...-Portable.zip` anywhere and run `BackReverse.exe`.
- **VST3 only**: unzip and copy the `BackReverse.vst3` folder into `C:\Program Files\Common Files\VST3`.
- **CLAP only**: unzip and copy `BackReverse.clap` into `C:\Program Files\Common Files\CLAP`.

### Linux x86_64
- **Standalone**: extract `BackReverse-v0.0.2-beta-Linux-x86_64-Standalone.tar.gz`, then run `./BackReverse`.
- **VST3**: extract `...-VST3.tar.gz` and copy the `BackReverse.vst3` folder to `~/.vst3/` (or `/usr/lib/vst3/`).
- **CLAP**: extract `...-CLAP.tar.gz` and copy `BackReverse.clap` to `~/.clap/` (or `/usr/lib/clap/`).
- Runtime libraries: `libasound2`, `libfreetype6`, `libfontconfig1`, the X11 client libraries (`libx11-6 libxext6 libxrandr2 libxinerama1 libxcursor1`) and `libgl1`. Verify the archives with the `SHA256SUMS` file.

In your DAW, rescan plug-ins and insert **BackReverse** on an audio track as a stereo effect. In the standalone app, click **Options** (top-left) to choose the audio input/output device, channels, sample rate and buffer size.

## Using it (quick start)
1. In the header, set **Source** to *Live Input* to reverse what's coming in, or *File* to use a loaded file (**Open**, drag-and-drop, or **Capture** live input).
2. On the **Chunks** tab, set **Chunk Length**. In live mode, the header shows the unavoidable reverse delay.
3. Use the **Time** tab for 1/4x to 3x speed, Rate vs Time-Stretch and per-chunk speed lanes. **Gates** is the gate sequencer, **Pan/Phase** handles stereo, **FX Rack** has Stutter, Delay and Echo, and **Scratch** has the turntable.
4. **Random** randomizes the domains you pick, using the seed. Every edit is undoable (Ctrl+Z).
5. The full manual is the **Help** tab inside the app ([`src/plugin/HelpText.h`](src/plugin/HelpText.h)).

## Build from source
Requirements: CMake 3.22 or newer and a C++17 compiler. JUCE 8.0.4 and clap-juce-extensions are fetched automatically unless you point CMake at local copies with `-DBR_JUCE_DIR=<path>` and `-DBR_CJE_DIR=<path>`.

**Windows** (Visual Studio 2022):
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target BackReverse_VST3 BackReverse_CLAP BackReverse_Standalone
ctest --test-dir build -C Release --output-on-failure
```

**Linux** (Ubuntu 24.04 or newer):
```bash
sudo apt-get install -y ninja-build libasound2-dev libjack-jackd2-dev libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl-dev libgtk-3-dev libwebkit2gtk-4.1-dev libcurl4-openssl-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --target BackReverse_VST3 BackReverse_CLAP BackReverse_Standalone
ctest --test-dir build --output-on-failure
```
Build artifacts are under `build/src/plugin/BackReverse_artefacts/Release/`.

To run only the DSP tests without JUCE: `cmake -B build -DBR_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build`.

CI ([`.github/workflows/build.yml`](.github/workflows/build.yml)) runs on every push:
- DSP tests on Linux;
- a Linux job that builds all three targets, runs the tests, validates the VST3 with pluginval (strictness 5) and the CLAP with clap-validator 0.3.2, and creates the tarballs;
- a Windows job that does the same and builds the installer (Inno Setup, [`installer/BackReverse.iss`](installer/BackReverse.iss)) and zips.

Pushing a `v*` tag, or running the workflow with a `release_tag`, publishes a GitHub pre-release. The packaging rules are in [`specs/07-release-packaging.md`](specs/07-release-packaging.md).

## Layout
| Path | Contents |
|---|---|
| `src/core/Types.h` | enums, parameter snapshot, pattern state, seeded hash PRNG, note table, lock-free seqlock |
| `src/core/Engine.*` | capture ring, chunk scheduler, reverse reader, rate/sinc interpolation, granular stretch, tape stop, scrub physics, pan/polarity/phase, gate, FX routing, output safety |
| `src/core/Fx.h` | Stutter, Delay, Echo, Hilbert all-pass phase rotator |
| `src/plugin/Params.h` | the stable host parameter table (IDs never change) |
| `src/plugin/PluginProcessor.*`, `Presets.cpp` | format adapter: state, lanes, presets, file/capture, render/record, A/B, randomize, copy/paste |
| `src/plugin/UI.h`, `PluginEditor.*`, `HelpText.h` | the editor |
| `specs/` | normative specification (00–06), release packaging (07), implementation status (08) and audits |
| `tests/` | `core_tests.cpp` (spec 06 DSP acceptance tests) and `state_test.cpp` (state round trip) |

## Licensing notes
The source code is MIT ([LICENSE](LICENSE)). The names BackReverse™ and Circuit Drift Labs™ are covered in [COPYRIGHT-TRADEMARK.md](COPYRIGHT-TRADEMARK.md). The binaries link **JUCE 8**, which you can use under its free *Starter* licence (under USD 50k revenue) or under AGPLv3. They also include the Steinberg VST3 SDK and the MIT-licensed CLAP SDK. If you distribute binaries, check that your use fits those terms.
