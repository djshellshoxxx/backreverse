# BackReverse feature audit and generative expansion — 2026-10-12

## Baseline and scope
Audited `main` README, CMake project, core Types/Engine headers, DSP test file, plugin parameter definitions, and existing specification index. Open PR #2 addresses Linux packaging and audio fixes; do not duplicate or cherry-pick its work without reviewing its diff. Existing features include chunk reversal, file/live capture, ordering, stretch/rate, gate sequencer, phase/pan, stutter, delay, echo, scratch and seeded variation. Current main reports a v0.0.1-beta release. No independent full binary build, sanitizer pass, or DAW-host validation was performed as part of this audit.

## Highest-value feature candidates
| Rank | Feature | Value | Primary integration | Status |
|---|---|---|---|---|
| 1 | Generative rhythm / gate-pattern designer | Fast musically useful results, discoverability | Pure core pattern generator -> existing GateStep and gate UI | Core API implementation planned here |
| 2 | Transient-aware reverse slicing | Better drum-loop intelligibility and fewer unnatural slice boundaries | Offline transient analyzer -> chunk scheduler | Spec only |
| 3 | Pattern scene morphing and MIDI recall | Live performance and DAW automation | Versioned scene bank, snapshot interpolation, MIDI mapping | Spec only |
| 4 | Audio-to-MIDI reverse rhythm export | Allows workflows with external drum instruments | Offline event extractor -> SMF writer | Concept; needs product validation |
| 5 | Real-time CPU/latency and clipping diagnostics | Trust and practical usability | lock-free telemetry and UI | Concept |

## Feature 1: generative gate/rhythm designer (implementation target)
### Objective
Generate playable, deterministic, editable gate grids in common rhythmic styles with a small API that does not require JUCE. It fills the existing `Patterns::gate` array; no new host IDs, serialization format or DSP allocation are required. Generation happens on the UI/message thread, never in `process()`.

### API contract
`src/core/RhythmGenerator.h` declares `br::RhythmStyle { Euclidean, Backbeat, Syncopated, Sparse, Dense }`, `RhythmRequest { steps, pulses, rotation, seed, variation, accent, style }` and `generateRhythm(const RhythmRequest&, Patterns&)`. Generator MUST only mutate the first `steps` entries of `Patterns::gate` (their on/width/depth/direction/fx fields) and MUST preserve all unrelated lanes. Existing GateStep field defaults are honored for enabled cells.

### Algorithm
Euclidean distributes `pulses` over `steps` via `floor((i+1)*pulses/steps) != floor(i*pulses/steps)`, applying signed normalized rotation. Backbeat marks quarter-grid snare accents and anchor beats. Syncopated favors offbeats; Sparse and Dense favor 25%/75% activity respectively. For non-Euclidean styles, a seeded stateless hash chooses non-anchor hits. `variation` from 0–1 controls variation probability, with at least one enabled gate for styles other than Euclidean if steps > 0. `accent` from 0–1 adjusts the width of strong/weak cells within [0.15,1]. All values are clamped, integer modulo works for negative rotation, and no random device or clock is consulted.

### Integration and acceptance
- Default behavior remains unchanged unless explicit generate action is invoked.
- The existing gate editor should expose Style, Pulses, Rotation, Variation, Accent, Seed, Preview and Apply in a follow-up UI PR. Preview MUST be non-mutating; Apply MUST use existing undo/state publication path, and refresh the visualization. The first increment only builds and tests the pure core generator, not the UI.
- Generation must be deterministic across invocations and host buffer sizes.
- 0 pulses -> silence and N pulses -> full grid for Euclidean; negative rotations supported.
- No heap allocations, exceptions or locks inside generator; bounded O(steps) work (steps <= 64).
- Unit tests check counts, rotation, determinism, preservation of non-gate lanes, fixed default fields, and edge cases. Add a separate CTest executable.
- Once UI integrated, run pluginval and clap-validator; test preset reload, undo/redo, automation and DAW state restoration.

## Feature 2: transient-aware slice suggestion
Offline onset envelope (windowed RMS difference and optional spectral flux) computes candidates from a decoded source with sensitivity and minimum spacing. UI shows candidate markers; committing markers creates chunk lengths or a bounded user sequence. Live capture requires an explicit frozen buffer. Must preserve chronological chunk semantics and reject invalid or insufficient samples. No analysis on audio thread. Tests: silence, impulses, swung percussion, stereo, sample-rate changes, boundaries and regression against manual chunks. Do not promise perfect source separation or beat detection.

## Feature 3: pattern scenes
At least 8 named scenes store full versioned parameter+pattern snapshots; A/B morph smoothly interpolates scalar controls while enums, trigger flags, and gate directions switch at the midpoint with click-free DSP transitions. MIDI program-change or mapped note recalls scenes on the message thread via a lock-free publication mechanism; MIDI events in the audio callback must be handled without allocations. Host project persistence must include schema version and older preset migrations. Acceptance: exact scene recall, real-time-safe interpolation, save/load, undo/redo, host tempo change, sample-accurate switching where supported.

## Build and merge plan
1. Isolated feature branch and this specification before implementation.
2. Core generator header, executable tests and CMake wiring in distinct commits.
3. Compile with `cmake -S . -B build -DBR_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build --output-on-failure`. Target Linux/Windows, sanitizers when available.
4. UI integration as a separate PR after conflicts with the pending v0.0.2 PR are reviewed.
5. Rebase rather than overwrite ongoing PR #2. Merge only after CI success and feature review.
