# BackReverse — Implementation Plan (v0.1 roadmap)

How to deliver the features in `09-feature-specifications.md` with the fewest bugs and merge conflicts. Read `11-engineering-requirements.md` first: the rules there (append-only parameters, golden renders, state migrations, CI gates) apply to every step.

## 1. Principles

1. **Prepare the ground before features.** Phase 0 removes the files that every feature would otherwise edit at the same time (the editor and the widget header). Without it, parallel work conflicts constantly.
2. **One feature, one branch, one PR.** Branch name `feat/F-xx-short-name`. PR title starts with the feature ID. Keep PRs under about 800 changed lines; split larger work into stacked PRs that each build and pass CI.
3. **Merge trunk in often.** Merge `main` into the feature branch at least twice a week, and before opening the PR. Use merge commits on shared branches. Squash-merge PRs into `main`. Never force-push a branch someone else is using.
4. **Behind a flag until proven.** Features F-06, F-07 and F-08 compile behind `BR_EXPERIMENTAL` (CMake option, default OFF in release builds) until their acceptance tests pass on all platforms.
5. **Golden first.** Before changing audio code, add or update the golden render test for the affected path, so any change in output is visible in the PR diff.

## 2. Phase plan

| Phase | Goal | Features | Gate to leave phase |
|---|---|---|---|
| 0 | Prepare | Editor split, param-ID guard, golden renders, sanitizer CI, screenshot harness in `tools/` | All existing tests pass; golden files committed; no UI change in screenshots |
| 1 | Release readiness | F-05, F-04, F-01, F-02, F-03 | Signed macOS and Windows artifacts; `auval` pass; relink works |
| 2 | Value | F-07, F-06, F-09, F-10, F-12, F-15 | Acceptance tests of each feature pass on Linux and macOS |
| 3 | Polish | F-11, F-13, F-14, F-16, F-08 spike | F-08 spike decision recorded in `specs/` before any build work |
| 4 | Stabilise | Remove `BR_EXPERIMENTAL` gates for F-06/F-07/F-08, soak tests, multi-host pass | Spec 06 §17 and §19 satisfied; release notes list tested hosts |

Phase 0 and Phase 1 can overlap partly: F-05 (diagnostics) and F-04 (file identity) touch different files from the Phase 0 refactor, so they can start as soon as the parameter guard is merged.

## 3. Phase 0 — preparation (≈ 4–6 dev-days)

### 0.1 Split the editor (PR `refactor/split-editor`)
- Move each tab builder out of `src/plugin/PluginEditor.cpp` into `src/plugin/pages/PageChunks.cpp`, `PagePatterns.cpp`, `PageTime.cpp`, `PageGates.cpp`, `PagePanPhase.cpp`, `PageFxRack.cpp`, `PageScratch.cpp`, `PageRandom.cpp`, `PageGlobal.cpp`, `PageHelp.cpp`. Each exposes one `void buildXxxPage(Content&, juce::TabbedComponent&)` function.
- Move widgets from `src/plugin/UI.h` into `src/plugin/widgets/` (`Knob.h`, `StepLane.h`, `GateEditor.h`, `Waveform.h`, `Platter.h`, `ChainStrip.h`, `Meter.h`, `Look.h`). `UI.h` becomes a thin include header to keep existing includes working.
- No behaviour change. Verify: screenshot harness output is pixel-identical to the 0.0.2 renders (compare SHA-256 of each PNG). Commit the reference PNGs under `tests/golden/ui/`.
- Why first: every UI feature (F-04, F-09, F-10, F-14, F-15) edits the editor. Splitting the file removes most merge conflicts.

### 0.2 Parameter ID guard (PR `test/param-id-guard`)
- Add `tests/param_ids.txt`: the ordered list of IDs as shipped in 0.0.2.
- Add a test that generates the current list from `brp::kDefs` and requires that the shipped list is a prefix of it. Adding parameters at the end passes; reordering, renaming or removing fails.
- Changing an ID intentionally needs a deliberate update to the file and a note in the PR.

### 0.3 Golden renders (PR `test/golden-render`)
- New test `tests/golden_render.cpp`: renders 12 fixed configurations (seed, source, each reverse mode, gate on, each effect alone, bypass) into float buffers, hashes them (SHA-256 of 32-bit float samples after rounding to 1e-6), and compares with `tests/golden/render_hashes.txt`.
- Hashes are platform-sensitive for floating point. Pin them to Linux GCC (CI runner) and allow a tolerance-based comparison on other platforms (max abs difference 1e-5, see spec 11 §3).
- G-01 to G-03 used by F-06, F-07 and F-12 are defined here.

