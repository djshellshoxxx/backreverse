# BackReverse — Feature Specifications (v0.1 roadmap)

Developer-ready specifications for the features proposed after 0.0.2 beta. Each feature has an ID (`F-xx`), a priority, scope, functional requirements (MUST / SHOULD / MAY, same meaning as specs/README.md), the code it touches, realtime constraints, tests and acceptance criteria. The implementation order and merge rules are in `10-implementation-plan.md`. Cross-cutting rules (parameter IDs, state schema, golden tests) are in `11-engineering-requirements.md`.

Priorities: **P0** blocks a public release. **P1** high value. **P2** polish or research-heavy.
Size: **S** ≈ 1–3 dev-days, **M** ≈ 4–10, **L** ≈ 2–4 weeks. These are planning estimates for one experienced C++/JUCE developer.

Common constraints for every feature:
- Audio thread: no allocation, no locks, no file or UI calls, no `setValueNotifyingHost` (spec 05 §3, spec 01 §13).
- Host parameter IDs are immutable. New parameters are appended to the end of `BR_PARAMS` in `src/plugin/Params.h` and never reused.
- Any change to saved state bumps `schemaVersion` (currently 1) and adds a migration test (spec 05 §6).
- Each feature ships with its own tests in `tests/` and the golden-render checks in spec 11 §3.

---

## F-01 macOS build (universal binary) — P0, S

**Goal:** Standalone, VST3 and CLAP for macOS, built as a universal binary (arm64 + x86_64), so the product is not limited to Windows and Linux.

**Scope in:** CMake configuration for macOS, CI job, packaging as a `.dmg` or `.zip`.
**Scope out:** Audio Unit (F-02), signing (F-03), Mac App Store.

Requirements:
- MUST build `BackReverse_VST3`, `BackReverse_CLAP`, `BackReverse_Standalone` for `arm64;x86_64` in one configuration.
- MUST set `CMAKE_OSX_DEPLOYMENT_TARGET` to a decided minimum (proposal: `11.0`; record the decision in `11-engineering-requirements.md` §6).
- MUST pass `pluginval --strictness-level 5 --skip-gui-tests` on the VST3 bundle.
- MUST pass `clap-validator validate ... --only-failed` on the CLAP bundle with zero failures.
- MUST keep `NSMicrophoneUsageDescription` (already set through `MICROPHONE_PERMISSION_ENABLED TRUE`) so capture permission prompts are correct.
- SHOULD include `docs/RELEASE_NOTES.md` macOS install steps (copy VST3 to `~/Library/Audio/Plug-Ins/VST3`, CLAP to `~/Library/Audio/Plug-Ins/CLAP`, drag the standalone app to Applications).

Files:
- `CMakeLists.txt`: platform switch for `CMAKE_OSX_ARCHITECTURES` and deployment target.
- `src/plugin/CMakeLists.txt`: no logic change expected; add `COPY_PLUGIN_AFTER_BUILD FALSE` stays.
- `.github/workflows/build.yml`: new `macos` job on `macos-14` (Xcode generator), validators, packaging.
- `installer/` : not required for macOS in this feature.

Tests:
- CI: build, `ctest`, pluginval, clap-validator on macOS.
- Manual: load VST3 and CLAP in Reaper (macOS) and the standalone on Apple Silicon and Intel (if hardware is available; otherwise Rosetta run on arm64 is acceptable for x86_64 slice verification via `lipo -info`).

Acceptance:
- `lipo -archs` on each Mach-O binary reports `x86_64 arm64`.
- Standalone opens a file, plays, and records to WAV without permission errors after the mic prompt is accepted.
- No validator failures; no new warnings from `-Wall -Wextra` in `src/core`.

Risks: JUCE's macOS SDK requirements (Xcode version pinned by the runner image); universal builds double build time (use Ninja with `-j` and cache the JUCE build).

---

## F-02 Audio Unit (AU) plug-in — P0, M

