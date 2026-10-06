# BackReverse — Gate, Stutter, Delay, Echo, and Routing Specification

## 1. Visual gate sequencer

BackReverse MUST include a visual, directly clickable gate editor.

Each gate cell MUST show:
- enabled/disabled state
- width/duration
- gap before or after the cell
- shape
- assignment state
- whether reversal is enabled for that gate
- effect badges/indicators where practical

Click toggles gate on/off. Drag gestures SHOULD support painting multiple gates.

## 2. Gate count and timing

Gate sequencer MUST support configurable step counts, at minimum:
- 2
- 4
- 8
- 16
- 32
- 64

Timing modes:
- fit inside current chunk
- fixed milliseconds
- host note values
- independent free-running clock

## 3. Gate shapes

Required amplitude shapes:
- Hard/Rectangular
- Linear Fade In
- Linear Fade Out
- Triangle
- Equal-power pulse
- Sine
- Exponential
- Logarithmic
- Custom user curve

Each gate MUST expose:
- attack/entry shape
- sustain width
- release/exit shape
- gap length
- depth

The custom curve editor SHOULD use draggable points.

## 4. Per-gate reversal

Every gate MUST have a reversible toggle.

Modes:
- inherit parent chunk direction
- force reversed
- force forward

This enables forward slices inside reversed chunks and reversed slices inside forward material.

## 5. Gap behavior

Gate gap MUST be independently adjustable.

Gap modes:
- silence
- dry-through
- hold previous sample
- crossfade
- effect-tail only

Gap duration MAY be expressed as:
- percentage of gate cell
- milliseconds
- musical subdivision

## 6. Stutter

Stutter MUST support:
- global
- per chunk
- selected chunks
- selected gates
- random assignment
- pattern assignment

Controls:
- rate/period
- repeats
- repeat length
- decay
- pitch/rate drift
- direction
- retrigger mode
- probability
- wet
- dry

Stutter direction modes:
- forward
- reverse
- alternate
- inherit chunk

Wet and dry MUST be independently controllable.

Stutter MAY be placed before or after Delay and Echo.

## 7. Delay

Delay MUST support:
- milliseconds
- host-synced note values
- stereo linked/unlinked time
- feedback
- cross-feedback
- low-pass/high-pass feedback filtering
- ping-pong
- modulation
- wet/dry
- freeze/hold if implemented

Assignment scopes:
- global
- per chunk
- selected chunks
- selected gates
- pattern
- probability/random

## 8. Echo

Echo is a deliberately characterful repeat effect distinct from general delay.

Minimum controls:
- echo time
- repeat count or feedback
- decay
- tone/damping
- stereo spread
- drift
- wow/flutter amount
- wet/dry

Optional character modes:
- Clean
- Tape
- Vinyl
- Lo-Fi
- Dub

Echo MUST use the same assignment scopes as Delay.

## 9. Effect chaining

Stutter, Delay, and Echo MUST be chainable.

Minimum chain editor:
- drag to reorder
- enable/disable each effect
- per-effect wet/dry
- chain wet/dry
- duplicate effect instance if architecture allows

Example chains:
- Stutter -> Delay -> Echo
- Echo -> Stutter -> Delay
- Delay -> Echo -> Stutter

Per-chunk and per-gate assignments MUST reference chain presets or effect-instance IDs rather than duplicating expensive DSP unnecessarily.

## 10. Parallel routing

The engine SHOULD support parallel buses:

- Dry
- Reverse
- Gate
- FX A
- FX B

At minimum, users MUST be able to blend dry and processed audio without phase discontinuities.

## 11. Click suppression

Gate, bypass, chain-order, and assignment changes MUST be smoothed.

No UI toggle may produce uncontrolled full-scale discontinuities.
