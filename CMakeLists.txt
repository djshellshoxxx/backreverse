cmake_minimum_required(VERSION 3.22)
project(BackReverse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BACKREVERSE_BUILD_PLUGIN "Build JUCE VST3/CLAP/Standalone targets" ON)
option(BACKREVERSE_BUILD_TESTS "Build core DSP tests" ON)

add_library(backreverse_core
    src/core/BackReverseEngine.cpp
    src/core/PatternEngine.cpp
    src/core/Effects.cpp
)
target_include_directories(backreverse_core PUBLIC include)

if(BACKREVERSE_BUILD_TESTS)
  enable_testing()
  add_executable(backreverse_tests tests/test_main.cpp)
  target_link_libraries(backreverse_tests PRIVATE backreverse_core)
  add_test(NAME backreverse_tests COMMAND backreverse_tests)
endif()

if(BACKREVERSE_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6)
  FetchContent_MakeAvailable(JUCE)
  FetchContent_Declare(clap_juce_extensions GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git GIT_TAG main)
  FetchContent_MakeAvailable(clap_juce_extensions)

  juce_add_plugin(BackReverse
    COMPANY_NAME "Circuit Drift Labs"
    BUNDLE_ID "com.circuitdriftlabs.backreverse"
    PLUGIN_MANUFACTURER_CODE Cdrl
    PLUGIN_CODE BkRv
    FORMATS VST3 Standalone
    PRODUCT_NAME "BackReverse"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
  )

  target_sources(BackReverse PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
  )
  target_include_directories(BackReverse PRIVATE include src/plugin)
  target_link_libraries(BackReverse PRIVATE
    backreverse_core
    juce::juce_audio_utils
    juce::juce_dsp
  )
  target_compile_definitions(BackReverse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
  )

  if(COMMAND clap_juce_extensions_plugin)
    clap_juce_extensions_plugin(
      TARGET BackReverse
      CLAP_ID "com.circuitdriftlabs.backreverse"
      CLAP_FEATURES "audio-effect" "stereo"
    )
  endif()
endif()
