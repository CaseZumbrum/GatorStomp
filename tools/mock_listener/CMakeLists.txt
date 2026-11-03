add_executable(mock_listener main.cpp)
target_link_libraries(mock_listener PRIVATE gs_socket_api)
target_compile_options(mock_listener PRIVATE -Wall -Wextra)