**Goal:** Ship the processor as an Audio Unit effect (`aufx`) so Logic Pro, GarageBand and other AU hosts can load BackReverse.

Requirements:
- MUST add `AU` to `FORMATS` in `src/plugin/CMakeLists.txt` `juce_add_plugin`. Keep `IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`.
- MUST set `AU_MAIN_TYPE kAudioUnitType_Effect` (the default for non-synth plug-ins; set explicitly to avoid ambiguity).
- MUST keep the existing `PLUGIN_MANUFACTURER_CODE Cdlb` and `PLUGIN_CODE Bkrv`. Changing these breaks saved AU projects. They are frozen from this release onward.
- MUST pass `auval -v aufx Bkrv Cdlb` (exit 0, all tests PASS).
- MUST restore state in Logic Pro: insert, adjust, save project, reopen, the same settings sound the same (golden check in spec 11 §3 with the state round trip).
- SHOULD expose the same editor size as VST3 (`W = 1280, H = 820` in `PluginEditor.h`).

Files: `src/plugin/CMakeLists.txt`, `.github/workflows/build.yml` (macOS job runs `auval`), `docs/RELEASE_NOTES.md`.

Edge cases:
- AU validation requires the bundle to be code-signed or at least ad-hoc signed on recent macOS. Coordinate with F-03; ad-hoc signing (`codesign -s -`) is enough for CI validation.
- Some AU hosts call `getTailLengthSeconds` on the audio thread indirectly; the value is a constant, which is fine.

Acceptance:
- `auval` passes on macOS CI.
- Logic Pro 11 and GarageBand load the effect, process audio, and restore state after a project save and reopen (manual, recorded in the release checklist).

Risks: Logic is strict about bus layouts; `isBusesLayoutSupported` must keep returning true for stereo-in/stereo-out and mono-in/stereo-out. Test both.

---

## F-03 Code signing and notarization — P0, M

**Goal:** Every shipped binary is signed, so Windows SmartScreen and macOS Gatekeeper accept the download without scary warnings.

Requirements:
- MUST sign Windows artifacts (`.exe` installer, standalone, VST3 and CLAP DLL files) with Authenticode, SHA-256 digest, RFC 3161 timestamp.
- MUST sign macOS artifacts with a Developer ID Application certificate using hardened runtime (`codesign --options runtime --timestamp --deep` is discouraged; sign each bundle inside-out: frameworks, then plug-in bundles, then the app).
- MUST notarize macOS artifacts with `xcrun notarytool submit --wait` and staple the ticket (`xcrun stapler staple`).
- MUST include the entitlement `com.apple.security.device.audio-input` for the standalone app, because the hardened runtime blocks microphone capture without it.
- MUST NOT commit certificates, passwords or Apple IDs. They live in GitHub Actions secrets only.
- SHOULD verify signatures in CI: `codesign --verify --deep --strict`, `spctl --assess` (macOS) and `signtool verify /pa` (Windows). Signing is skipped on forks and pull requests from forks.
- MUST publish the SHA-256 sums file signed or with the checksums in the release notes (already produced for Linux by CI).

Secrets (names are proposals): `MACOS_CERT_P12_BASE64`, `MACOS_CERT_PASSWORD`, `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`, `WIN_CERT_PFX_BASE64`, `WIN_CERT_PASSWORD` (or Azure Trusted Signing credentials if chosen; see `11-engineering-requirements.md` §5).

Acceptance:
- A downloaded macOS archive opens with no Gatekeeper dialog on a clean account.
- Windows installer properties show a valid signature with the company name.
- CI fails the release job if any signature check fails.

---

## F-04 Portable file references and missing-file relink — P0, M

**Goal:** Saved states and presets keep working when moved between machines, and a missing audio file never silently erases the user's settings.

