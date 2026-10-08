# BackReverse — Implementation Status Matrix

This document maps each normative requirement in specs 00–07 to its current implementation status, the code that implements it, and what is still required before the 1.0 criteria (spec 06 §20) can be met. It is updated with every release and with each audit (see `implementation-audit-*.md`).

Status values:
- **Done** — implemented and covered by an automated test or a documented manual check.
- **Done (undocumented test)** — implemented; coverage exists but is not yet a named acceptance test.
- **Partial** — implemented for some modes or parameters; gaps are listed.
- **Not implemented** — required by spec but absent. Release notes must list it as a limitation while it stays open.
- **Not applicable (this release)** — explicitly out of scope for the targeted release.

Code references use `src/core/Engine.cpp` (engine), `src/core/Fx.h` (effects), `src/plugin/*` (format adapter and UI), `tests/core_tests.cpp` (DSP acceptance).

## 1. Product, reversal model and chunk timing (spec 00)

| Requirement | Status | Implementation / gap |
|---|---|---|
| Sequential Chunk Reverse, chronological windows (§3) | Done | `Engine::fillVoice`, `Engine::fileNext`, `Engine::liveNext`; tested in `reverseCorrectness`, `productExamples` |
| Whole Source Reverse (§4.2) | Done | `RevMode::WholeSource`; file only, live requires capture |
| Reordered Chunk Reverse (§4.3) | Done | `Engine::buildOrder` with 11 orders and the user pattern language |
| Free Scrub Reverse (§4.4) | Done | `Engine::process` scrub section; tested in `scrub` |
| Hybrid (live playhead interrupts, then return/latch/continue) (§4.5) | Done | Scrub release modes `Latch`, `Spring`, `Continue` |
| Chunk length in ms, seconds, note values, samples (§5) | Done | `baseChunkFrames`, `chunkSync`, `chunkUnit` (display unit only) |
| 1 ms to 10 minutes, fractional values (§5) | Done | `chunkMs` range 1–600000; tested in `fractionalChunks` |
| Parameter changes smoothed or boundary-quantised (§5) | Partial | Boundary crossfades quantise changes; `chunkMs` changes take effect at the next regrid. Not measured for clicks |
| Rate (tape) and Time-Stretch modes (§6) | Done | `TimeMode`, `renderVoice`, `grainTick`; tested in `rateAndStretch` |
| Per-chunk temporal patterns, all assignment modes (§6, spec 01 §9) | Done | `TempoAssign` (7 modes) and the Speed Lane |
| Seed lock for repeatable arrangements (§6) | Done | `Patterns`, `P.seed`, `P.freeze`; tested in `determinism` |
| Latency displayed and reported; Dynamic and Fixed Maximum policies (§7) | Done | `computeLatency`, `LatPolicy`; tested in `latency` |
| Standalone and plug-in share buffering semantics (§7) | Done | Single engine for all formats |
| File, live input, capture, drag-and-drop, file dialog (§8) | Done | `BackReverseProcessor::loadFile`, `startCapture`, `filesDropped` |
| File loading resamples to device rate (§8, audit fix) | Done (undocumented test) | `makeSource` uses `WindowedSincInterpolator`; fixed in 0.0.2 to zero-fill past the end |
| Stereo and mono input handling (§8) | Done | `isBusesLayoutSupported`, mono input duplicated to stereo |
| Polarity, Stereo/Pan, Phase as distinct concepts (§9) | Done | `PolMode`, `PanMode`/`SwapMode`, `PhaseMode` with clearly named parameters |
| Variable phase documented as all-pass based (§9) | Done | See spec 00 §9 implementation note and spec 01 §12 |
| Processing scopes: Global, Every, Alternating, Selected, Pattern, Random, Gates (§10) | Done | `Scope`, `RevScope`, `fxAssigned`; "Selected chunks" is the Chunk Lane |
| Pan/phase timing: chunk-locked, independent, parallel, offset (§10) | Done | `PanClock`, `panOffset`, `panClockMs` |
| Non-destructive real-time processing (§11) | Done | All processing is on playback; the source is never modified |
| Realtime safety: no allocation, I/O or blocking locks in audio (§13) | Done (verified by reading) | Parameters are atomics, patterns use `SeqBox`, sources are retired after the audio thread moves on. Not yet run under a sanitiser in CI |
| NaN/Inf never reach output (§13) | Done | `process` output sanitising; tested in `safetyAndFx` |

