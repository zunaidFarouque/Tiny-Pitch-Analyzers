cmake_minimum_required(VERSION 3.22)

project(TinyPitchAnalyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# GoogleTest before JUCE — ZIP fetch avoids flaky git-based FetchContent on Windows (see keyboardMidi-try1).
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(JUCE)

add_library(PitchLabEngine STATIC
    Source/Engine/AgcInt16.cpp
    Source/Engine/ChromaPostProcess.cpp
    Source/Engine/ChromaFolder.cpp
    Source/Engine/ChromaMap.cpp
    Source/Engine/CircularInt16Buffer.cpp
    Source/Engine/Decimator4x.cpp
    Source/Engine/EngineState.cpp
    Source/Engine/FloatIngress.cpp
    Source/Engine/FftMagnitudes.cpp
    Source/Engine/MultiResSpectrumStitch.cpp
    Source/Engine/PitchLabChord.cpp
    Source/Engine/PitchFromSpectrum.cpp
    Source/Engine/PitchLabEngine.cpp
    Source/Engine/SpectralMagSmear.cpp
    Source/Engine/StaticTables.cpp
    Source/Engine/WindowApplyQ24.cpp
)

target_include_directories(PitchLabEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine"
)

target_compile_features(PitchLabEngine PUBLIC cxx_std_20)

target_link_libraries(PitchLabEngine PUBLIC
    juce::juce_dsp
)

if(WIN32)
    target_compile_definitions(PitchLabEngine PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    target_compile_options(PitchLabEngine PRIVATE /FS)
endif()

juce_add_gui_app(TinyPitchHost
    PRODUCT_NAME "Tiny Pitch Analyzer"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "TinyPitch"
    DOCUMENT_EXTENSIONS wav aif aiff flac mp3 ogg m4a aac wma caf
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Microphone input for live pitch analysis."
)

target_sources(TinyPitchHost PRIVATE
    Source/App/CpuVisualizerHost.cpp
    Source/App/LegacyGLBatcher.cpp
    Source/App/Main.cpp
    Source/App/MainComponent.cpp
    Source/App/OpenGLVisualizerHost.cpp
    Source/App/SharedWaterfallRing.cpp
    Source/Tools/VizExport/VizCpuRenderer.cpp
)

target_link_libraries(TinyPitchHost PRIVATE
    PitchLabEngine
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(TinyPitchHost)

target_include_directories(TinyPitchHost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/App"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/VizExport"
)

target_compile_definitions(TinyPitchHost PRIVATE
    JUCE_USE_MP3AUDIOFORMAT=1
    "PITCHLAB_EXAMPLE_AUDIO_DIR=\"${CMAKE_SOURCE_DIR}/Source/App/ExampleAudio\""
    "PITCHLAB_TEST_ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/test-assets\""
    "PITCHLAB_DEFAULT_EXAMPLE_WAV=\"${CMAKE_SOURCE_DIR}/test-assets/voice and whistle.wav\""
)

if(WIN32)
    target_compile_definitions(TinyPitchHost PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    target_compile_options(TinyPitchHost PRIVATE /FS)
endif()

if(APPLE)
    target_compile_definitions(TinyPitchHost PRIVATE GL_SILENCE_DEPRECATION=1)
endif()

if(WIN32)
    target_link_libraries(TinyPitchHost PRIVATE opengl32)
endif()

add_executable(PitchLabVizExport
    Source/Tools/VizExport/main.cpp
    Source/Tools/VizExport/VizCpuRenderer.cpp
)

target_include_directories(PitchLabVizExport PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/VizExport"
)

target_compile_features(PitchLabVizExport PRIVATE cxx_std_20)

target_link_libraries(PitchLabVizExport PRIVATE
    PitchLabEngine
    juce::juce_audio_formats
    juce::juce_graphics
    juce::juce_recommended_warning_flags
)

if(WIN32)
    target_compile_definitions(PitchLabVizExport PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    target_compile_options(PitchLabVizExport PRIVATE /FS)
endif()

enable_testing()
add_subdirectory(Tests)
