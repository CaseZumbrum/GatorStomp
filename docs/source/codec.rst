Audio Codec
============

This project uses the DA7212 audio codec (coder-encoder). This device is essentially a I2S DAC+ADC with some hardware to make it work as a sound card.

This device is mapped to the OS via the devicetree as a soundcard using the existing DA7213 drivers for linux (which are compatible).

More documentation can be found `here <https://docs.waveshare.com/DA7212-Audio-Board-A/Resources-And-Documents>`_

The device tree for this project should automatically mount it and it should show up with these commands

.. code-block:: bash

    # show the DAC interface
    aplay -l
    # show the ADC interface
    arecord -l

.. note:: 

    The DA7212 should be mounted as a ``plughw:0,0`` and ``hw:0,0`` device for both

You can pass ``plughw:0,0`` into the ``audio_io_workers`` as ALSA devices to get them working.

Hardware Connnections
+++++++++++++++++++++

.. list-table::
   :header-rows: 1
   :widths: 35 35 30

   * - Function
     - PocketBeagle 2 Header Pin
     - Processor Signal
   * - I2S Data Input (RX)
     - P1.10
     - MCASP0_AXR3
   * - I2S Data Output (TX)
     - P1.12
     - MCASP0_AXR2
   * - I2S Bit Clock (BCLK)
     - P2.01
     - MCASP0_ACLKX
   * - I2S Word Clock (LRCLK)
     - P1.04
     - MCASP0_AFSX
   * - I2C Data (SDA)
     - P1.26
     - I2C2_SDA
   * - I2C Clock (SCL)
     - P1.28
     - I2C2_SCL

Follow this table for connecting the codec to the PocketBeagle. The picture below may be useful!

.. image:: /_static/images/codec_connection.png
   :width: 400px
   :alt: Image showing codec connected to the PocketBeagle
   :align: center


Configuring the Codec
+++++++++++++++++++++

The codec can be configured with ``alsamixer`` and ``amixer``.

.. note::

    ``alsamixer`` is a very useful command line tool that provides live control of the codec (via it's i2c interface)

The configuration used for most testing is shown below, this takes in audio across the AUX jack (only from the left channel) and makes it available to the OS, and then sends mono audio out across the speaker jack.

.. code-block:: bash

    # Enable AUX input
    amixer cset name='Aux Switch' on
    amixer cset name='Aux Volume' 90%

    # Route AUX left/right into the input mixers
    amixer cset name='Mixin Left Aux Left Switch' on
    amixer cset name='Mixin Right Aux Right Switch' off
    amixer cset name='Mixin Right Mixin Left Switch' on

    # Enable input PGA and ADC
    amixer cset name='ADC Switch' on
    amixer cset name='ADC Volume' 80%
    amixer cset name='Mixin PGA Switch' on
    amixer cset name='Mixin PGA Volume' 50%

    # Select digital audio interface as DAC source
    amixer cset name='DAC Left Source MUX' 'DAI Input Left'
    amixer cset name='DAC Right Source MUX' 'DAI Input Right'

    # Route DAC to headphone mixer
    amixer cset name='Mixout Left DAC Left Switch' on
    # possibly remove this, might only be needed for mono guitar hack
    amixer cset name='Mixout Right DAC Right Switch' off

    # Enable headphone output
    amixer cset name='DAC Volume' 80%
    amixer cset name='Headphone Volume' 80%
    amixer cset name='Headphone Switch' on 

Fun Stuff
+++++++++

The codec has two mems mics that can be configured through ``alsamixer`` as well as some on board filtering and waveform generation, very fun to mess around with!

If you just want to record/play ``.wav`` files, the commands below can help!

.. code-block:: bash

   # this will play the .wav file to the speakers
   aplay -D plughw:0,0 stairway.wav

   # this will record across the aux input
   arecord -D plughw:0,0 -f S16_LE -r 48000 -c 2 -d 10 line_input.wav