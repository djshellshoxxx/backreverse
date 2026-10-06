# BackReverse — DSP Engine Specification

## 1. Processing graph

The shared DSP engine MUST use a modular graph:

1. Input conditioning
2. Capture/ring buffer
3. Chunk boundary scheduler
4. Reverse reader
5. Chunk-order sequencer
6. Rate/time-stretch processor
7. Pan/stereo processor
8. Phase/polarity processor
9. Gate sequencer
10. Stutter
11. Delay
12. Echo
13. Wet/dry and effect-chain router
14. Output safety

Stutter, Delay, and Echo MUST be reorderable. Pan/phase MAY be movable in the chain if the implementation remains deterministic.

## 2. Live reverse buffering

Live reversal MUST use preallocated circular/ring buffers.

For chunk duration C:
1. Capture C frames.
2. Freeze the completed region for playback.
3. Read that region in reverse.
4. Simultaneously capture the next C frames into a safe region.
5. Continue until bypass, stop, or reset.

The engine MUST correctly handle:
- chunks smaller than a host block
- chunks larger than a host block
- boundaries inside a host block
- arbitrary block sizes
- sample-rate changes
- non-power-of-two chunk sizes
- stereo and mono sources
- final partial chunks for files

## 3. Boundary treatment

Chunk boundaries MUST offer:
- Hard
- Micro Crossfade
- Equal-power Crossfade
- Zero-crossing assist where practical

Crossfade time MUST be adjustable and constrained so it cannot consume an unreasonable fraction of a short chunk.

Default behavior SHOULD suppress clicks without noticeably smearing transients.

## 4. Chunk metadata

Every scheduled chunk MUST have:
- chunk ID/index
- source start frame
- source end frame
- playback direction
- playback rate or stretch ratio
- pitch-preserve state
- order-pattern position
- random seed/sub-seed
- pan assignment
- phase/polarity assignment
- gate pattern assignment
- per-chunk effect assignments
- selection state

This metadata MUST be serializable when needed for preset/project recall.

## 5. Chunk-order engine

Required modes:
- Sequential
- Reverse Chunk Order
- Random
- Shuffle Without Immediate Repeat
- Ping-Pong
- Odds then Evens
- Evens then Odds
- Rotate Left
- Rotate Right
- Alternating A/B banks
- User Pattern

User patterns MAY contain relative chunk references, absolute references, repeats, rests, and probability tokens.

Examples:
- 0,1,0,2
- 3,2,1,0
- 0,2,1,3
- 0,0,1,REST,2,1

Pattern controls MUST include:
- loop
- one-shot
- seed
- regenerate
- freeze random result
- repeat probability
- skip probability
- maximum consecutive repeat
- pattern length
- pattern start offset

## 6. Mixed chunk sizes

Chunk Variation MUST allow:
- fixed size
- list of sizes
- weighted random sizes
- alternating sizes
- pattern-driven sizes
- host-synced sizes
- probability-selected sizes

Example sequence:
250 ms, 1 s, 125 ms, 2 s, 500 ms.

Changing chunk size MUST not corrupt existing buffered data.

## 7. Rate engine

Rate mode changes time and pitch together.

Required values:
- 0.25x
- 0.5x
- 1x
- 2x
- 3x
- arbitrary user value

Suggested implementation range: 0.05x to 8x.

Interpolation quality modes SHOULD include:
- Draft
- Normal
- High

The high-quality mode SHOULD minimize aliasing at large speed changes.

## 8. Time-stretch engine

Time-stretch changes duration independently of pitch.

Required controls:
- stretch ratio
- preserve pitch on/off
- algorithm/quality
- transient sensitivity if applicable
- formant preserve if supported
- reset phase/history at chunk boundary on/off
- crossfade between stretch states

The architecture MUST permit algorithm replacement without changing the host parameter contract.

Extreme stretching is an intentional creative mode; the product SHOULD preserve unusual textures instead of forcing transparent output at all settings.

## 9. Per-chunk temporal patterns

Each chunk MAY independently choose:
- rate mode
- stretch mode
- ratio
- pitch-preserve setting
- probability

Required assignment modes:
- same for all
- alternating
- cycle list
- random from set
- weighted random
- manual lane editing
- gate-follow
- pattern-follow

## 10. Scrub/playhead engine

The visible playhead MUST be user-draggable.

Scrub behavior:
- click/drag seeks playback position
- dragging backward plays backward
- dragging forward plays forward unless reverse-only lock is enabled
- velocity affects playback rate in vinyl mode
- zero movement MAY hold/freeze a micro-buffer
- release behavior options: latch, spring return, continue from release point

Modes:
- Linear Scrub
- Vinyl/Scratch
- Tape Shuttle
- Fine Scrub

Scratch mode SHOULD expose:
- inertia
- drag/friction
- acceleration curve
- maximum speed
- motor-return strength
- release ramp

No mode may access uninitialized samples.

## 11. Pan and stereo processing

Required operations:
- static pan
- swap L/R
- mirror pan
- auto-pan
- per-chunk pan
- random pan
- pattern pan

Pan pattern clock MAY be:
- reverse chunk clock
- independent time clock
- host musical clock
- gate clock

Pan transitions MUST be smoothable.

## 12. Polarity and phase

Polarity inversion MUST support:
- L only
- R only
- both
- alternating channels by chunk
- pattern assignment
- random assignment

True phase processing SHOULD support variable phase rotation or L/R offset through a stable all-pass or equivalent implementation.

Phase parameters MUST be clearly named so polarity inversion is not misrepresented as generic phase rotation.

## 13. Dry path and alignment

The dry path MUST have configurable latency behavior:
- uncompensated immediate dry
- latency-aligned dry

Wet/dry mixing SHOULD default to latency-aligned behavior when phase-coherent blending is expected.

## 14. Safety

DSP MUST:
- clamp or sanitize invalid parameter values
- prevent buffer overrun/underrun
- clear stale regions before reuse
- prevent NaN/Inf
- use click-free bypass
- avoid blocking synchronization in audio thread
- avoid dynamic allocation in audio thread