## 2. Reverse, rate and time-stretch engine (spec 01)

| Requirement | Status | Implementation / gap |
|---|---|---|
| Processing graph order (§1) | Done | `Engine::process`: input, ring, scheduler, reverse, order, rate, pan, polarity, phase, gate, FX chain, mix, output safety |
| Live reverse buffering, arbitrary block sizes (§2) | Done | `tests/core_tests.cpp` `blockSizes`; host blocks larger than the prepared size are sliced in the plug-in adapter (0.0.2) |
| Boundary modes: Hard, Micro, Equal-Power, Zero-Cross (§3) | Done | `Xfade`; crossfade capped at 25% of a chunk |
| Chunk metadata serialisable (§4) | Partial | Scheduler state is derived deterministically from parameters and the seed, so it is not stored per chunk. Equivalent behaviour is covered by state tests |
| Chunk-order modes incl. Shuffle without immediate repeat (§5) | Done | `Order::Shuffle` (permutation with a no-repeat guard across groups) |
| User pattern: relative, absolute, repeats, rests, probability (§5) | Done | `parseUserPattern` |
| Pattern controls: loop, one-shot, seed, regenerate, freeze, repeat/skip, max repeat, length, offset (§5) | Done | All present as parameters |
| Mixed chunk sizes: fixed, list, weighted, alternating, pattern, synced, probability (§6) | Done (probability by weight) | `SizeMode`; "probability-selected" is implemented as weighted random |
| Rate values 0.25/0.5/1/2/3 and 0.05–8× (§7) | Done | `ratio` range 0.05–8; tested in `rateAndStretch` |
| Interpolation Draft / Normal / High (§7) | Done | `Quality`; High is windowed-sinc above 1× |
| Time-stretch: ratio, preserve pitch, algorithm, reset at boundary (§8) | Done | `StretchAlgo` (4), `stretchReset`, `preserve` |
| Time-stretch: transient sensitivity, formant preservation (§8) | Not implemented | Listed as a beta limitation |
| Time-stretch: crossfade between stretch states (§8) | Not implemented | Boundary crossfade applies to playback, not to stretch state |
| Stretch algorithm replaceable without changing host contract (§8) | Done | `StretchAlgo` is internal; parameter IDs unchanged |
| Per-chunk temporal assignment modes (§9) | Done | See §1 above |
| Scrub: linear, vinyl, shuttle, fine; release modes; inertia, friction, acceleration, max speed, motor, ramp (§10) | Done | `ScrMode`, `ScrRelease`, scrub section of `process` |
| No access to uninitialised samples during scrub (§10) | Done | `Src::at` returns zero outside the valid range; tested in `scrub` |
| Pan: static, swap, mirror, auto, per-chunk, random, pattern (§11) | Done | `PanMode` (6) and `SwapMode` (3) |
| Pan clock: reverse chunk, independent, host musical, gate (§11) | Done | `PanClock` |
| Polarity: L, R, both, alternating, pattern, random (§12) | Done | `PolMode` (7); tested in `panPhase` |
| Phase rotation or L/R offset, all-pass (§12) | Done (documented) | Hilbert all-pass pair, flat magnitude, exact L/R offset; 0° is a fixed frequency-dependent all-pass colouration. Documented in spec 00 §9; tested in `phaseRotatorAllPass` |
| Dry path: immediate or latency-aligned (§13) | Done | `DryAlign` |
| Clamp invalid parameters; prevent overrun; clear stale regions; click-free bypass (§14) | Done | `brp::read`, ring-bounds in `Src::at`, `bypassS` smoothing |

## 3. Gates, stutter, delay, echo and routing (spec 02)

