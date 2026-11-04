find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)

# Linux/glibc close-from spawn actions prevent inheriting the orchestrator's
# control sockets, even though IO_Controller does not currently set CLOEXEC.
include(CheckSymbolExists)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(posix_spawn_file_actions_addclosefrom_np "spawn.h"
                    AUDIO_IO_HAVE_SPAWN_CLOSEFROM)
unset(CMAKE_REQUIRED_DEFINITIONS)
if(NOT AUDIO_IO_HAVE_SPAWN_CLOSEFROM)
    message(FATAL_ERROR "Audio workers require Linux/glibc posix_spawn_file_actions_addclosefrom_np")
endif()

# Keep the blocking I/O module reusable by C and C++ callers. ALSA is a public
# dependency so executables linking this static library receive it as well.
add_library(audio_io STATIC audio_io.c audio_io_process.c audio_io_pipe.c)
target_include_directories(audio_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_io PUBLIC ALSA::ALSA Threads::Threads)
target_compile_features(audio_io PRIVATE c_std_99)

add_executable(audio_io_worker audio_io_worker.c)
target_compile_features(audio_io_worker PRIVATE c_std_99)
target_link_libraries(audio_io_worker PRIVATE audio_io)

# Compile the existing controller unchanged into this finite integration demo.
add_executable(audio_io_orchestrator_example
    examples/orchestrator_integration.cpp
    ../audio_orchestrator/io_system/io_controller.cpp)
target_include_directories(audio_io_orchestrator_example PRIVATE ../audio_orchestrator)
target_link_libraries(audio_io_orchestrator_example PRIVATE audio_io gs_socket_api)
add_dependencies(audio_io_orchestrator_example audio_io_worker)

if(BUILD_TESTING)
    # Raw-file tests run without capture/playback hardware.
    add_executable(audio_io_pcm_test tests/pcm_integrity.c)
    target_compile_features(audio_io_pcm_test PRIVATE c_std_99)
    target_link_libraries(audio_io_pcm_test PRIVATE audio_io)
    add_test(NAME audio_io_pcm_integrity COMMAND audio_io_pcm_test)

    add_executable(audio_io_pipe_test tests/pipe_transport.c)
    target_compile_features(audio_io_pipe_test PRIVATE c_std_99)
    target_link_libraries(audio_io_pipe_test PRIVATE audio_io)
    add_dependencies(audio_io_pipe_test audio_io_worker)
    add_test(NAME audio_io_pipe_transport
             COMMAND audio_io_pipe_test $<TARGET_FILE:audio_io_worker>)
    add_test(NAME audio_io_orchestrator_integration
             COMMAND audio_io_orchestrator_example $<TARGET_FILE:audio_io_worker>)
    # 32768 frames exceed normal pipe capacity and force partial writes; the
    # 32771-frame fixture also verifies a final short block.
    add_test(NAME audio_io_orchestrator_large_blocks
             COMMAND audio_io_orchestrator_example $<TARGET_FILE:audio_io_worker> 32768)
    set_tests_properties(audio_io_pipe_transport audio_io_orchestrator_integration
                         audio_io_orchestrator_large_blocks PROPERTIES TIMEOUT 20)
endif()
