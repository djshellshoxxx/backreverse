# BackReverse — Product Specification

## 1. Product identity

BackReverse is an audio-manipulation application and plug-in designed for listening to and performing recorded or live-routed audio backwards in controllable near-real-time segments.

Required targets:
- Standalone desktop application
- VST3 plug-in
- CLAP plug-in

All targets MUST share the same DSP engine, parameter definitions, preset format where practical, and core UI behavior.

## 2. Primary use cases

BackReverse MUST support:

1. Listening to live or routed audio backwards in configurable chunks.
2. Loading a complete audio file and playing it backwards.
3. Performing moving-window reversal on a file or incoming signal.
4. Reordering reversed chunks using deterministic and randomized patterns.
5. Changing the playback speed or time-stretch of individual chunks.
6. Manipulating stereo pan and phase/polarity independently or in synchronization with reversal chunks.
7. Gating individual chunks or subdivisions.
8. Applying stutter, delay, and echo globally, per chunk, or to selected gates.
9. Scrubbing the playback head for scratch and vinyl-style effects.
10. Automating musically useful controls from a DAW host.
11. Saving and restoring deterministic patterns and random states.

## 3. Core reversal model

BackReverse operates on ordered input windows.

Given input duration D and selected chunk duration C:

- Divide the input into consecutive windows [0,C), [C,2C), [2C,3C), etc.
- The final window MAY be shorter than C.
- Each window is emitted with its samples in reverse chronological order.
- Unless a rearrangement mode is enabled, window order itself remains chronological.

Example, 120-second input with C=10 seconds:

- output window 1 = input 10s -> 0s
- output window 2 = input 20s -> 10s
- output window 3 = input 30s -> 20s
- ...
- output window 12 = input 120s -> 110s

For a 120-second file with C=30 seconds:

- 30 -> 0
- 60 -> 30
- 90 -> 60
- 120 -> 90

If C >= total file duration, the complete file MUST play from its final sample to its first sample.

This behavior is called **Sequential Chunk Reverse**.

## 4. Reverse modes

### 4.1 Sequential Chunk Reverse
Reverse samples inside each chunk while preserving chunk order.

### 4.2 Whole Source Reverse
Play a complete loaded file from end to start.

For live input, this mode is unavailable until a finite recording/capture buffer exists.

### 4.3 Reordered Chunk Reverse
Reverse each chunk internally, then reorder chunk playback according to the active sequence pattern.

### 4.4 Free Scrub Reverse
Playback direction and position are controlled directly by a movable playhead.

### 4.5 Hybrid
A live playhead can interrupt the sequenced position temporarily, then either return, latch, or continue from the new position.

## 5. Chunk duration

Chunk length MUST be user selectable.

Supported representations:
- milliseconds
- seconds
- musical note values when tempo is available
- samples in advanced mode

The implementation MUST permit fractional-second values.

Target range:
- 1 ms to 10 minutes via normal controls
- direct numeric entry for larger values
- Whole File shortcut
- host-synced values from 1/128 note through at least 32 bars, including dotted and triplet values

Parameter changes MUST be smoothed or boundary-quantized to avoid clicks.

## 6. Chunk playback-rate and time-stretch

Every chunk MUST support independent temporal processing.

Required fixed ratios:
- 0.25x quarter-time
- 0.5x half-time
- 1.0x normal
- 2.0x double-time
- 3.0x triple-time

The user MUST also be able to enter arbitrary ratios within a safe implementation range.

Two distinct modes MUST exist:

### Rate mode
Changes playback duration and pitch together, analogous to changing tape or record speed.

Examples:
- 0.5x produces half-speed, lower-pitched playback.
- 2.0x produces double-speed, higher-pitched playback.

### Time-stretch mode
Changes duration while attempting to preserve pitch.

The engine SHOULD eventually provide multiple time-stretch algorithms optimized for:
- rhythmic/transient material
- vocals
- complex/polyphonic material
- deliberately extreme/surreal stretching

Playback-rate or stretch values MAY be:
- global
- assigned per chunk
- assigned by pattern
- randomized per chunk
- probability-selected from a user-defined set
- synchronized to gates
- automated from the host

A sequence MAY mix temporal modes, for example:
1x, 0.5x, 2x, 0.25x, 3x, stretch 175%, 1x.

