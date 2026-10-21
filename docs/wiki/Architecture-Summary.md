# Architecture Summary

From the README and the specification register (`specs/README.md`).

- **One engine, three formats.** The DSP lives in `src/core` and is shared by the Standalone, VST3 and CLAP builds (README).
- **Chunks, not chunk order.** The reversal is inside each chronological chunk. Chunk order, speed, stretch, gating, stereo and phase, and effects are layered on top as independent stages (README).
- **Specs by layer.** `specs/01-dsp-engine.md`, `02-gate-effects-routing.md`, `03-ui-ux.md`, `04-plugin-standalone-host.md`, `05-parameters-presets-state.md`, `06-testing-acceptance.md`, with implementation audits dated 2026-10-06 and 2026-10-20.
- **Shared resources.** `SHARED_RESOURCE_MANIFEST.yaml` lists shared resources; the standards in `standards/` cover audio app baseline, help documentation and theme.
