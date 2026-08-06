add_executable(EngineTests
    EngineTestMain.cpp
    GlobalVelocityConfigTests.cpp
    RoundRobinTests.cpp
    KitMidiMapTests.cpp
    PresetTests.cpp
    DrumsetExporterTests.cpp
    SampleImportMatcherTests.cpp
    SamplePathRelinkerTests.cpp
    VoicePoolTests.cpp
    DrumEngineTests.cpp
)

target_include_directories(EngineTests PRIVATE ${CMAKE_SOURCE_DIR}/source)

target_link_libraries(EngineTests
    PRIVATE
        DrumEngineCore
        juce::juce_audio_utils
)

target_compile_definitions(EngineTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=0
)

add_test(NAME EngineTests COMMAND EngineTests)