| Requirement | Status | Implementation / gap |
|---|---|---|
| Visual gate editor: click toggle, drag paint, right-click properties (§1, §8) | Done | `GateEditor` |
| Step counts 2, 4, 8, 16, 32, 64 (§2) | Done | `gateSteps` choice; tested in `gates` |
| Timing: fit chunk, fixed ms, host note, free-running (§2) | Done | `GateTiming` (4) |
| Shapes: hard, linear in/out, triangle, equal-power, sine, exponential, logarithmic, custom (§3) | Done | `Shape` (9); tested in `gates` |
| Custom curve with draggable points (§3) | Done | `CurveEditor` |
| Per-gate attack/release fades, width, depth, gap (§3) | Done | `GateStep` fields |
| Per-gate reverse toggle: inherit, force reverse, force forward (§4) | Done | `GateStep::dir`; tested in `gates` |
| Gap modes: silence, dry-through, hold, crossfade, FX tail (§5) | Done | `GapMode`; silence no longer squares the gate envelope (0.0.2) |
| Gap length as % of cell, ms, or musical subdivision (§5) | Partial | Width is a percentage of the cell; gap length is the complement. Millisecond gap entry is not exposed |
| Stutter scopes, controls and direction modes (§6) | Done | `Stutter`, `StutDir`, `Retrig` |
| Wet and dry independent (§6) | Done | `stutWet`, `stutDry`; tested in `safetyAndFx` |
| Delay: ms, sync, stereo link, feedback, cross-feedback, filters, ping-pong, modulation, wet/dry, freeze (§7) | Done | `Delay` |
| Echo with character modes and the minimum control set (§8) | Done | `Echo`, `EchoChar` (5) |
| Feedback protected against runaway (spec 06 §12) | Done | `tanh` limiter and loop gain cap in `Delay` and `Echo` |
| Effect chain reorder, enable/disable, per-effect and chain wet/dry (§9) | Done | `chainOrder` (6 permutations), `ChainStrip` |
| Duplicate effect instance (§9) | Not implemented | Architecture allows it; not exposed |
| Per-chunk and per-gate assignment by reference (§9) | Done | Scopes reference the shared FX instances |
| Parallel buses: Dry, Reverse, Gate, FX A, FX B (§10) | Not implemented | Dry blend is provided by Wet/Dry and the dry path. Separate buses are not |
| Click suppression for gate, bypass, chain order, assignment (§11) | Done | `Smooth` on gate, send, bypass and chain-enable paths |

## 4. Interface (spec 03)

| Requirement | Status | Implementation / gap |
|---|---|---|
| Waveform with chunk overlays, playhead, zoom, scroll, drag scrub (§3) | Done | `Waveform` |
| Shift fine scrub, wheel zoom, drag selection (§3) | Done | `Waveform::mouseDrag`, `mouseWheelMove` |
| Live visualisation: write/read heads, buffered amount, latency (§4) | Done | `Waveform` live branch |
| Chunk inspector with batch editing (§5) | Partial | Batch edit of lane steps and inspector readout exist. Start/end cannot be edited numerically |
| Rate quick buttons 1/4, 1/2, 1, 2, 3 and continuous ratio (§6) | Done | Time page buttons and `Speed` knob |
| Pattern editor: add, remove, reorder, duplicate, randomise, seed, freeze, clear, save, load (§7) | Done | `PatternEditor` |
| Gate editor visual state and FX badges (§8) | Done | `GateEditor::paint` |
| Scratch UI: grab-capable playhead, vinyl feedback (§9) | Done | `Platter`, waveform playhead |
| Randomise: amount, seed, lock, reroll, undo, domain exclusion (§10) | Done | Random page; all undoable |
| Tooltips on non-obvious controls (§11) | Done | `tooltipFor`, `setTooltip` throughout |
| Help covering quick start, reversal, latency, chunks, rate vs stretch, polarity vs phase, gate, scratch, routing, shortcuts (§11) | Done | `HelpText.h` topics |
| Multi-level undo for editable state (§12) | Done | `UndoManager` for lanes, gates, chain, randomise, presets |
| Accessibility: scalable UI, keyboard navigation, visible focus (§13) | Partial | UI scales by transform; keyboard works on pattern, gate, lane and waveform. Screen-reader labels are set on the main controls only |
| Modern visual design: tabs, typography, consistent panels (spec 03 §1) | Done (0.0.2) | Tab underline style, label scaling, layout fixes; verified by rendering every tab |

## 5. Host, standalone and state (specs 04, 05)

