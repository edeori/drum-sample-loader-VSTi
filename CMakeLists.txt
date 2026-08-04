cmake_minimum_required(VERSION 3.22)

project(DrumSampleLoaderVSTi VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Engine/functionality core, independent of the plugin wrapper so the same
# code can be linked into both the plugin target and the unit test executable.
add_library(DrumEngineCore STATIC
    source/Engine/GlobalVelocityConfig.cpp
    source/Engine/Instrument.cpp
    source/Engine/Kit.cpp
    source/Engine/BusRoutingTable.cpp
    source/Engine/DrumsetExporter.cpp
    source/Engine/SamplePool.cpp
    source/Engine/SamplePathRelinker.cpp
    source/Engine/SampleImportMatcher.cpp
    source/Engine/VoicePool.cpp
    source/Engine/DrumEngine.cpp
    source/Engine/DefaultKitFactory.cpp
    source/Engine/Preset.cpp
)

target_include_directories(DrumEngineCore PUBLIC source)

target_compile_definitions(DrumEngineCore
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(DrumEngineCore
    PUBLIC
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_add_binary_data(DrumVaultAssets
    SOURCES
        assets/instruments/kick.png
        assets/instruments/snare.png
        assets/instruments/tom.png
        assets/instruments/cymbal.png
        assets/stage/drumset.png
        assets/stage/standalone-cymbal.png
)

juce_add_plugin(DrumSampleLoaderVSTi
    COMPANY_NAME "Moth Production"
    BUNDLE_ID "com.mothproduction.swarm"
    PLUGIN_MANUFACTURER_CODE Moth
    PLUGIN_CODE Swrm
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Swarm"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES "Instrument" "Drum"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

target_sources(DrumSampleLoaderVSTi
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/UI/DrumVaultLookAndFeel.cpp
        source/UI/KitEditController.cpp
        source/UI/Dialogs.cpp
        source/UI/KitOverviewPanel.cpp
        source/UI/SampleTable.cpp
        source/UI/InstrumentEditorPanel.cpp
        source/UI/MixerPanel.cpp
        source/UI/SidePanel.cpp
        source/UI/TopBar.cpp
        source/UI/KitStageView.cpp
)

target_include_directories(DrumSampleLoaderVSTi PRIVATE source)

target_compile_definitions(DrumSampleLoaderVSTi
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DrumSampleLoaderVSTi
    PRIVATE
        DrumEngineCore
        DrumVaultAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_subdirectory(tests)