The sequencer MUST support locking the random seed so an arrangement is exactly repeatable.

## 7. Near-real-time behavior and latency

Reversing an unseen future interval requires buffering that interval. Sequential Chunk Reverse therefore introduces a minimum algorithmic delay approximately equal to the selected chunk size.

The UI MUST display:
- selected chunk length
- effective buffering delay
- plug-in latency where available

VST3 and CLAP builds SHOULD report stable latency to the host when practical.

Two latency policies MUST be designed:

- **Dynamic/Live:** chunk-size changes take effect promptly; host delay compensation may not track rapid latency changes perfectly.
- **Fixed Maximum:** the user selects a maximum reverse buffer and the plug-in reports a fixed latency so chunk length can vary inside that allocation.

Standalone mode MUST use the same buffering semantics.

## 8. Source handling

Standalone MUST support:
- audio-device input
- drag/drop file loading
- file-open dialog
- recording live input into a finite capture buffer
- stereo output

Plug-in builds MUST support:
- stereo effect input/output
- mono-in/stereo-out where supported
- host transport and tempo
- automation
- preset/state serialization

Initial file targets SHOULD include WAV, AIFF, FLAC, MP3, and OGG where framework support permits.

## 9. Stereo and phase terminology

The interface MUST distinguish the following:

### Polarity Invert
Multiply the selected channel(s) by -1.

Modes:
- none
- left
- right
- both

### Stereo/Pan Reverse
Transform stereo placement.

Required modes:
- swap L/R
- mirror current pan position
- invert a pan automation trajectory
- per-chunk pan pattern

### Phase Manipulation
A separate processor for actual phase manipulation.

Possible modes:
- off
- polarity invert
- variable phase rotation
- left/right phase offset

If variable phase is implemented, documentation MUST describe whether it is broadband, all-pass based, or frequency dependent.

**Implementation note (BackReverse 0.0.2).** Phase Rotation is broadband and all-pass based. It is built from an IIR Hilbert pair (`src/core/Fx.h`, `struct Hilbert`) whose two outputs have exactly unity magnitude. The rotation is `I·cos θ ± Q·sin θ`, so the magnitude response is flat at every angle. The phase response is the pair's common all-pass phase φ(ω) plus the chosen angle θ. Consequences:
- At 0° the rotator is not bit-transparent. It applies a fixed, frequency-dependent all-pass phase colouration. Use Phase Rotation = Off to bypass it. The engine fades the rotator in and out over 20 ms.
- Left/right offset is exact at all frequencies: the right channel is the left channel rotated by the chosen angle.
- Polarity inversion is a separate processor (`PolMode`) and is never implemented as a phase rotation.
- The phase colouration is audible mainly on transients. It is documented here rather than hidden because a fixed-latency FIR alternative would shift every file-mode output sample.

## 10. Processing scope

Reversal, rate/stretch, stereo/pan, phase/polarity, gating, and effects MUST support scopes where meaningful:

- Global
- Every chunk
- Alternating chunks
- Selected chunks
- Pattern-selected chunks
- Random per chunk
- Selected gates

Pan/phase timing MUST be able to:
- lock to reverse chunk boundaries
- use an independent chunk length
- run in parallel
- start with an offset relative to the reverse stream

## 11. Non-destructive architecture

All playback processing MUST be real-time and non-destructive.

Standalone SHOULD later support rendered export, but destructive waveform editing is not required for initial releases.

## 12. Product stages

Prototype:
- file playback
- core reversal

Alpha:
- live buffering
- chunk scheduling
- rate/stretch engine
- scrub
- pattern sequencer

Beta:
- gate
- effects
- automation
- VST3
- CLAP
- standalone
- preset system

1.0:
- robust project/state restoration
- installers
- documentation
- broad automated tests
- performance validation

## 13. Realtime safety requirements

- Audio callback MUST NOT allocate memory.
- Audio callback MUST NOT perform file I/O.
- Audio callback MUST NOT acquire blocking locks.
- Parameters MUST be thread-safe.
- DSP MUST survive sample-rate and block-size changes.
- Denormals MUST be handled.
- Irregular host block sizes MUST be supported.
- NaN/Inf MUST never propagate to output.
- Bypass and routing changes MUST be click-free.
