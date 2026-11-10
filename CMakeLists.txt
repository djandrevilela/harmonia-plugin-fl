cmake_minimum_required(VERSION 3.22)
project(Harmonia VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

option(HARMONIA_BUILD_PLUGIN "Compilar o plugin VST3 (descarrega o JUCE)" ON)
option(HARMONIA_COPY_AFTER_BUILD "Copiar o VST3 para a pasta de plugins do sistema após compilar" OFF)

set(HARMONIA_CORE_SOURCES
  core/Theory.cpp core/Progressions.cpp core/VoiceLeading.cpp core/NextNote.cpp core/HarmonyChecker.cpp)

# --- Motor de teoria + testes (sem JUCE) ---
add_library(harmonia_core STATIC ${HARMONIA_CORE_SOURCES})
target_include_directories(harmonia_core PUBLIC core)
add_executable(harmonia_core_tests tests/core_tests.cpp)
target_link_libraries(harmonia_core_tests PRIVATE harmonia_core)
enable_testing()
add_test(NAME core COMMAND harmonia_core_tests)

# --- Plugin VST3 ---
if(HARMONIA_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(Harmonia
    COMPANY_NAME "Harmonia"
    PLUGIN_MANUFACTURER_CODE Hrml
    PLUGIN_CODE Hrm1
    FORMATS VST3
    PRODUCT_NAME "Harmonia"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${HARMONIA_COPY_AFTER_BUILD})

  target_sources(Harmonia PRIVATE
    ${HARMONIA_CORE_SOURCES}
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp)
  target_include_directories(Harmonia PRIVATE core plugin)
  target_compile_definitions(Harmonia PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(Harmonia
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
endif()
