# BackReverse implementation audit — 2026-10-08 (release 0.0.2 beta)

Reviewed branch: `claude/modest-turing-nhyrx4` at 0.0.1 plus the changes in this release.

Scope: a full read of `src/core` (engine, FX, types, parameter table), `src/plugin` (processor, presets, editor, UI widgets), `tests/`, the CMake and CI configuration, and the normative specs 00–06. Every finding below was either reproduced by a test or probe, verified by reading the code path with the relevant numbers, or recorded as documented behaviour. Nothing here is a beta acceptance sign-off. Multi-host testing has not been done (see spec 08 §6).

Build and test environment: Ubuntu 24.04 container, GCC 13.3, CMake 3.28, Ninja, JUCE 8.0.4 and clap-juce-extensions built from source, Xvfb for GUI rendering. The VST3, CLAP and Standalone targets were built and the core and state tests passed on this release.

## 1. Findings fixed in this release

| # | Area | Finding | Evidence | Fix |
|---|---|---|---|---|
| A1 | Gate, engine | In Silence gap mode the gate gain was applied twice (pre-FX and post-FX), so shaped gates squared their envelope. A linear-in gate at mid-cell gave 0.125 for a 0.5 input instead of 0.25 | New test `gateEnvelopeAppliedOnce` fails on 0.0.1 logic, passes now | Envelope applied once to the FX input; a separate smoothed 0/1 gate (`postS`, 0.5 ms) closes FX tails in silent gaps |
| A2 | Gate, engine | A naive fix (1 ms silence ramp) reduced the energy of 64-step gates below the duty-cycle test's 0.4 floor (measured 0.395) | `gates` test, 64 steps | Silence ramp set to 0.5 ms; click-free and passes all 114 checks |
| A3 | Resampler, processor | `WindowedSincInterpolator::process` (4-argument form) reads filter taps past the end of the input for the last output samples | JUCE header: "must contain at least (speedRatio × numOutput) samples"; taps extend beyond | Use the 6-argument form with the true input length and zero-fill (`wrap = 0`) |
| A4 | Host blocks, processor | Host blocks longer than the size passed to `prepareToPlay` were processed in one call even though the live ring's slack and latency bound are sized for the prepared block | `Engine::prepare` sizes `ringCap` with `4 × maxBlock`; `Dcap` depends on `maxBlock` | `processBlock` slices into engine-sized pieces and advances sample and beat position for each slice |
| A5 | Editor, performance | The preset folder was scanned up to 30 times a second on the message thread | `tick()` called `presetNames()` every frame | Rescan about every 2 s; the 30 Hz tick only re-selects the current entry |
| A6 | Delay/Echo, performance | `OnePole::setHz` recomputed `exp()` every sample for every filter | `Delay::process` and `Echo::process` call `setHz` per sample | Coefficients cached and recomputed only when cutoff or rate changes. Output unchanged |
| A7 | Randomise | `Randomize` never chose the Custom gate shape (range 0–7 of 0–8) | `brp::gateShape` has nine choices | Range corrected to nine |
| A8 | UI, layout | The Variation Amount knob's value text was clipped at the bottom of its panel on the Patterns tab | Rendered Patterns tab, 0.0.1 layout | Size-variation panel height increased to fit the knob row |
| A9 | UI, labels | Long knob names were truncated ("Reverse Probabi…") | Rendered Chunks tab | Labels scale down to 72% width rather than cutting text |
| A10 | UI, gate curve | The curve editor caption overlapped the top control point | Rendered Gates tab | Caption moved to the bottom edge |
| A11 | UI, tabs | Tabs were plain labels with no active-state indication | Rendered tab bar | Accent underline on the active page, hover state and themed bar background |
| A12 | Version | The editor title tooltip hard-coded "0.0.1" and the version was spread over four files | `grep` for the version string | Tooltip uses `BR_VERSION_STRING`; single source in `CMakeLists.txt`; 0.0.2 in CI, installer and release notes |
| A14 | Threading | `SeqBox` copied its payload with plain `memcpy` under the sequence counter, a formal data race | Code review against the C++ memory model | Payload stored as relaxed atomic words with fenced sequence check; `static_assert` for trivially copyable payloads |
| A15 | Delay/Echo | Time smoothers started at 0 and glided to the set time, so the first echo arrived at the wrong time | `delayEchoStutter` impulse tests (tempo-sync and free-time echoes) | Smoothers prime to the target on first use after reset |
| A16 | Stutter | Forward repeats restarted one sample early (4799-sample repeats), so each repeat drifted one sample against the captured slice | Probe: repeat k+1 matched captured index +1 | Forward repeats restart at exactly one slice length |
| A13 | CI | The planned CLAP validator install (`cargo install clap-validator --version 0.3.2`) fails because that version is not on crates.io | `cargo install` returns "could not find clap-validator with version =0.3.2" | CI installs from the upstream git tag `0.3.2` |

## 2. Findings verified correct (no change)

