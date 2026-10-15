cmake_minimum_required(VERSION 3.22)

if(APPLE)
    # Apple Silicon only
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
endif()

project(VassaGruuv VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VG_COPY_PLUGIN "Copy the VST3 to the user plug-in folder after build" OFF)
set(VG_JUCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading it")

if(VG_JUCE_DIR)
    add_subdirectory(${VG_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VG_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/Exporter.cpp
    Source/AI/PatternGenerator.cpp
    Source/AI/ClaudeClient.cpp)

juce_add_plugin(VassaGruuv
    COMPANY_NAME "Vassa"
    PLUGIN_MANUFACTURER_CODE Vass
    PLUGIN_CODE Vgrv
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vassa Gruuv"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD ${VG_COPY_PLUGIN})

target_sources(VassaGruuv PRIVATE ${VG_SOURCES})
target_include_directories(VassaGruuv PRIVATE Source)

target_compile_definitions(VassaGruuv PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VassaGruuv
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_data_structures
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Unit tests (engine + AI logic, no GUI) ----
juce_add_console_app(VassaGruuvTests PRODUCT_NAME "VassaGruuvTests")
target_sources(VassaGruuvTests PRIVATE
    Tests/Tests.cpp
    Source/Engine/Exporter.cpp
    Source/AI/PatternGenerator.cpp
    Source/AI/ClaudeClient.cpp)
target_include_directories(VassaGruuvTests PRIVATE Source)
target_compile_definitions(VassaGruuvTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(VassaGruuvTests
    PRIVATE juce::juce_core juce::juce_events juce::juce_audio_basics juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Headless test of the real processor (sequencer, sampler, routing)
juce_add_console_app(VassaGruuvProcessorTest PRODUCT_NAME "VassaGruuvProcessorTest")
target_sources(VassaGruuvProcessorTest PRIVATE Tests/ProcessorTest.cpp)
target_include_directories(VassaGruuvProcessorTest PRIVATE Source)
target_compile_definitions(VassaGruuvProcessorTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(VassaGruuvProcessorTest
    PRIVATE VassaGruuv juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

enable_testing()
add_test(NAME unit COMMAND VassaGruuvTests)
add_test(NAME processor COMMAND VassaGruuvProcessorTest)
