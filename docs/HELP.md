# BackReverse Quick Help

## Reverse chunking

BackReverse waits for one chunk of live audio, then reads that chunk backwards.

A 10-second setting means:
- capture 0–10 seconds
- play 10–0
- capture 10–20 while the first reverse chunk is heard
- continue through the source

For loaded files, the complete source is already available.

## Rate versus Time Stretch

**Rate** behaves like tape or vinyl speed. Slower playback lowers pitch; faster playback raises pitch.

**Time Stretch** changes duration while attempting to keep pitch stable. BackReverse deliberately permits extreme ratios because granular artifacts can be musically useful.

Quick ratios:
- 0.25x = quarter speed
- 0.5x = half speed
- 1x = unchanged duration
- 2x = double speed
- 3x = triple speed

## Gate lane

The 16 numbered buttons enable or disable gate cells.

The small **F** control forces that gate to play forward even when its parent chunk is reversed. This is useful for breaking up intelligible reversed passages with small forward fragments.

## Stereo and phase

**Swap L/R** exchanges the stereo channels.

**Polarity** multiplies the selected channel by -1. This is not the same as time-shifting phase.

**Phase Rotation** uses all-pass processing to alter frequency-dependent phase without simply changing polarity.

## Effects

**Stutter** repeats short regions. The engine supports alternate-direction repeats.

**Delay** is the clean repeat line and supports ping-pong routing.

**Echo** is the more colored repeat stage with damping, drift, and wow/flutter behavior.

The DSP engine treats Stutter, Delay, and Echo as a reorderable chain.

## Scrub / playhead

Load a file and drag the horizontal playhead. Releasing at a different position seeks the source. The DSP core also exposes interpolated sample access used by scratch/vinyl-style motion.

## Randomization

Randomize creates a new seed, gate pattern, forward-gate pattern, and speed value. The seed is saved in plug-in state so the result can be recreated.

## Live latency

Reverse playback needs future samples. The active chunk duration therefore determines the fundamental live buffering delay.

The UI shows the active reverse buffer and the host plug-in reports the same chunk length as latency.

## Whole Source

For a loaded file, Whole Source reads from the last sample toward the first. This is the traditional complete-record-backwards mode.

## Safety

BackReverse clamps feedback, speed ratios, pan, phase and invalid output values to keep extreme settings bounded.
