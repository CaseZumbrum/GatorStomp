WAV to PCM conversion
=====================

There are simple python scripts that convert .wav files to raw PCM files (and vice versa).

.. note::

    raw PCM files are just bytestrings that can be passed to a PCM interface

.. code-block:: bash

    python tools/wav_to_pcm.py music.wav music.raw
    python tools/pcm_to_wav.py music.raw music.wav