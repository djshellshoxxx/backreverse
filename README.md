# BackReverse

BackReverse is a live and file-based reverse-audio instrument/effect for listening to records, speech, vocals, and complete tracks backwards in controllable chunks.

It builds as:
- Windows standalone application
- VST3
- CLAP

## Core behavior

For a 120-second track and a 10-second chunk, BackReverse outputs source regions:

```
10 -> 0
20 -> 10
30 -> 20
...
120 -> 110
```

The chunk order can then be rearranged independently.

A chunk equal to the loaded file duration is equivalent to whole-source reverse.

## Time engine

Each chunk supports:
- 0.25x
- 0.5x
- 1x
- 2x
- 3x
- arbitrary 0.05x–8x ratios

Rate mode changes pitch with speed, like changing tape or record speed.

Time Stretch mode changes duration while retaining local pitch using a granular overlap-add engine. Extreme settings are intentionally allowed for surreal textures.

## Performance controls

- fractional-second chunks
- deterministic random chunk order
- mixed chunk sizes
- per-chunk pan, stereo swap, polarity and phase coloration
- clickable 16-step gate lane
- per-gate forced-forward slices inside reversed chunks
- stutter
- delay
- echo
- reorderable effect chain in the DSP core
- loaded-file playhead/scrub control
- seed-based randomization
- VST/CLAP host state recall

## Live latency

Live reverse processing cannot output audio that has not arrived yet. A 5-second live reverse chunk therefore requires approximately five seconds of capture before that chunk can be heard. BackReverse reports the active chunk buffer as plug-in latency.

Whole-source reverse is available immediately for a fully loaded file because the end of the source is already known.

## Build

Requirements:
- CMake 3.22+
- C++20 compiler
- Git

Core tests only:

```bash
cmake -S . -B build -DBACKREVERSE_BUILD_PLUGIN=OFF -DBACKREVERSE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Full VST3 / CLAP / Standalone:

```bash
cmake -S . -B build -DBACKREVERSE_BUILD_PLUGIN=ON -DBACKREVERSE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

JUCE and clap-juce-extensions are fetched by CMake.

## Repository structure

- `include/backreverse` — format-independent DSP API
- `src/core` — reversal, sequencing, time processing and effects
- `src/plugin` — JUCE VST3/CLAP/Standalone adapter and GUI
- `tests` — deterministic DSP tests
- `specs` — normative product and engineering specifications

## Sound-design philosophy

BackReverse keeps reversal, chunk order, temporal transformation, stereo/phase movement, gating, and effects as separate dimensions. This permits combinations such as:

- reversed 500 ms chunks with every second chunk at half speed
- quarter-speed reverse fragments with selected forward gates
- deterministic random chunk order at triple speed
- independently moving stereo patterns
- stutter feeding ping-pong delay and drifting echo
- long reverse speech fragments for phonetic listening

## Current design decisions

- Phase rotation is implemented as frequency-dependent first-order all-pass coloration; polarity inversion is a separate operation.
- Rate mode uses interpolated resampling.
- Time Stretch mode uses granular overlap-add to retain local pitch.
- Delay and echo feedback are clamped below unity for numerical safety.
- Random behavior is seed-driven so offline rendering and recalled states can be repeatable.

See `specs/README.md` for the full normative specification.
