find_package(ALSA REQUIRED)

# The original synchronous API is retained. FIFO helpers never launch processes.
add_library(audio_io STATIC audio_io.c audio_io_fifo.c)
target_include_directories(audio_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_io PUBLIC ALSA::ALSA)
target_compile_features(audio_io PRIVATE c_std_99)

add_executable(audio_io_worker audio_io_worker.c)
target_link_libraries(audio_io_worker PRIVATE audio_io)
target_compile_features(audio_io_worker PRIVATE c_std_99)

add_executable(audio_io_passthrough examples/pcm_passthrough.c)
target_link_libraries(audio_io_passthrough PRIVATE audio_io)
target_compile_features(audio_io_passthrough PRIVATE c_std_99)

add_executable(audio_io_lowpass examples/pcm_lowpass.c)
target_link_libraries(audio_io_lowpass PRIVATE audio_io)
target_compile_features(audio_io_lowpass PRIVATE c_std_99)

add_executable(audio_io_echo examples/pcm_echo.c)
target_link_libraries(audio_io_echo PRIVATE audio_io)
target_compile_features(audio_io_echo PRIVATE c_std_99)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(audio_io_pcm_test tests/pcm_integrity.c)
    target_link_libraries(audio_io_pcm_test PRIVATE audio_io)
    target_compile_features(audio_io_pcm_test PRIVATE c_std_99)
    add_test(NAME audio_io_pcm_integrity COMMAND audio_io_pcm_test)

    # Python is the test supervisor, not part of the production audio pipeline.
    set(fifo_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/fifo_transport.py)
    add_test(NAME audio_io_pipe_transport
        COMMAND ${Python3_EXECUTABLE} ${fifo_test}
                $<TARGET_FILE:audio_io_worker> $<TARGET_FILE:audio_io_passthrough> faults)
    add_test(NAME audio_io_orchestrator_integration
        COMMAND ${Python3_EXECUTABLE} ${fifo_test}
                $<TARGET_FILE:audio_io_worker> $<TARGET_FILE:audio_io_passthrough> 128)
    add_test(NAME audio_io_orchestrator_large_blocks
        COMMAND ${Python3_EXECUTABLE} ${fifo_test}
                $<TARGET_FILE:audio_io_worker> $<TARGET_FILE:audio_io_passthrough> 32768)
    set_tests_properties(audio_io_pipe_transport PROPERTIES TIMEOUT 60)
    set_tests_properties(audio_io_orchestrator_integration
                        audio_io_orchestrator_large_blocks PROPERTIES TIMEOUT 20)
endif()