| Area | Check | Result |
|---|---|---|
| Ring buffer bounds | `Src::at` range check and modulo indexing for live reads | Correct; reads outside `[lo, hi)` return zero |
| Scheduler arrays | `cStart[256]`, `cLen[256]`, `order[2·kMaxGroup+2]`, `fStarts[kMaxGroup+2]` indices | In range for all group sizes and order modes |
| Stutter buffer | capture length `len` vs `cap - 4`, repeat read index | Clamped; no out-of-range read |
| Delay and echo lines | Maximum read delay vs buffer size (delay 4000 ms in 4.2 s buffer; echo 2000 ms + modulation in 2.5 s) | Within buffer; reads clamp to `size - 3` |
| Feedback stability | Delay and echo loop gain and `tanh` limiter | Bounded; tested in `safetyAndFx` |
| Source hot-swap | Audio thread compares source pointers only before dereferencing; old sources are retired after the audio thread has advanced | Safe. Retirement on "idle" also safe because the pointer is not dereferenced until the swap |
| Pattern publication | `SeqBox` single writer, reader copies under sequence check | Correct for the single-writer case (see open item O1) |
| Parameter parsing | `brp::read` atomics, choice and bool conversion, `gateSteps = 2 << v` | Correct |
| Preset indices | Factory presets set parameters through the APVTS with valid choice indices | Correct |
| State restore | `NORM` values restore skewed float parameters bit-exactly; file restore is asynchronous on the message thread | Correct |
| Undo | All lane, gate, chain, curve and randomise edits use `UndoManager` transactions | Correct |
| Gate props menu | Menu IDs map to fields: direction 10–12, shape 21–29, width 41–50, depth 61–71, fades 80–95, FX 100–102 | Correct |
| Pan law | Linear pan with unity centre (centre preserves level per channel) | Correct, per the 0.0.1 audit |
| Live latency | Reported latency equals the chunk-size delay under the Dynamic and Fixed Maximum policies | Correct; tested in `latency` |
| Phase rotator | Magnitude flatness at 200 Hz, 1 kHz and 5 kHz; L/R offset of 90° at 1 kHz and 3 kHz | Verified by `phaseRotatorAllPass`; documented as all-pass in spec 00 §9 |

## 3. Documented behaviour (not changed)

| # | Area | Behaviour | Action |
|---|---|---|---|
| D1 | Phase rotator | At 0° the rotator is an all-pass with a frequency-dependent phase, not bit-transparent. The engine fades it in over 20 ms | Documented in spec 00 §9 and spec 08 §2. A fixed-latency FIR alternative would shift every file-mode output sample, which breaks the sample-exact reverse acceptance tests |
| D2 | Chunk size and LPF | `chunkUnit` changes only the display unit, not the scheduling | Intentional; the parameter is documented as a display unit |
| D3 | Standalone recording | Output is recorded at 24-bit WAV at the device rate | Intentional for this release |

## 4. Open items

| # | Area | Item | Risk | Plan |
|---|---|---|---|---|
| O1 | Threading | Closed by A14. ThreadSanitizer run in CI is still open under O2 | — | — |
| O2 | Threading | No allocation-hook or sanitiser build runs in CI | Unknown realtime regressions could slip in | Add a debug job with ASan and UBSan, and an allocation counter around `processBlock` |
| O3 | Hosts | No multi-host acceptance pass in this release | Host-specific state, automation or latency issues are unverified | Run the spec 06 §17 scenarios in at least one DAW per platform before a stable release |
| O4 | Long runs | No soak test | Memory stability over hours is unproven | Add a soak harness for spec 06 §19 |
| O5 | Features | Parallel buses and duplicate FX instances (spec 02 §9–10), stretch transient and formant controls (spec 01 §8) | Spec gap | Listed in spec 08 §7 |
| O6 | Tests | Stutter per-scope matrix (selected chunk and gate scopes) not yet enumerated | Partial coverage | Add to `tests/core_tests.cpp` |

## 5. Verification performed for this release

- `br_core_tests`: 124 of 124 checks pass, including the gate, phase, delay, ping-pong, tempo-sync, echo, feedback, stutter repeat and stutter reverse tests.
- `BackReverseStateTest`: passes.
- Full Linux build of `BackReverse_VST3`, `BackReverse_CLAP`, `BackReverse_Standalone`, `br_core_tests` and `BackReverseStateTest` on GCC 13.3 with no errors.
- GUI rendering: every editor tab was rendered under Xvfb from the built processor and editor, before and after the UI changes, and inspected for clipping and overlap.
- Standalone smoke test: the Linux standalone starts under a virtual display and stays running. Only ALSA sequencer warnings are printed because the container has no sound device.
- Runtime dependencies of the standalone, from `ldd`: `libasound.so.2`, `libfreetype.so.6`, `libfontconfig.so.1`, `libstdc++.so.6`, `libpng16`, `libexpat`, `libz`, `libbz2`, `libbrotli`. JUCE loads X11 client libraries at runtime.
- pluginval 5 (`--strictness-level 5 --skip-gui-tests`) on the Linux VST3: exit 0.
- clap-validator 0.3.2 on the Linux CLAP: 21 tests run, 16 passed, 0 failed, 5 skipped, exit 0. clap-validator is built from the upstream 0.3.2 git tag without its lockfile, because the pinned `time` crate does not compile on current rustc.
