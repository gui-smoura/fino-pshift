# Tests CMakeLists.txt

add_executable(fino_tests
    test_spsc_ring_buffer.cpp
    test_pitch_math.cpp
    test_dsp_processor.cpp
)

target_link_libraries(fino_tests PRIVATE
    fino_core
    Catch2::Catch2WithMain
)

target_compile_options(fino_tests PRIVATE ${FINO_WARNING_FLAGS})

include(CTest)
include(Catch)
catch_discover_tests(fino_tests)
