# BackReverse — Testing and Acceptance Specification

## 1. Test philosophy

BackReverse MUST have automated tests for deterministic DSP behavior and explicit acceptance tests for interactive/audio behavior.

At minimum, testing MUST cover:
- reversal correctness
- chunk boundaries
- rate/stretch behavior
- scheduling
- random determinism
- gate behavior
- routing
- plug-in state
- latency
- edge cases

## 2. Reverse correctness tests

For known sample sequences, verify exact output ordering.

Example input:
0,1,2,3,4,5,6,7

Chunk size 4 MUST output:
3,2,1,0,7,6,5,4

Whole-source reverse MUST output:
7,6,5,4,3,2,1,0

Partial final chunk input:
0,1,2,3,4,5

Chunk size 4 MUST output:
3,2,1,0,5,4

## 3. Time examples

Acceptance tests MUST explicitly verify the product examples:

### 120-second source, 30-second chunks
Playback source intervals:
30->0
60->30
90->60
120->90

### 120-second source, 10-second chunks
Playback source intervals:
10->0
20->10
30->20
...
120->110

### Chunk equal to song length
A 180-second source with a 180-second chunk MUST be functionally equivalent to whole-source reverse.

## 4. Fractional chunk tests

Test chunk sizes including:
- 1 ms
- 7.5 ms
- 50 ms
- 125.5 ms
- 333.333 ms
- 0.5 s
- 1.25 s

Verify no dropped, duplicated, or uninitialized samples.

## 5. Host-block boundary tests

Run reversal using host blocks:
- 16
- 32
- 64
- 128
- 256
- 511
- 512
- 1024
- varying block sizes

Chunk boundaries MUST remain independent of host-block boundaries.

## 6. Rate tests

Verify fixed ratios:
- 0.25x
- 0.5x
- 1x
- 2x
- 3x

In Rate mode:
- output duration must scale as expected
- pitch must follow playback speed

In Time-Stretch mode:
- duration must follow selected stretch ratio
- pitch preservation must remain within algorithm-specific tolerance

## 7. Mixed temporal pattern tests

Pattern:
1x, 0.5x, 2x, 0.25x, 3x

Verify each consecutive chunk receives the correct ratio and that loop behavior repeats exactly.

## 8. Random determinism

For a fixed seed and identical input:
- chunk random order MUST repeat exactly
- rate randomization MUST repeat exactly
- gate randomization MUST repeat exactly
- pan/phase randomization MUST repeat exactly

Offline render and realtime render SHOULD match within expected floating-point/DSP tolerance.

## 9. Gate tests

Verify:
- every step count
- click toggle
- disabled cells
- shapes
- gap modes
- custom curve
- per-gate forward/reverse override
- effect assignment

A reversed parent chunk containing a force-forward gate MUST play that gate region forward.

## 10. Pan and phase tests

Verify:
- L/R swap
- mirrored pan
- static pan
- alternating pan
- polarity invert L
- polarity invert R
- polarity invert both
- phase processor stability

No pan/phase configuration may generate NaN/Inf.

## 11. Stutter tests

Verify:
- global
- per chunk
- selected chunk
- selected gate
- random/pattern assignments
- wet-only
- dry-only
- mixed wet/dry
- direction modes

## 12. Delay and echo tests

Verify:
- free-time
- tempo sync
- feedback stability
- ping-pong
- filtering
- wet/dry
- selected chunk/gate assignments
- effect reordering

Feedback controls MUST be constrained or protected against numerical runaway.

## 13. Effect-chain tests

At minimum test:
- Stutter -> Delay -> Echo
- Echo -> Stutter -> Delay
- Delay -> Echo -> Stutter

Changing chain order MUST audibly and mathematically alter routing rather than only changing UI state.

## 14. Scrub tests

Verify:
- seek
- forward drag
- backward drag
- stationary/freeze behavior
- release modes
- scratch velocity response
- no access outside valid sample region

Rapid alternating scrub direction MUST not crash or output uninitialized memory.

## 15. Latency tests

For live reverse:
- verify first reversed output cannot precede availability of required future samples
- verify displayed latency corresponds to selected buffer policy
- verify aligned dry path
- verify fixed-maximum latency remains stable while changing chunk size within the declared maximum

## 16. State tests

Save and reload state while using:
- complex chunk patterns
- rate/stretch patterns
- gate patterns
- random seed
- effect chain
- pan/phase patterns

The recalled state MUST produce equivalent behavior.

## 17. Plug-in validation

VST3 MUST pass applicable validator tooling.

CLAP SHOULD pass available CLAP validation/smoke testing.

Test in multiple DAW hosts before beta.

Required host scenarios:
- insert effect
- save/reopen project
- automation
- tempo change
- transport seek
- loop
- offline render
- freeze/bounce

## 18. Realtime-safety tests

Use instrumentation/debug builds to detect:
- allocations in audio callback
- mutex/blocking waits
- file I/O from audio thread
- buffer overrun
- use-after-free
- race conditions where practical

## 19. Long-run tests

Run continuous processing for at least several hours with:
- live input
- short chunks
- long chunks
- randomized patterns
- maximum gate count
- chained effects

Memory usage MUST remain stable.

## 20. 1.0 acceptance criteria

BackReverse 1.0 is not complete until:
- all three targets build
- reversal math passes automated tests
- fractional chunks work
- whole-source reverse works
- live chunk reverse works
- chunk reordering works
- rate and stretch modes work
- 1/4x, 1/2x, 1x, 2x, 3x work
- per-chunk temporal patterns work
- gate editor works
- per-gate direction works
- stutter works
- delay works
- echo works
- effect chaining works
- pan reversal/patterns work
- polarity and phase features work as documented
- scrub/scratch works
- state restoration is deterministic
- random seed recall works
- latency behavior is documented and tested
- no known critical crash/data-corruption bugs remain