### 0.4 Sanitizer and thread CI (PR `ci/sanitizers`)
- Add `ci` jobs: `core-asan-ubsan` (`-fsanitize=address,undefined`) and `core-tsan` (`-fsanitize=thread`) running `br_core_tests`. Keep LeakSanitizer enabled on CI runners (the 2026-10-06 audit disabled it because of container ptrace limits; GitHub runners allow it).

### 0.5 Screenshot harness to `tools/` (PR `tools/snapshot`)
- Move the harness used in this session (`snap.cpp` plus CMake) into `tools/snapshot/`, built only with `-DBR_BUILD_TOOLS=ON`. Used for UI golden checks in 0.1 and every UI feature.

## 4. Phase 1 — release readiness

Order inside the phase: F-05 → F-04 → F-01 → F-02 → F-03. F-05 first because it helps debug everything after it.

### F-05 Diagnostics (≈ 2 days)
1. Add `src/plugin/Diagnostics.h/.cpp` with a singleton that owns `juce::FileLogger` and exposes `log(const juce::String&)` (message thread only).
2. Call sites: `prepareToPlay`, `loadFile`, `startRender`, `applyStateTree`, the exception catch around state load.
3. Add the Global-page button and the clipboard function.
4. Tests: path redaction unit test; rotation stress test; clipboard contains no user path.

### F-04 Portable references (≈ 5 days)
1. Pure function `FileIdent makeIdent(const juce::File&, …)` and `bool matches(const FileIdent&, const juce::File&)` in `src/plugin/FileIdent.h/.cpp`, unit-tested with fixtures (`tests/fixtures/ident/`).
2. Background hashing via `juce::ThreadPool` in the processor; results delivered with `MessageManager::callAsync`.
3. State schema 2: add `fileIdent`, migrate schema 1 states (test fixture from 0.0.2 committed in `tests/fixtures/state/v0.0.2_*.xml`).
4. Resolver used by `loadFile`, `applyStateTree` and `Presets.cpp`.
5. UI: banner component in `PluginEditor.cpp`, Relink action, Audio search folders setting (Global page).
6. Test extension of `BackReverseStateTest`: schema 1 loads, schema 2 round trip, missing file keeps parameters bit-exact.

### F-01 macOS build (≈ 3 days)
1. CMake platform switch (architectures, deployment target). Record the deployment target decision in `11-engineering-requirements.md` §6 first.
2. CI `macos` job on `macos-14`: build, ctest, pluginval, clap-validator, `lipo -archs` check, archive packaging.
3. Fix any macOS-only warnings found (expected: `-Wshadow` in `UI.h` already flagged in the 0.0.2 build log).

### F-02 Audio Unit (≈ 3 days)
1. Add `AU` to `FORMATS`; no code change expected.
2. CI runs `auval -v aufx Bkrv Cdlb` after the macOS build; ad-hoc signing for CI.
3. Manual Logic Pro test on a Mac (record result in the PR).

### F-03 Signing (≈ 4 days, plus certificate lead time)
1. Start certificate procurement at the start of Phase 0 (Apple Developer enrolment and Windows certificate can take days). See `11-engineering-requirements.md` §5.
2. Add a signing script `tools/release/sign-macos.sh` and `tools/release/sign-windows.ps1`, both taking the artifact path as the only argument. Test locally with a dummy certificate.
3. CI steps guarded by `if: secrets.MACOS_CERT_P12_BASE64 != ''` so forks still build.

## 5. Phase 2 — value features

Recommended order and reasons:

1. **F-07 duplicate effect instances** first, because F-06 depends on it. Plan:
   - Refactor `Stutter`, `Delay`, `Echo` usage in `Engine.cpp` from single members to `Stutter stut[2]` etc., **with instance 2 disabled**. Golden tests must stay identical (G-01). Merge this step alone.
   - Add the instance-2 parameters at the end of `Params.h`, then the UI strip, then copy/paste sections.
2. **F-06 parallel buses** behind `BR_EXPERIMENTAL`. Plan:
   - Step 1: `src/core/Bus.h` with a `BusProcessor` struct holding the two chains and gains; default configuration equals 0.0.2 (G-01).
   - Step 2: routing and sum in `Engine::process`, behind the flag.
   - Step 3: null, sum and pan tests (spec 09 F-06).
   - Step 4: CPU benchmark (`tools/bench/`) recorded in the PR.
3. **F-09 MIDI learn and macros.** Only touches processor and UI. Plan: MIDI input enabling first (with a PR that proves no regression when no MIDI is sent), then learn UI, then macros.
4. **F-10 preset morphing.** Plan: `morphApply` as a pure function with unit tests first (no threads), then the timer and UI.
5. **F-12 freeze to audio.** Reuses `startRender`. Plan: refactor `startRender` into a function that returns a rendered buffer so both export and freeze call it (PR 1, no behaviour change), then add the freeze path (PR 2).
6. **F-15 wizard, overlay and demo.** Plan: shortcut table first (used by both Help and overlay), then the overlay, then the wizard, then the demo asset (licence cleared before the asset merges).

