# BackReverse — Release, Packaging and Platform Specification

This document defines what a BackReverse release contains, how each platform artifact is built, validated and named, and what a beta release must demonstrate. It is the source for `.github/workflows/build.yml`, `installer/BackReverse.iss` and `docs/RELEASE_NOTES.md`.

## 1. Release identity

- Product version: `project(BackReverse VERSION x.y.z)` in `CMakeLists.txt` is the single source of truth. It is passed to the plug-in build as `BR_VERSION_STRING` and shown in the editor title tooltip and the Global page.
- Release label: `vX.Y.Z-beta` (for example `v0.0.2-beta`). Pre-1.0 releases are always marked as GitHub pre-releases.
- Parameter schema version and state format version are independent of the product version (spec 05 §6). A product release MUST NOT change a parameter ID that was shipped in an earlier release (spec 05 §1).
- Version bump rule: a patch bump (`0.0.x`) is used for beta fixes; a minor bump (`0.x.0`) is used when a new platform, a new format or a new user-visible feature set ships.

## 2. Targets

| Target | Windows x64 | Linux x86_64 | macOS |
|---|---|---|---|
| Standalone | required | required | not yet (spec 04 §11) |
| VST3 (64-bit) | required | required | not yet |
| CLAP (64-bit) | required | required | not yet |
| Installer | Inno Setup (`installer/BackReverse.iss`) | not required; tarballs | — |

Every target MUST be built from the same `src/core` and `src/plugin` sources in the same CMake configuration. Platform code MUST live only in JUCE and in format adapters (spec 04 §1).

## 3. Artifacts per release

The release MUST contain these files. Names use the release label `<LABEL>`.

Windows (`windows-2022` runner):
- `BackReverse-<LABEL>-Windows-Setup.exe` — installer: standalone, VST3, CLAP (components selectable).
- `BackReverse-<LABEL>-Windows-Portable.zip` — `BackReverse.exe` and the licence files.
- `BackReverse-<LABEL>-Windows-VST3.zip` — `BackReverse.vst3` bundle.
- `BackReverse-<LABEL>-Windows-CLAP.zip` — `BackReverse.clap`.

Linux (`ubuntu-24.04` runner):
- `BackReverse-<LABEL>-Linux-x86_64-Standalone.tar.gz` — `BackReverse` executable plus `README.md`, `LICENSE`, `COPYRIGHT-TRADEMARK.md`.
- `BackReverse-<LABEL>-Linux-x86_64-VST3.tar.gz` — `BackReverse.vst3` bundle (`Contents/x86_64-linux/BackReverse.so`) plus licence files.
- `BackReverse-<LABEL>-Linux-x86_64-CLAP.tar.gz` — `BackReverse.clap` plus licence files.
- `BackReverse-<LABEL>-Linux-x86_64-SHA256SUMS.txt` — SHA-256 sums of every Linux archive.

Every archive MUST be reproducible from the tagged commit by running CI again. Archives MUST NOT contain build directories, object files, preset files or user data.

## 4. Build requirements

### 4.1 Windows
- Visual Studio 2022, x64, `cmake -G "Visual Studio 17 2022" -A x64`.
- Release configuration, `BackReverse_VST3 BackReverse_CLAP BackReverse_Standalone br_core_tests BackReverseStateTest` targets.

### 4.2 Linux
- Ubuntu 24.04 or newer, GCC 13 or Clang 18, CMake 3.22 or newer, Ninja.
- Development packages: `libasound2-dev libjack-jackd2-dev libfreetype-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl-dev libgtk-3-dev libwebkit2gtk-4.1-dev libcurl4-openssl-dev`.
- Configure with `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`. JUCE and clap-juce-extensions are fetched by CMake, or supplied with `-DBR_JUCE_DIR=<path>` and `-DBR_CJE_DIR=<path>` for offline builds.
- The Linux build MUST compile with `-Wall -Wextra` and produce no new warnings in `src/core` (the core is the realtime-critical code).
- Runtime dependencies of the standalone binary MUST be listed in the release notes, taken from `ldd` on the built binary.

### 4.3 Shared
- Core tests (`br_core_tests`) and the state test (`BackReverseStateTest`) MUST pass before any artifact is packaged.
- The version stamp inside every binary MUST equal `project(VERSION)`.

## 5. Validation gates

A release candidate MUST pass every gate below. A gate failure blocks packaging.

| Gate | Tool | Required result |
|---|---|---|
| DSP acceptance | `br_core_tests` (spec 06) | every check passes |
| State round trip | `BackReverseStateTest` | every check passes |
| VST3 validation | pluginval, strictness 5, `--skip-gui-tests`, 600 s timeout | exit 0 |
| CLAP validation | clap-validator 0.3.2 (`--only-failed`) | exit 0 |
| Standalone smoke | launch and load a file (manual or scripted) | no crash, audio device opens |

pluginval and clap-validator are obtained from their upstream releases or built from source. A failing gate is never silenced by skipping a test (spec 06 §1).

Manual host checks (spec 06 §17) are required before a stable release. For a beta, the release notes MUST list which hosts were tested. The beta gate is: insert, save/reopen, automation, tempo change, transport seek, loop, offline render, freeze/bounce in at least one DAW per platform.

## 6. Installer and layout rules

- Windows installer: standalone under `Program Files\Circuit Drift Labs\BackReverse`, VST3 under `Common Files\VST3`, CLAP under `Common Files\CLAP`.
- Linux tarballs: the user copies the VST3 bundle to `~/.vst3/` (or `/usr/lib/vst3/`) and the CLAP file to `~/.clap/` (or `/usr/lib/clap/`). The standalone binary is run in place.
- No artifact may write outside its install location. Presets and renders go to `Documents/BackReverse/` (spec 05 §4).

## 7. Release notes

`docs/RELEASE_NOTES.md` MUST contain, in this order:
1. Download table for every artifact of the release.
2. Highlights of the release.
3. Fixes since the last release, grouped by area (engine, plug-in, UI, build).
4. Known limitations, copied from `specs/08-implementation-status.md` items marked *Partial* or *Not implemented* that a user can observe.
5. Hosts and platforms tested, and the runtime dependencies on Linux.

## 8. Publishing

- Publishing a GitHub release is an outward-facing action. It happens only when the workflow is run with a `release_tag` or a `v*` tag is pushed by a maintainer.
- Every release is a pre-release until the 1.0 criteria in spec 06 §20 are met.
- The release job MUST wait for every platform job. A partial release (for example Windows only) MUST NOT be published under the same tag.

## 9. Traceability

Each release MUST have a short audit note under `specs/` (for example `implementation-audit-2026-10-08.md`) that records the findings, fixes and open items for that release.
