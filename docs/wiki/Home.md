# BackReverse Wiki

BackReverse (Circuit Drift Labs) plays audio backwards in controllable chunks. It reverses the samples inside each
chronological chunk, not the chunk order: a 120 s song with 10 s chunks plays 10→0, 20→10 and so on.
Standalone, VST3 and CLAP, all built from one shared DSP engine (`src/core`).

- [Developer Guide](Developer-Guide.md)
- [Architecture Summary](Architecture-Summary.md)
- [Codebase Map](Codebase-Map.md)

The authoritative specification is `specs/` (start with `specs/README.md`).
