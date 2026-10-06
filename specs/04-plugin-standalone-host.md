# BackReverse — VST3, CLAP, and Standalone Specification

## 1. Shared core

VST3, CLAP, and standalone builds MUST compile from a shared processing core.

Format-specific code MUST be adapters around:
- audio I/O
- host transport
- parameters
- state serialization
- UI embedding

DSP behavior MUST not intentionally differ between formats.

## 2. VST3

VST3 MUST support:
- 64-bit builds
- stereo audio effect operation
- standard parameter automation
- host tempo
- host sample position where available
- transport play/stop state
- state save/restore
- latency reporting
- resizing/scaling

## 3. CLAP

CLAP MUST support:
- 64-bit builds
- stereo audio effect operation
- parameter automation
- sample-accurate or high-resolution parameter events where architecture permits
- host tempo/transport
- state save/restore
- latency reporting
- resizing/scaling

CLAP-specific capabilities MAY later expose modulation separately from automation.

## 4. Standalone

Standalone MUST support:
- audio input device selection
- audio output device selection
- sample rate selection where supported
- block/buffer size
- input channel selection
- output channel selection
- file loading
- play/pause/stop
- loop selection
- live capture
- processed monitoring
- record processed output when implemented
- preset load/save

## 5. File playback transport

Standalone file mode MUST expose:
- play
- pause
- stop
- seek
- loop
- return to start
- whole-source reverse
- sequential chunk reverse

Loaded audio duration and current position MUST be visible.

## 6. DAW synchronization

When host sync is enabled:
- musical chunk sizes MUST follow host tempo
- gate clock MAY follow host PPQ
- delay/echo synced values MUST follow host tempo
- patterns SHOULD restart according to selectable rules

Pattern restart rules:
- free-run
- on host play
- on bar
- on beat
- on manual trigger

Tempo changes MUST not crash or corrupt buffered audio.

## 7. Host transport edge cases

The engine MUST define behavior for:
- host stop during partial capture
- seek while playing
- loop jumps
- tempo automation
- offline rendering
- non-realtime bounce
- sample-rate change
- block-size change

Default seek behavior SHOULD flush or safely rebase live reverse buffers so stale audio from the prior timeline is not emitted unexpectedly.

## 8. Plug-in latency

Latency MUST be exposed accurately when practical.

Because chunk size determines reverse look-ahead, supported policies:
- dynamic reported latency
- fixed maximum latency

UI MUST explain the active policy.

Host compensation changes SHOULD occur only at safe times according to host/API constraints.

## 9. Offline render

Offline/non-realtime host rendering MUST produce deterministic output for a fixed:
- source
- parameters
- preset
- random seed
- automation

Randomized patterns MUST not change merely because the host renders faster than realtime.

## 10. Sidechain/future I/O

Sidechain is not required for 1.0.

Architecture SHOULD leave room for future:
- sidechain gate triggering
- external scratch/modulation input
- multichannel processing

## 11. Installation targets

Initial supported desktop OS target SHOULD prioritize Windows x64.

Architecture SHOULD avoid unnecessary Windows-only DSP dependencies so later macOS/Linux support remains possible.

VST3 and CLAP binaries MUST be installable to conventional plug-in locations.

Standalone MUST have a normal application executable and SHOULD eventually have an installer.