## 6. Phase 3 — polish and research

- **F-11, F-13, F-14, F-16** are small and independent. Assign to whoever is least busy; each is one PR.
- **F-08 spike.** Time-box to 3 days. Output is a decision record `specs/adr-stretch-algorithm.md` with measured numbers from the fixture set (spec 09 F-08 acceptance). No product code is merged from the spike; the build starts as a separate feature branch after the decision.

## 7. Conflict-avoidance rules

- **File owners.** Declare owners for `src/core/Engine.cpp`, `src/plugin/Params.h`, `src/plugin/PluginProcessor.cpp`, `src/plugin/pages/*` in a `CODEOWNERS` file. Owners review changes in their area. (With a single developer, use it as a checklist of files to think about.)
- **Params.h is append-only** (enforced by the test in 0.2). Two features that both add parameters will conflict at the end of the list; resolve by renumbering your own additions, never the other feature's.
- **State keys.** New lane keys are added in one block in `PluginProcessor.h` (`BR_LANE_KEYS`, `BR_UI_KEYS`). Put each feature's keys on their own line to make the merge trivial.
- **Schema version.** Only one schema bump per release. If two features need schema changes in the same release, combine them in one migration function, written by the first feature to merge; the second adds its fields to that function.
- **Golden hashes.** Any PR that changes a golden hash must include the reason and the new hash in its description. Reviewers check the reason, not the number.

## 8. Testing strategy per feature

| Feature | Unit | Golden | State | Host / manual |
|---|---|---|---|---|
| F-01 | — | — | — | build + validators on macOS |
| F-02 | — | — | round trip in Logic | `auval`, Logic Pro |
| F-03 | — | — | — | Gatekeeper and SmartScreen on clean accounts |
| F-04 | identity hash, matches | — | schema 1 and 2, missing file | move project to another folder |
| F-05 | redaction, rotation | — | — | copy diagnostics on each OS |
| F-06 | null, sum, pan | G-01 with buses on | — | CPU benchmark |
| F-07 | instance equivalence | G-01 | six-slot round trip | — |
| F-08 | spike metrics | granular path unchanged | — | listening test with 3 people |
| F-09 | CC injection, scaling | — | mapping round trip | MIDI controller in Reaper |
| F-10 | morph endpoints | — | — | zipper check by listening |
| F-11 | zip-slip, format refusal | — | — | cross-OS import |
| F-12 | — | G-03 | bit-exact unfreeze | — |
| F-13 | tap tempo, beat alignment | groove lock render | — | — |
| F-14 | roll history reproduce | — | roll state bit-exact | — |
| F-15 | shortcut table | — | settings file | usability test, 3 users |
| F-16 | — | — | — | visual check, reduce-motion off/on |

## 9. Definition of done (every feature)

- [ ] Spec in `09-feature-specifications.md` updated if the implementation deviated (with reason).
- [ ] All tests in the table above exist and pass on Linux CI; macOS and Windows jobs pass where applicable.
- [ ] Sanitizer and TSan jobs pass.
- [ ] Golden hashes unchanged, or changed with a reason in the PR.
- [ ] No new parameter ID changes (guard test passes).
- [ ] Schema migration tests exist for any state change.
- [ ] Realtime check: no allocation on the audio thread (allocation hook build passes, spec 11 §4).
- [ ] `specs/08-implementation-status.md` updated.
- [ ] Release notes entry written.
- [ ] Reviewed by someone who did not write the code (for a solo developer: a fresh-session review using the code-review skill).

## 10. Risk register

| Risk | Impact | Mitigation |
|---|---|---|
| Certificate lead time delays F-03 | Public release slips | Start procurement in Phase 0; ship beta with a documented "unsigned" warning if needed |
| F-06 CPU cost too high | Users disable the feature | Benchmark gate in the plan; flag stays off by default |
| F-08 quality target unreachable | Scope creep | Time-boxed spike; decision recorded, feature can be dropped |
| Schema migration bugs | Lost user settings | Fixture-based migration tests from 0.0.2 files |
| macOS-only build breaks Windows/Linux | Lost CI signal | Matrix builds on every PR, not just on release |
| Editor split changes behaviour | Hidden UI regressions | Pixel-identical screenshot golden check in Phase 0 |
| Licence conflicts (GPL time-stretch library, sample licence) | Cannot ship | Licence review gate before merging F-08 or F-15 assets |
