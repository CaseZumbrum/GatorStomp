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


Demo on Beagle Board
+++++++++++++++++++++

There is also a demo for the beagle board itself, this demo uses the aux input and output of the dev-board (which are the two audio jacks)

Follow the instructions for :doc:`/cross_compile` to get the binaries prepared.

.. code-block:: bash
   
   # in the beagle board!
   bash beagle_demo.sh

.. important::

   make sure you transfer binaries for the audio_orchestrator and audio_io_workers (as well as the demo script itself)

If you just want to test the speakers/mic independently, you can use the below commands

.. code-block:: bash
   
   # configure what channels are on/off, you can also use the amixer settings found in the demo if wanted!
   alsamixer

   # this will play the .wav file to the speakers
   aplay -D plughw:0,0 stairway.wav

   # this will record across the aux input
   arecord -D plughw:0,0 -f S16_LE -r 48000 -c 2 -d 10 line_input.wav