**Problem today:** `loadFile` stores the absolute path in the `filePath` lane key (`src/plugin/PluginProcessor.cpp`, `setUi (lk::filePath, ...)`). On another machine the file is absent and `applyStateTree` skips it without telling the user.

Design:
- Store a **file identity** next to the path: basename, byte size, sample rate, length in samples, and SHA-256 of the first and last 1 MiB (fast for long files). New lane key `fileIdent` (string, `key=value;` pairs). Bump `schemaVersion` to 2.
- Resolution order on state load or preset load:
  1. Absolute `filePath` exists and identity matches → load.
  2. Search the user's **Audio search folders** (new setting, default `Documents/BackReverse/Sources` and the folder of the last-used file) for a file with the same basename and identity → load and update `filePath`.
  3. Otherwise mark the source **MISSING**: keep all parameters, do not play, show a banner "Missing audio: <basename> — Relink…".
- Relink opens a file chooser. The chosen file must match `fileIdent` or the user is asked to confirm a mismatch.

Requirements:
- MUST never fail or reset parameters because a file is missing (spec 05 §6, state tests in spec 06 §16).
- MUST never block the message thread while hashing a very large file. Hash on a background `juce::ThreadPool` job; show "checking…" in the banner.
- MUST keep reading schema 1 states (no `fileIdent`): they fall back to step 1 and 2 using basename only.
- SHOULD show the expected path in the banner tooltip.

Files: `src/plugin/PluginProcessor.cpp` (`loadFile`, `applyStateTree`, `stateTree`), `src/plugin/PluginProcessor.h`, `src/plugin/PluginEditor.cpp` (banner, relink action), `src/plugin/Presets.cpp` (preset load path uses the same resolver).

Tests:
- Unit: identity hash on a fixed fixture file; mismatch detection.
- State: schema 1 fixture loads; schema 2 round trip; missing file keeps parameters bit-exact (`BackReverseStateTest` extension).
- Manual: move a project file and its audio to another folder; reopen.

Acceptance: the same project opens with audio on a machine where the file was moved into a search folder; with the file removed, all parameters are unchanged and the banner appears.

---

## F-05 Diagnostics log and "Copy diagnostics" — P0, S

**Goal:** Beta testers can send a useful report in one click, without sending audio or personal data.

