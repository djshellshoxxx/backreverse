# Developer Guide

Core DSP tests, no JUCE (checked on this snapshot: configure, build and 1 of 1 ctest test passes):

```sh
cmake -S . -B build-core -DBR_BUILD_PLUGIN=OFF -DBR_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core -j 8
ctest --test-dir build-core --output-on-failure
```

Full plugin build (JUCE 8.0.4 and clap-juce-extensions are fetched): see the README "Build from source" section, which lists the Windows generator and the Linux packages. The VST3, CLAP and Standalone targets were not built in this pass.

CI: `.github/workflows/build.yml`. It runs the DSP tests on Linux, builds Windows and universal macOS targets, runs pluginval and clap-validator, and packages releases.
