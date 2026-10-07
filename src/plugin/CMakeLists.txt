include(FetchContent)
set(FETCHCONTENT_QUIET ON)
if(BR_JUCE_DIR)
  add_subdirectory(${BR_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)
else()
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(juce)
endif()
if(BR_CJE_DIR)
  add_subdirectory(${BR_CJE_DIR} ${CMAKE_BINARY_DIR}/cje EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(clap_juce_extensions GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git GIT_TAG main GIT_SHALLOW ON GIT_SUBMODULES_RECURSE ON)
  FetchContent_GetProperties(clap_juce_extensions)
  if(NOT clap_juce_extensions_POPULATED)
    FetchContent_Populate(clap_juce_extensions)
    add_subdirectory(${clap_juce_extensions_SOURCE_DIR} ${clap_juce_extensions_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
endif()

# Shared processor/editor sources: compiled into the plug-in and into the state test.
add_library(br_plugin_src INTERFACE)
target_sources(br_plugin_src INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Presets.cpp)
target_compile_definitions(br_plugin_src INTERFACE
  BR_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1
  JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(br_plugin_src INTERFACE br_core juce::juce_audio_utils juce::juce_audio_formats)

juce_add_plugin(BackReverse
  COMPANY_NAME "Circuit Drift Labs"
  COMPANY_WEBSITE "https://github.com/djshellshoxxx/backreverse"
  BUNDLE_ID com.circuitdriftlabs.backreverse
  PLUGIN_MANUFACTURER_CODE Cdlb
  PLUGIN_CODE Bkrv
  FORMATS VST3 Standalone
  PRODUCT_NAME "BackReverse"
  VERSION ${PROJECT_VERSION}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  VST3_CATEGORIES Fx Delay
  DESCRIPTION "Chunked reverse, time manipulation, gating and effects")
juce_generate_juce_header(BackReverse)
target_link_libraries(BackReverse PRIVATE br_plugin_src
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET BackReverse
  CLAP_ID "com.circuitdriftlabs.backreverse"
  CLAP_FEATURES audio-effect stereo delay
  CLAP_MANUAL_URL "https://github.com/djshellshoxxx/backreverse"
  CLAP_SUPPORT_URL "https://github.com/djshellshoxxx/backreverse/issues")

if(BR_BUILD_TESTS)
  # State round-trip test against the real processor (spec 06 s16)
  juce_add_console_app(BackReverseStateTest PRODUCT_NAME "BackReverseStateTest")
  juce_generate_juce_header(BackReverseStateTest)
  target_sources(BackReverseStateTest PRIVATE ${CMAKE_SOURCE_DIR}/tests/state_test.cpp)
  target_link_libraries(BackReverseStateTest PRIVATE br_plugin_src juce::juce_recommended_config_flags)
  add_test(NAME state COMMAND BackReverseStateTest)
endif()