Requirements:
- MUST write a rolling log with `juce::FileLogger`: `~/Library/Logs/BackReverse/` (macOS), `%LOCALAPPDATA%\BackReverse\Logs\` (Windows), `~/.local/state/BackReverse/logs/` (Linux). Rotate at 1 MiB, keep 5 files.
- MUST log, on the message thread only: plug-in load, `prepareToPlay` (sample rate, block size), file load (basename, duration, sample rate, channels), render start and finish, state load errors and migrations, relink events, and any caught exception.
- MUST NOT log audio data, parameter values per block, or full paths. Redact the home directory to `~` before writing.
- MUST provide Global tab button **Copy diagnostics** that copies to the clipboard: version, build date, OS and version, CPU architecture, host name (`juce::PluginHostType`), format, sample rate and block size, the last 200 log lines.
- MUST NOT transmit anything. Telemetry is out of scope.

Files: new `src/plugin/Diagnostics.h/.cpp`, calls from `PluginProcessor.cpp`, a button in `PluginEditor.cpp` (Global page).

Realtime: logging must never run inside `processBlock`. Verify with the allocation check in spec 11 §4.

Acceptance: the clipboard text contains no `/home/` or `C:\Users\` prefix; log rotation keeps total size under 5 MiB in a 10,000-event stress test.

---

## F-06 Parallel routing buses (Dry, Reverse, Gate, FX A, FX B) — P1, L

**Goal:** Spec 02 §10: process two effect chains in parallel and blend them, so users can, for example, stutter one bus while echoing another.

Design:
- Keep the existing signal path up to the gate stage (`src/core/Engine.cpp` gate section). Split into two buses after the gate:
  - **FX A** and **FX B**: each is a full chain (Stutter, Delay, Echo, with its own chain order, scopes and sends).
  - Bus gains and pans: `busAGain`, `busBGain` (dB), `busAPan`, `busBPan`, appended to `Params.h`.
  - Final mix: `out = dryOrDry + wetA * gainA + wetB * gainB`, with the existing wet/dry and output gain stages unchanged.
- Bus routing: `busMode` choice: `A only`, `B only`, `A+B` (default `A+B` when the bus feature is enabled), and `Bus B source`: `Post-gate` or `Dry input`.
- Depends on F-07 (duplicate effect instances).

Requirements:
- MUST keep the default configuration bit-exact with the 0.0.2 output (golden test G-01 in spec 11 §3). The feature is off by default; `busMode = A only` with `busBGain = -inf` must equal 0.0.2.
- MUST keep dry alignment and latency unchanged (the buses share the same latency; no extra delay).
- MUST avoid allocation: bus buffers are fixed-size members allocated in `prepare()`.
- MUST keep CPU within +60% of the single-chain baseline measured by a render benchmark (spec 11 §4).

Tests:
- Null test: `busB` muted, bus A equal to 0.0.2 output within 1e-6.
- Sum test: A and B with identical settings and gains of 0.5 each equals single-chain output within 1e-6.
- Pan test: bus pans produce the expected channel balance.
- Golden test with the bus feature enabled.

Files: `src/core/Engine.h/.cpp`, new `src/core/Bus.h`, `src/plugin/Params.h`, `src/plugin/UI.h` (FX rack gains a second chain strip), `src/plugin/PluginEditor.cpp`.

Acceptance: the spec 02 §10 requirement "users can blend dry and processed audio without phase discontinuities" is verified with a sweep test (no sample-to-sample jump above 0.2 full scale on a sine when `busMode` changes at a zero crossing).

---

## F-07 Duplicate effect instances — P1, M

**Goal:** Spec 02 §9: allow a second Stutter, Delay and Echo, each with independent parameters, so they can be used in parallel buses or in one chain.

Design:
- Exactly two instances per effect type: `Stutter 1/2`, `Delay 1/2`, `Echo 1/2`. Instance 2 has a parallel parameter set with IDs `stut2*`, `dly2*`, `echo2*`, appended to `Params.h`.
- Chain order (`chainOrder`) becomes an order over six slots: `S1, D1, E1, S2, D2, E2`. Keep the existing six permutations as presets and add an advanced order choice later if needed.
- Engine: `Stutter stut[2]; Delay dly[2]; Echo echo[2];` in `Engine.h`. The per-instance state (buffers, periods) is independent.

Requirements:
- MUST keep the first instance identical to 0.0.2 (same parameter IDs, same defaults).
- MUST default instance 2 to off (`stut2On = 0` etc.), so existing presets are unaffected.
- MUST keep `FX Chain Order` as the existing parameter (ID unchanged); instance 2 uses new parameters only.
- MUST add the instance-2 parameters to the `fx` section of `sections()` in `PluginProcessor.cpp` so copy and paste works.
- MUST NOT increase memory beyond +12 MiB per instance at 48 kHz (buffers sized in `prepare()`).

Tests: duplicate of instance 1 with identical parameters produces the same output as the second instance alone; chain order with six slots covers all permutations of the 3-type chain in the existing table; state round trip with instance 2 active.

Acceptance: "Stutter → Delay → Echo → Stutter 2" can be built in the FX rack and saved, reloaded and rendered identically.

---

## F-08 Time-stretch transient and formant control — P2 (research spike first), L

**Goal:** Spec 01 §8: reduce smearing of drums in stretched audio and optionally preserve vowel formants.

Plan:
- **Spike (S, 3 days):** prototype two options and measure: (a) onset-triggered grain phase reset (transient sensitivity parameter) inside the existing granular stretcher; (b) a phase-vocoder variant with phase locking, behind a new `StretchAlgo::Vocoder` value. Report CPU and quality on the fixture set in spec 11 §3 (click train, drum loop, sung vowel).
- **Build (after spike):** pick the option that meets the acceptance numbers below. Expose `transientSens` (0–100 %) and `formantPreserve` (on/off) as appended parameters.
- **Licensing:** do not adopt Rubber Band Library without a decision. It is GPL or commercial, which conflicts with the MIT source licence in `LICENSE`.

Acceptance (on the fixture set, 2× and 0.5× stretch):
- Transient smearing: onset position error < 5 ms for click trains.
- Formant preservation: spectral envelope correlation ≥ 0.9 between input and output of a sustained vowel when `formantPreserve` is on, measured by the cepstral envelope.
- CPU ≤ 2× the granular stretcher at 48 kHz stereo.
- Golden test for the 0.0.2 granular path unchanged when `transientSens = 0` and `formantPreserve = off`.

---

## F-09 MIDI learn and performance macros — P1, M

**Goal:** Map any control to a MIDI controller for live performance, and provide 8 macro knobs that drive several parameters at once.

Design (no engine changes):
- Enable MIDI input: `acceptsMidi() = true` in `PluginProcessor.h`, `NEEDS_MIDI_INPUT TRUE` in `src/plugin/CMakeLists.txt`, and in CLAP the MIDI port is declared by `clap_juce_extensions`.
- In `processBlock`, read `midiMessages` (audio thread). For a CC message matching a learned mapping, store the scaled value into `std::atomic<float> pendingValue[paramIndex]` and set a dirty flag. Do **not** call `setValueNotifyingHost` here.
- A 30 Hz timer (already exists: `timerCallback`) applies dirty values with `setValueNotifyingHost` on the message thread, using the existing gesture pattern in `BackReverseProcessor::set`.
- Macros: 8 host-automatable float parameters `macro1..macro8` (0–1). Each macro has a mapping list `[(paramIndex, minNorm, maxNorm)]`, stored in lanes state. Applying a macro sets each mapped parameter to `min + macro*(max-min)` in normalized space on the message thread, through the same setter.
- Mappings are stored in `lanes` as `midiMap` and `macroMap` keys (schema 2).

Requirements:
- MUST complete a MIDI learn in ≤ 3 interactions: right-click a control → "MIDI learn" → move a controller.
- MUST show the learned mapping in the tooltip and offer "Clear MIDI mapping".
- MUST keep MIDI CC to parameter latency under 50 ms (timer-driven).
- MUST not interfere with host automation: when the host automates a parameter, the learned value is overwritten by the next automation event, which is the expected behaviour (document it).
- MUST be unavailable in format builds where MIDI input is not supported, with a clear message.

Files: `src/plugin/PluginProcessor.h/.cpp`, `src/plugin/UI.h` (right-click menu on `Knob`, `Toggle`, `Choice`), `src/plugin/PluginEditor.cpp` (Macros strip on the header or a Performance tab).

Tests: MIDI CC unit test by injecting `MidiBuffer` into `processBlock`; mapping serialization round trip; macro scaling test at 0, 0.5 and 1.

Acceptance: a controller CC changes the Chunk Length knob within 50 ms; a saved project restores the mapping.

---

## F-10 Preset morphing — P1, M

**Goal:** Crossfade between two saved states over a set time, for live transitions and sound-design movement.

Design (overlay, not host parameters):
- Morph slots A and B are two `EngineParams` + `Patterns` snapshots (taken from `snapshot()` and `pats`).
- A morph position `t ∈ [0,1]` lives in an atomic in the processor. A message-thread timer advances `t` over a time (ms) using a time-based curve.
- In `processBlock`, after `brp::read`, the engine's `P` is replaced by `morphApply(P, A, B, t)`:
  - float fields: linear interpolation in the parameter's own (not normalised) units, except frequencies, which interpolate in log space (declare a `logScale` flag per parameter).
  - choice, bool and integer fields: snap at `t = 0.5`.
  - pattern arrays: snap at 0.5 (no interpolation of step values).
- The host sees raw parameter values only; morph does not write host parameters, so automation and undo are unaffected.

Requirements:
- MUST be bit-exact at `t = 0` (equals A) and `t = 1` (equals B) (test M-01).
- MUST NOT allocate in `morphApply` (use a fixed struct copy).
- MUST not be saved in host state (morph is performance-only); the UI shows "Morph active" in the header so users notice.
- MUST stop morphing automatically if the user edits a parameter (edits cancel the morph and keep the current values).

Files: `src/plugin/PluginProcessor.h/.cpp` (snapshots, `morphApply`, timer), `src/plugin/Params.h` (`logScale` flag on definitions), `src/plugin/PluginEditor.cpp` (morph panel: A/B capture buttons, time, go button, position slider).

Acceptance: a 4-second morph from a 1/4x preset to a 3x preset passes through intermediate values without zipper noise (max sample-to-sample step within 1.5× the static-render maximum).

---

## F-11 Preset packs (import and export) — P2, S

**Goal:** Share sets of presets as one file.

Requirements:
- A pack is a `.brpack` zip: `manifest.xml` (pack name, author, version, `formatVersion`, `productVersion`) plus `presets/*.brpreset`. No audio is ever included (spec 05 §4).
- Import MUST validate `formatVersion`; packs from a newer format are refused with a message, not partially imported.
- Import MUST reject zip entries whose path escapes the pack folder (zip-slip). Use `juce::ZipFile` and check every entry with `juce::File::isAChildOf`.
- Export: Presets menu → "Export pack…" with a multi-select dialog.
- Imported presets go to `Documents/BackReverse/Presets/<pack name>/`.

Files: `src/plugin/Presets.cpp`, `src/plugin/PluginEditor.cpp`.

Tests: round trip of 3 presets; zip-slip fixture rejected; newer format refused.

Acceptance: export a pack on Windows, import on Linux, all presets load and produce identical state.

---

## F-12 Freeze to audio — P1, M

**Goal:** Render the current processed result to a new audio source, so the heavy processing can be switched off and the result edited or reused. Helps CPU use in a DAW.

Requirements:
- Standalone and plug-in: "Freeze" renders the loaded source (file or capture) through the current settings with the existing `startRender` path (offline, deterministic, spec 05 §9), writes a WAV to `Documents/BackReverse/Frozen/`, and switches the Source to the frozen file with processing bypassed (`bypass` on).
- A frozen file is a normal file source; the original settings are kept in an "unfreeze" slot (`lanes` key `frozenSettings`) so the user can return to the live processing.
- MUST render at the file's sample rate and the file's length (no rate conversion).
- MUST keep the frozen result identical to a realtime render of the same settings with the same seed (golden test G-03).
- Plug-in use: freeze works on the loaded or captured source only (the plug-in cannot read future host audio). This limitation is documented in the button tooltip.

Files: `src/plugin/PluginProcessor.cpp` (`startRender`, new `freeze()` and `unfreeze()`), `src/plugin/PluginEditor.cpp` (button).

Acceptance: a frozen file plays identically to the live render; "unfreeze" restores the exact prior parameter set (bit-exact state check).

---

## F-13 Tap tempo and groove lock — P2, S

**Goal:** Make chunks and gates lock to a musical grid, even when the standalone app has no host tempo.

Requirements:
- **Tap tempo** (standalone): a button averages the last 4 taps (ignore taps > 2 s apart) and sets `bpm` (existing parameter `bpm`, range 20–300).
- **Groove lock** (`grooveLock` on/off, appended): when on, pattern restarts and chunk boundaries are scheduled on the beat grid. It uses the existing `Restart::Bar/Beat` machinery in `Engine::process`. Chunk length is rounded to the nearest beat fraction when `chunkSync` is set.
- Host tempo overrides the standalone tempo when the host provides one (existing behaviour); tap tempo is disabled in plug-in builds and shows the host BPM.

Tests: a 120 BPM tap sequence gives 120 ± 0.5 BPM; with groove lock, a 1/4 chunk starts within ±1 sample of each beat at 120 BPM over 16 beats.

Acceptance: beat-aligned chunk starts measured by a render with `bpm = 120`, groove lock on.

---

## F-14 Dice roll randomizer with history — P2, S

**Goal:** A fun, one-key way to try random arrangements, with each roll recorded so the user can return to it.

Requirements:
- A **Roll** button and the `D` key run `randomize(reroll = true)` (existing), with a 300 ms visual "dice" animation (UI only; the audio result is applied immediately).
- A history list shows the last 16 rolls: seed, time, and the domains changed. Clicking an entry restores that roll's state through the undo manager, so it is undoable.
- Each history entry stores the seed and the randomize domain mask; a roll is reproducible from those two values (spec 05 §5).

Files: `src/plugin/PluginProcessor.cpp` (`randomize`, history in `lanes` as `rollHistory`), `src/plugin/PluginEditor.cpp` (Random page).

Acceptance: roll, roll again, click the first roll: the state equals the first roll's state bit-exact.

---

## F-15 First-run wizard, keyboard overlay and bundled demo — P1, S

**Goal:** A new user makes a first reverse sound within one minute.

Requirements:
- **First-run wizard:** shown when no user settings exist (`Documents/BackReverse/settings.xml` missing). Three steps: pick Source (Live / File), load the bundled demo, press Play. "Don't show again" is stored in settings, not in presets.
- **Bundled demo:** one CC0-licensed WAV, 16-bit 44.1 kHz, ≤ 2 MB, shipped in `assets/demo/`. Record the source and licence in `assets/demo/README.md`. The sample MUST NOT be a copyrighted track.
- **Keyboard overlay:** `?` (or F1 alias) toggles an overlay listing all bindings. The list is generated from a single table of shortcuts (`src/plugin/Shortcuts.h`), which the Help tab also uses, so the two cannot drift.

Tests: the shortcut table has no duplicate keys; the overlay and Help text are generated from the same table (unit test).

Acceptance: a tester completes the wizard and hears reversed audio without reading the manual (usability check with at least 3 users; record results).

---

## F-16 Beat-synced playhead pulse — P2, S

**Goal:** Visual feedback that shows the chunk and beat grid as the audio plays. Pure UI.

Requirements: the waveform's active chunk edge pulses on each chunk start using `tel.chunkIdx` changes; a "reduce motion" setting disables the pulse (accessibility, spec 03 §13). No effect on audio.

Acceptance: the pulse follows chunk changes within one UI frame (33 ms at 30 Hz); disabling the setting removes all animation.

---

## Summary matrix

| ID | Feature | Priority | Size | Depends on |
|---|---|---|---|---|
| F-01 | macOS universal build | P0 | S | — |
| F-02 | Audio Unit | P0 | M | F-01 |
| F-03 | Code signing and notarization | P0 | M | F-01, F-02 |
| F-04 | Portable file references, relink | P0 | M | — |
| F-05 | Diagnostics log | P0 | S | — |
| F-06 | Parallel buses | P1 | L | F-07 |
| F-07 | Duplicate effect instances | P1 | M | — |
| F-08 | Transient and formant stretch | P2 | L | research spike |
| F-09 | MIDI learn and macros | P1 | M | — |
| F-10 | Preset morphing | P1 | M | — |
| F-11 | Preset packs | P2 | S | — |
| F-12 | Freeze to audio | P1 | M | — |
| F-13 | Tap tempo and groove lock | P2 | S | — |
| F-14 | Dice roll history | P2 | S | — |
| F-15 | First-run wizard, overlay, demo | P1 | S | — |
| F-16 | Playhead pulse | P2 | S | — |