| Requirement | Status | Implementation / gap |
|---|---|---|
| VST3, CLAP and Standalone build from one core (§1) | Done | `src/plugin/CMakeLists.txt` |
| Stereo effect, automation, tempo, transport, state, latency (§2, §3) | Done | `PluginProcessor`, `juce_add_plugin`, `clap_juce_extensions_plugin` |
| Standalone device, sample rate, buffer, channel selection (§4) | Done | JUCE standalone Options menu |
| File transport: play, pause, stop, seek, loop, return, whole and sequential reverse (§5) | Done | `Engine::ctl`, `Content` transport buttons |
| Host sync: tempo-synced sizes, gate clock, delay/echo sync, restart rules (§6) | Done | `Restart`, `HostInfo`; host-play and bar/beat restarts |
| Host transport edge cases: stop, seek, loop, tempo change, offline (§7) | Partial | Seek and offline are handled; long-running behaviour under DAW loops is not yet verified in a host |
| Offline render deterministic (§9) | Done (undocumented test) | `determinism` test; render-to-WAV path in `startRender` |
| Linux and Windows builds (spec 04 §11, spec 07) | Done (0.0.2) | Linux build verified in this release; Windows build from CI |
| macOS | Not applicable (this release) | Spec 04 §11 allows later support |
| Parameter IDs stable; display names may change (spec 05 §1) | Done | `Params.h` table; IDs never reused |
| Automation smoothing (spec 05 §3) | Done | `Smooth` on gains, pan, polarity, phase, gate |
| Presets save and recall full state, no embedded audio (spec 05 §4) | Done | `stateTree`, `saveUserPreset`, `loadPreset` |
| Deterministic random state from seed (spec 05 §5) | Done | `hashUnit`, `Rng`; tested in `determinism` |
| State versioning: format, product, schema versions (spec 05 §6) | Done | `formatVersion`, `productVersion`, `schemaVersion` |
| Migration path from older state (spec 05 §6) | Partial | Hook present (`applyStateTree`); no older schema exists yet |
| Factory presets: all 15 categories (spec 05 §7) | Done | `Presets.cpp` (18 entries: all 15 categories plus Init, Tape-stop chunks and Cycle lane) |
| Copy/paste sub-presets: chunk, rate, gate, pan/phase, FX (spec 05 §8) | Done | `copySection`, `pasteSection` |

## 6. Testing and acceptance (spec 06)

| Area | Status | Notes |
|---|---|---|
| Reverse correctness (§2, §3) | Done | Chunk, whole-source, partial and 120 s examples |
| Fractional chunks (§4) | Done | 1 ms to 1.25 s set |
| Host-block boundary tests (§5) | Done | 16 to 1024 and varying sizes |
| Rate and stretch (§6) | Done | Ratios and pitch tolerance |
| Mixed temporal pattern (§7) | Done | 1x, 0.5x, 2x, 0.25x, 3x |
| Random determinism (§8) | Done | Repeat, seed sensitivity, block-size independence |
| Gate tests (§9) | Done | Step counts, disabled cells, shapes, gaps, forward override, single envelope application |
| Pan and phase (§10) | Done | Swap, mirror, polarity, all-pass magnitude, L/R offset |
| Stutter (§11) | Partial | Dry-only and bounded output tested. Per-scope and direction-mode matrix not fully enumerated |
| Delay and echo (§12) | Partial | Feedback stability and finiteness tested. Tempo sync and ping-pong numeric tests not yet written |
| Effect-chain routing (§13) | Done | Chain order changes output |
| Scrub (§14) | Done | Seek, drag, release, rapid direction changes |
| Latency (§15) | Done | Availability, displayed latency, aligned dry, fixed maximum |
| State round trip (§16) | Done | `BackReverseStateTest` |
| pluginval, clap-validator (§17) | Done (this release) | See spec 07 §5 for results |
| Multi-host tests (§17) | Not implemented | Required before stable; beta notes must list hosts actually tested |
| Realtime-safety instrumentation (§18) | Not implemented | No sanitiser or allocation-hook build in CI yet |
| Long-run stability (§19) | Not implemented | Requires a soak harness; planned |

## 7. Open items, ordered by release impact

1. Multi-host acceptance pass (spec 06 §17) on Windows and Linux DAWs.
2. Realtime-safety instrumentation in CI (ThreadSanitiser build of the core test, allocation hook on the audio thread).
3. Long-run soak test with live input and maximum gate count (spec 06 §19).
4. Stretch: transient sensitivity, formant preservation, stretch-state crossfade.
5. Parallel buses (spec 02 §10) and duplicate FX instances (spec 02 §9).
6. Delay and echo numeric acceptance tests (spec 06 §12).
7. macOS targets (spec 04 §11).
