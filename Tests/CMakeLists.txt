add_executable(PitchLabEngineTests
    ../Source/App/SharedWaterfallRing.cpp
    smoke_engine.cpp
    test_agc.cpp
    test_chord.cpp
    test_chroma_map.cpp
    test_chroma_folder.cpp
    test_chroma_post.cpp
    test_chroma_waterfall_shading.cpp
    test_copy_latest.cpp
    test_decimator.cpp
    test_multi_res_ingress.cpp
    test_multires_engine_smoke.cpp
    test_spectrum_stitching.cpp
    test_circular_buffer.cpp
    test_engine_state.cpp
    test_fft_chroma.cpp
    test_float_ingress.cpp
    test_null_pipeline.cpp
    test_offline_hop.cpp
    test_optimizations.cpp
    test_pitch_from_spectrum.cpp
    test_pre_emphasis.cpp
    test_pitch_lab_engine.cpp
    test_render_frame_snapshot.cpp
    test_renderer_backend_policy.cpp
    test_spectral_mag_smear.cpp
    test_shared_waterfall_ring.cpp
    test_static_tables.cpp
    test_viz_export_smoke.cpp
    test_waterfall_direction.cpp
    test_window_q24.cpp
)

target_link_libraries(PitchLabEngineTests PRIVATE
    PitchLabEngine
    GTest::gtest_main
)

add_dependencies(PitchLabEngineTests PitchLabVizExport)

target_include_directories(PitchLabEngineTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Source/App"
    "${CMAKE_SOURCE_DIR}/Source/Tools/VizExport"
)

target_compile_definitions(PitchLabEngineTests PRIVATE
    "PITCHLAB_VIZ_EXPORT_EXE=\"$<TARGET_FILE:PitchLabVizExport>\""
    PITCHLAB_ENGINE_TESTING=1
)

target_compile_features(PitchLabEngineTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PitchLabEngineTests PRIVATE /FS)
endif()

set_target_properties(PitchLabEngineTests PROPERTIES OUTPUT_NAME "PitchLabEngineTests")

include(GoogleTest)
gtest_discover_tests(PitchLabEngineTests)
