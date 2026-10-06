# BackReverse — Parameters, Presets, Automation, and State Specification

## 1. Stable parameter identity

Every host-visible parameter MUST have a stable immutable identifier.

Renaming display text MUST NOT change parameter identity.

Parameter IDs MUST never be reused for unrelated functions after release.

## 2. Parameter classes

### Global
- bypass
- input gain
- output gain
- wet/dry
- mode
- latency policy

### Chunk
- duration
- unit/sync
- boundary crossfade
- order mode
- variation amount

### Time
- temporal mode: Rate / Stretch
- ratio
- preserve pitch
- algorithm quality
- transient/formant controls where available

### Pan/phase
- pan
- stereo swap
- polarity mode
- phase amount
- pan/phase pattern mode

### Gate
- enabled
- step count
- gate width
- gap
- shape
- depth
- timing mode

### Stutter
- enabled
- period
- length
- repeats
- direction
- decay
- wet
- dry
- probability

### Delay
- enabled
- time
- sync
- feedback
- cross-feedback
- filtering
- ping-pong
- modulation
- wet/dry

### Echo
- enabled
- time
- feedback/repeats
- decay
- tone
- spread
- drift
- wow/flutter
- character
- wet/dry

### Scratch
- mode
- playhead
- inertia
- friction
- max rate
- return behavior

## 3. Automation

Continuous controls SHOULD be automatable unless automation is unsafe or conceptually inappropriate.

Pattern structure changes do not need to be directly host-automatable; pattern selection MUST be automatable.

Automation MUST be smoothed where discontinuities would click.

Discrete state changes SHOULD be applied at safe sample/block/chunk boundaries as appropriate.

## 4. Presets

Preset MUST save:
- all global DSP values
- chunk settings
- order pattern
- chunk-size pattern
- rate/stretch pattern
- pan pattern
- phase/polarity pattern
- gate pattern
- effect chain
- per-scope assignments
- random seed
- UI state where reasonable

Presets MUST NOT embed copyrighted source audio by default.

## 5. Deterministic random state

Randomized behavior MUST use explicit seeded PRNG state.

A recalled project/preset with a locked seed MUST reproduce the same:
- chunk ordering
- rate selection
- gate randomization
- pan/phase randomization
- effect assignment

Random state MUST be independent from wall-clock time during deterministic playback/render.

## 6. State versioning

Serialized state MUST include:
- format version
- product version
- parameter schema version

The loader MUST support migration from older state versions once public versions exist.

Unknown future fields SHOULD be ignored safely where possible.

## 7. Preset categories

Factory presets SHOULD demonstrate:
- Basic 5-second reverse
- Whole-track reverse
- Half-speed reversed
- Quarter-speed surreal
- Triple-speed fragments
- Vocal stretch
- Random cut-up
- Alternating forward/reverse gates
- Pan mirror chunks
- Phase/polarity pattern
- Stutter chain
- Delay chain
- Echo chain
- Scratch/vinyl
- Extreme surreal

## 8. Copy/paste

The UI SHOULD support copying/pasting:
- chunk pattern
- gate pattern
- FX chain
- rate/stretch pattern
- pan/phase pattern

This enables reusable sub-presets.
