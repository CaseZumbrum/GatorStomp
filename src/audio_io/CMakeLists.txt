find_package(ALSA REQUIRED)

# The original synchronous API is retained. FIFO helpers never launch processes.
add_library(audio_io STATIC audio_io.c audio_io_fifo.c)
target_include_directories(audio_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_io PUBLIC ALSA::ALSA)
target_compile_features(audio_io PRIVATE c_std_99)

add_executable(audio_io_worker audio_io_worker.c)
target_link_libraries(audio_io_worker PRIVATE audio_io)
target_compile_features(audio_io_worker PRIVATE c_std_99)
