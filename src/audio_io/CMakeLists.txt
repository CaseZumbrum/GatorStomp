find_package(ALSA REQUIRED)

# Keep the blocking I/O module reusable by C and C++ callers. ALSA is a public
# dependency so executables linking this static library receive it as well.
add_library(audio_io STATIC audio_io.c)
target_include_directories(audio_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_io PUBLIC ALSA::ALSA)
target_compile_features(audio_io PRIVATE c_std_99)

if(BUILD_TESTING)
    # Raw-file tests run without capture/playback hardware.
    add_executable(audio_io_pcm_test tests/pcm_integrity.c)
    target_compile_features(audio_io_pcm_test PRIVATE c_std_99)
    target_link_libraries(audio_io_pcm_test PRIVATE audio_io)
    add_test(NAME audio_io_pcm_integrity COMMAND audio_io_pcm_test)
endif()
