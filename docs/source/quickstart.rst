Quickstart
==========

This document is a guide to setting up the project for the first time.

The current state of the project uses 2 dev boards, the PocketBeagle 2 and the DA7212 audio codec.

#. Building the code
    - To build the project for PCs, use :doc:`/building`
    - To build the project fo the PocketBeagle, use :doc:`/cross_compile`
#. OS
    - To build the OS, use :doc:`/yocto_setup` (this is not needed though, there are images on github releases!)
    - To flash the OS, follow :doc:`/running_os`
#. Hardware
    - To setup the hardware for the audio codec, follow :doc:`codec`
#. Running Demos
    - For testing the whole system, follow :doc:`/demos/audio_orchestrator_demos`
    - For testing in the user input devices, use :doc:`/demos/pocketbeagle2_button_demo` and :doc:`pocketbeagle2_pedal_controls`