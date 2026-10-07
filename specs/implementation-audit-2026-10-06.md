# BackReverse implementation audit — 2026-10-06

Reviewed branch: `dev/full-spec-implementation`

This review compared the implementation with the normative specs in this directory and ran the standalone DSP-core tests. It is a code audit, not a beta acceptance sign-off. The VST3, CLAP, and JUCE Standalone targets were not built in this environment.

## Verified fixes in this audit

- Center pan was applying equal-power gains to both stereo channels, attenuating the complete stereo image by about 3 dB. Center now preserves each channel's level; pan attenuates only the channel on the opposite side.
- The documented pattern grammar allows spaces between tokens, but the parser only split commas, semicolons, and line breaks. Whitespace now separates tokens.
- Non-finite/out-of-range values passed through `setChunkParams` and `setChunkPattern`. Durations, ratios, pan, phase, gate values, and custom curve points are now bounded/sanitized before processing.
- Added core regressions for each issue above.

## Spec coverage

| Area | Status | Audit result |
|---|---|---|
| Sequential and whole-source finite reversal | Partial | Core sample-order tests pass. Finite file transport, stop-at-end behavior, and file sample-rate conversion remain incomplete. |
| Live reversal | Partial | Fixed-size capture and reverse playback work for tested block boundaries. Live processing does not apply per-chunk rate/time-stretch, chunk order patterns, or variable chunk-size patterns. |
| Chunk boundaries | Missing | `BoundaryMode` and `crossfadeFrames` are configured but not applied. Reversed chunks can click at boundaries. |
| Rate and time-stretch | Partial | Finite-source processing has rate resampling and a basic granular stretch. Live chunks do not use either processor. Pitch-preservation tolerance and quality modes are not acceptance-tested. |
| Chunk order and patterns | Partial | Several ordering modes and a text pattern parser exist. Shuffle-no-repeat currently uses the same one-pass shuffle as random; loop/repeat, seed freeze UI, editable pattern lanes, and per-step probability controls are incomplete. |
| Gates | Partial | Core supports gate shapes, gaps, and forward slices. The plug-in UI exposes a fixed set of controls; custom curves and per-gate assignments have no complete acceptance coverage. |
| Stutter, delay, echo | Partial | Core effects and reorderable three-effect chains exist. The editor has no controls for `stutterMs`, `delayMs`, or `echoMs`, so users cannot set their times. Several required scopes and effect characteristics are absent. |
| Stereo and phase | Partial | Swap, polarity, pan, and all-pass phase coloration exist. Pattern timing, mirrored pan patterns, and scope assignment are not implemented. |
| Scrub | Partial | File playhead drag and velocity-based movement exist. Spring/latch/continue behavior is only partly wired; transport seek and loop handling are incomplete. |
| Host and standalone | Partial | CMake declares VST3, CLAP, and Standalone. Host automation/state code exists, but format builds, validators, device/channel selection UI, host transport edge cases, and multi-host tests are not verified. |
| State and presets | Partial | Host state and factory presets exist. The state schema has a version field but no migration path; user presets and broad state-equivalence tests are missing. |
| Help, undo, accessibility | Partial | Help text and tooltips exist, but help omits required tutorials/shortcuts. Gate edits and randomization update state without the undo manager, so the Undo button does not cover those operations. |
| Realtime safety | Blocked | The plug-in invokes parameter synchronization from `processBlock`; it converts the user-pattern text to `std::string` there. File load/unload changes vectors that the audio callback reads, and gate/UI state is shared without an audio-safe handoff. This violates the no-allocation/no-blocking/no-unsafe-lifetime requirements. |
| Resource use | Blocked | The default engine allocates capture and playback buffers for 10 minutes of stereo audio on construction (about 440 MiB at 48 kHz, before FX buffers and host scratch). Multiple plug-in instances can exhaust memory. |

## Priority follow-up

1. Replace plug-in/audio-thread shared mutable state with a bounded, preallocated parameter handoff and safe file-buffer lifetime scheme.
2. Reduce default memory allocation; allocate only the active/fixed-maximum live buffer and enforce a documented ceiling.
3. Apply configured crossfades and implement rate/stretch plus chunk patterns in live processing without callback allocation.
4. Correct file transport: resample when file and device rates differ, define stop/loop behavior, and safely handle seek/load/unload.
5. Add the missing effect-time controls and complete UI/state/undo acceptance coverage.
6. Build and validate VST3, CLAP, and Standalone; run host transport, automation, save/reopen, offline render, and long-run checks.

## Verification performed

- Built the core and test executable directly with GCC C++20 and `-Wall -Wextra -Wpedantic`; all 29 core tests passed.
- Rebuilt and ran those tests with AddressSanitizer and UndefinedBehaviorSanitizer; all 29 passed with leak detection disabled because LeakSanitizer cannot inspect this container's ptrace-restricted process environment.
- CMake was unavailable, and network access prevented cloning JUCE, so plug-in/standalone targets were not built.

## Resolution (2026-10-07, v0.0.1 beta)

The `dev/full-spec-implementation` code was superseded by a rewrite on a new shared engine (`src/core`). That branch was merged with its history kept, so it does not survive as an orphan. Each blocked or partial item above is resolved as follows:

| Finding | Resolution |
|---|---|
| Realtime safety blocked | The audio thread allocates nothing and takes no locks. Parameters are read from atomics. Patterns reach the audio thread through a single-writer seqlock (`SeqBox`). File buffers are `shared_ptr`s retired only after the audio thread has moved past them. |
| 440 MiB per instance | The live ring is 48 s and the capture buffer is 60 s, both allocated once in `prepare()`. Live reverse is capped at 16 s and the UI shows CLAMPED when a setting exceeds that. |
| Boundary crossfades not applied | Hard, Micro, Equal-Power and Zero-Cross boundaries are implemented with a two-voice crossfade, capped at 25% of a chunk. |
| Live rate/stretch/order/size patterns missing | The live scheduler reorders groups with latency = pattern length x maximum chunk size. Per-chunk speed, stretch and size apply in live mode too. |
| File transport / sample-rate conversion | Files are resampled with a windowed-sinc interpolator. Play, pause, stop, seek, loop region and stop-at-end are all supported. |
| Missing effect controls/scopes | All Stutter, Delay and Echo controls are exposed. All five scopes work, and the chain can be reordered by dragging. |
| Undo coverage | Every lane, gate, pattern, randomize, preset and paste edit goes through `UndoManager`. |
| State migration | State carries format, product and schema versions, has a migration hook, ignores unknown fields, and stores exact normalised values. |
| Formats not built/validated | VST3, CLAP and Standalone build. pluginval strictness 5 passes and clap-validator passes 16/16. CI repeats these checks on Windows. |
| Tests | 108 DSP acceptance checks (spec 06), plus a state round-trip test and a UI smoke test. |
