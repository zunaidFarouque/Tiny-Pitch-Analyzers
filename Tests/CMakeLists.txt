add_executable(PitchLabEngineTests
    smoke_engine.cpp
    test_circular_buffer.cpp
    test_engine_state.cpp
    test_float_ingress.cpp
    test_pitch_lab_engine.cpp
    test_static_tables.cpp
)

target_link_libraries(PitchLabEngineTests PRIVATE
    PitchLabEngine
    GTest::gtest_main
)

target_compile_features(PitchLabEngineTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PitchLabEngineTests PRIVATE /FS)
endif()

set_target_properties(PitchLabEngineTests PROPERTIES OUTPUT_NAME "PitchLabEngineTests")

include(GoogleTest)
gtest_discover_tests(PitchLabEngineTests)
