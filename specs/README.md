# BackReverse Specifications

This directory defines the required behavior of BackReverse.

## Specification set

- [00-product-overview.md](00-product-overview.md) — product behavior, chunk semantics, time manipulation, source handling, phase/pan terminology, targets
- [01-dsp-engine.md](01-dsp-engine.md) — buffers, chunk scheduler, reversal, temporal processing, scratch/playhead, pan/phase DSP
- [02-gate-effects-routing.md](02-gate-effects-routing.md) — gate sequencer, per-gate direction, Stutter, Delay, Echo, effect chaining
- [03-ui-ux.md](03-ui-ux.md) — waveform, chunk editor, visual gates, randomization, tooltips, help and interaction
- [04-plugin-standalone-host.md](04-plugin-standalone-host.md) — VST3, CLAP, standalone, transport, synchronization, latency and rendering
- [05-parameters-presets-state.md](05-parameters-presets-state.md) — stable parameters, automation, presets, deterministic random state and migration
- [06-testing-acceptance.md](06-testing-acceptance.md) — unit, DSP, host, realtime, state and release acceptance tests
- [07-release-packaging.md](07-release-packaging.md) — release identity, Windows and Linux artifacts, build requirements, validation gates, publishing rules
- [08-implementation-status.md](08-implementation-status.md) — requirement-by-requirement status matrix with the open items that gate 1.0
- [09-feature-specifications.md](09-feature-specifications.md) — developer specifications for the v0.1 roadmap features F-01 to F-16
- [10-implementation-plan.md](10-implementation-plan.md) — phased plan, branch and merge rules, per-feature test strategy, definition of done
- [11-engineering-requirements.md](11-engineering-requirements.md) — compatibility and realtime rules, golden renders, CI gates, external prerequisites, decisions to record
- [implementation-audit-2026-10-06.md](implementation-audit-2026-10-06.md) — earlier audit and its resolution
- [implementation-audit-2026-10-08.md](implementation-audit-2026-10-08.md) — audit for the 0.0.2 beta: findings, fixes and open items

## Normative language

- MUST = required for the specified release/capability
- SHOULD = strongly recommended and expected unless an implementation constraint is documented
- MAY = optional

## Core behavior summary

BackReverse does not reverse the order of chunks by default. It reverses the samples within each chronological chunk.

For a two-minute source with 10-second chunks:

10 -> 0
20 -> 10
30 -> 20
...
120 -> 110

Chunk order itself can then be independently rearranged by the sequencing system.

## Temporal manipulation

Each chunk can independently use:
- quarter-time (0.25x)
- half-time (0.5x)
- normal (1x)
- double-time (2x)
- triple-time (3x)
- arbitrary playback-rate ratios
- arbitrary time-stretch ratios

Rate mode changes pitch with speed.

Time-Stretch mode changes duration while attempting to preserve pitch.

These settings can be sequenced, randomized, weighted, or manually assigned per chunk.

## Recommended implementation order

### Phase 1 — Core
1. Shared project/build system
2. Audio file loader
3. Ring/capture buffer
4. Sequential Chunk Reverse
5. Whole Source Reverse
6. Automated reversal tests

### Phase 2 — Temporal engine
1. Rate processor
2. 0.25x / 0.5x / 1x / 2x / 3x
3. Arbitrary rate
4. Time-stretch interface
5. Per-chunk temporal pattern

### Phase 3 — Sequencing
1. Chunk metadata
2. Reorder patterns
3. Deterministic PRNG
4. Mixed chunk sizes
5. Randomization/freeze

### Phase 4 — Performance
1. Movable playhead
2. Scratch/vinyl mode
3. Pan/stereo patterns
4. Polarity/phase patterns

### Phase 5 — Gate and FX
1. Visual gate sequencer
2. Per-gate forward/reverse
3. Stutter
4. Delay
5. Echo
6. Chain editor

### Phase 6 — Product formats
1. Standalone audio device layer
2. VST3 adapter
3. CLAP adapter
4. Host state/automation
5. latency reporting

### Phase 7 — Release
1. Complete UI
2. Tooltips/help
3. Factory presets
4. plugin validators
5. multi-host tests
6. installers
7. beta acceptance pass

## Design principle

The audio engine must treat reversal, chunk ordering, time manipulation, gating, stereo/phase manipulation, and effects as separate composable dimensions. This avoids hard-coding one effect into another and allows combinations such as:

- reversed 500 ms chunks with every second chunk at half speed
- randomized chunk order with pitch-preserving 3x compression
- quarter-speed reverse chunks containing selected forward gates
- pan patterns running independently from reverse chunk boundaries
- stutter on selected gates followed by delay and tape-style echo
- vinyl-style manual scrubbing through a previously generated reverse pattern
