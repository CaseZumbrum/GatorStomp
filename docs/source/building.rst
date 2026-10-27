Building the C++ Code
=====================

We are using the CMake build system for the C++/C sides of this project. Each library should be defined with it's own CMakeLists.txt file that can be included and linked to when needed for an executable.

An example of this can be found with the `audio_orchestrator` and `gs_socket_api` directories, which each contain and CMakeLists.txt. The overall CMakeLists.txt file includes both of them, which allows them to link properly.

To actually build the code:

.. code-block:: bash

    cmake -S . -B build
    cd build
    make [TARGET]
    ./bin/[TARGET]