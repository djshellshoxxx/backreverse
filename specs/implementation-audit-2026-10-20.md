# BackReverse implementation audit — 2026-10-20

This audit reflects the `main` implementation reviewed before the changes in this branch and the changes made for this alpha. The earlier 2026-10-06 audit is retained as history in `implementation-audit-2026-10-06.md`; its first coverage table describes the pre-rewrite implementation. The 2026-10-07 resolution table and current source supersede those historical findings.

## Current feature state

| Area | Status | Evidence / remaining work |
|---|---|---|
| Core reversal, chunk ordering, fractional sizes | Implemented | `src/core/Engine.cpp`; 108 core checks pass. |
| Live buffers, crossfades, latency, rate/stretch, pan/phase, gates, FX and scratch | Implemented with documented limits | Shared engine, 108 DSP checks and processor state/UI tests. Formant preservation and transient-sensitive stretching remain absent. |
| Host state and factory/user presets | Partial | 17 factory presets and XML `.brpreset` save/load exist. Shared baseline calls for JSON import/export and robust custom-preset validation. |
| VST3, CLAP and Standalone | Implemented for Windows; macOS workflow added | Windows pipeline already builds and validates VST3/CLAP. Universal macOS packaging is configured in `.github/workflows/build.yml` and awaits CI evidence. Packages are unsigned without signing credentials. |
| MIDI performance | Added in this branch | Opt-in note trigger, velocity-to-wet and CC 1 speed controls; source event offsets are used. Plug-in build and DAW verification remain pending. |
| Shared CDL appearance | Partial | Canonical palette and 2 px signature notch are applied. No official logo asset exists. Theme selection and additional accessible themes are not implemented. |
| Visualization and common controls | Partial | Waveform, output meter, presets, bypass, randomize, help, undo and resize exist. CPU-use gauge and first-run introduction are absent. |
| Help and troubleshooting | Partial | Product help is embedded and the shared help standard is copied. Help still needs review against all new/remaining feature limitations. A project troubleshooting template was added because the shared README referenced a missing source template. |

## Changes in this branch

- Copied the shared `standards/`, `skills/README.md`, and `troubleshooting/README.md` material from shared revision `4c1a948bc93dc2e18ef676f2d3b913e7e1081eb7`, preserving BackReverse's existing skills.
- Added a BackReverse troubleshooting template and `SHARED_RESOURCE_MANIFEST.yaml`.
- Added three opt-in MIDI performance mappings: note-triggered pattern/file start, velocity-controlled wet mix, and mod-wheel speed scaling.
- Changed the UI base colors to the approved CDL palette and added the specified signature notch.
- Added a universal macOS CI build/package for Standalone, VST3 and CLAP. Main-branch CI is configured to publish a prerelease only after Windows, macOS and core jobs pass.

## Verification

- `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Isrc/core src/core/Engine.cpp tests/core_tests.cpp -o /tmp/backreverse-core-tests && /tmp/backreverse-core-tests` — passed 108/108 before the current plugin/UI/workflow edits; the core engine was not modified in this branch.
- The equivalent CMake build could not run in this environment because CMake is not installed.
- Full JUCE plugin, Standalone, MIDI event-offset, state, VST3 validation, CLAP validation, and macOS universal packaging have not yet run for this branch. GitHub Actions is the required next verification gate.

## Remaining work

1. Run CI and fix any Windows/macOS compile or packaging failures before merging.
2. Verify MIDI event behavior in at least one VST3 host and one CLAP host, including note-on and CC events within a block.
3. Implement JSON preset import/export with schema and range validation.
4. Add selectable themes, CPU-use reporting where meaningful, and first-run onboarding, or record explicit product decisions to defer them.
5. Consider the transient-aware and formant-preserving stretch algorithms as separate DSP projects with algorithm-specific acceptance tests.
