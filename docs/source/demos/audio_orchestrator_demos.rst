Audio Orchestrator Demos
========================

There are two demos for the audio orchestrator that spin up a simulated DAC/ADC and pipe audio through the orchestrator.

There are a few dependencies for running

.. code-block:: bash

   pip install -r requirements.dev.txt
   sudo apt install alsa-utils pulseaudio-utils libasound2-plugins

One demo is live (plays output through your speakers, I believe this will only work on wsl but it may work on bare-metal) and the other performs processing as fast as possible and saves it to an output file.

.. code-block:: bash

   bash demos/demo.sh input.wav output.wav
   # real time demo (may be a little static-y, this seems to be from the virtual interface and not the actual orchestrator)
   bash demos/live_demo.sh stairway.wav

.. note::

    As of right now, the demos only function on 44.1 kHz 2 channel audio, but small changes could be made to make it work with other .wav files