# Codebase Map

| I want to... | Look in |
|---|---|
| DSP engine and core types | `src/core/Engine.*`, `src/core/Fx.h`, `src/core/Types.h` |
| Plugin processor, editor, parameters, presets, help | `src/plugin/` (`PluginProcessor.*`, `PluginEditor.*`, `Params.h`, `Presets.cpp`, `HelpText.h`, `UI.h`) |
| Core DSP tests | `ctest` target in `build-core` (from `CMakeLists.txt`) |
| Specs and audits | `specs/` |
| Release notes | `docs/RELEASE_NOTES.md` |
| Installer | `installer/BackReverse.iss` |
| Shared standards | `standards/` |
| CI | `.github/workflows/build.yml` |
