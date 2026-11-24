PocketBeagle 2 Foot Switch and Encoder
======================================

This page connects a real foot switch and rotary encoder to the PocketBeagle 2 and sends them to the :doc:`mock orchestrator listener <mock_listener>`. Pressing the foot switch sends ``GS_MSG_BUTTON`` messages and turning the encoder sends ``GS_MSG_TWEAK`` messages, the same way the input service will on the finished pedal.

It works on both the BeagleBoard Debian image and the GatorStomp Yocto image, using the same overlay and the same sender. The only difference is how the encoder is read.

On a kernel with the ``rotary-encoder`` driver (the Yocto image)::

    Foot switch -> P2.22 -> gpio-keys driver -> /dev/input/eventN -> pedal_sender.py
    Encoder pins A and B -> P2.18 and P2.20 -> rotary-encoder driver -> /dev/input/eventN -> pedal_sender.py
    pedal_sender.py -> Unix socket -> mock_listener

On a kernel without it (the Debian image)::

    Foot switch -> P2.22 -> gpio-keys driver -> /dev/input/eventN -> pedal_sender.py
    Encoder pin A -> P2.18 -> gpiod -> pedal_sender.py
    Encoder pin B -> P2.20 -> gpiod -> pedal_sender.py
    pedal_sender.py -> Unix socket -> mock_listener

``pedal_sender.py`` is a prototype of the input service. It reads both controls in one process, since the listener accepts one client at a time, and checks at startup which way to read the encoder.

Parts
-----

* **Foot switch:** momentary, normally open SPST with 2 solder lugs. It only connects while pressed, and either lug can go to either pin.
* **Rotary encoder:** EC11 with a push button, 5 pins, 20 clicks and 20 pulses per turn. The 3 pin side is A, C (common, the middle pin) and B. The 2 pin side is the push button.

No resistors are needed. Every pin uses the processor's internal pull up.

Wiring
------

.. warning::

   Only change the wiring while the board is off and unplugged. Never connect anything to 5 V or VSYS, and keep wires off the pins marked below. Voltage on an I/O pin while the board is unpowered can damage the processor.

The P2 header has two rows. The board is not labelled pin by pin, but the end of the header is marked 35 and 36. With that end on the left and 36 in the top row, the top row holds the even pins and the bottom row the odd pins, and each column moves down by 2. Count columns from the 35/36 end:

.. list-table::
   :header-rows: 1

   * - Column
     - Top (even)
     - Bottom (odd)
   * - 1
     - P2.36
     - P2.35
   * - ...
     - ...
     - ...
   * - 7
     - P2.24
     - P2.23 (3.3 V, avoid)
   * - 8
     - P2.22
     - P2.21 (GND)
   * - 9
     - P2.20
     - P2.19
   * - 10
     - P2.18
     - P2.17
   * - 11
     - P2.16
     - P2.15 (GND)
   * - 12
     - P2.14
     - P2.13 (VSYS, avoid)

Connect the controls like this. The encoder's common pin and push button need GND too, so run P2.15 to the breadboard's ground rail and connect them there.

.. list-table::
   :header-rows: 1

   * - Control pin
     - Board pin
     - GPIO
   * - Foot switch lug 1
     - P2.22
     - GPIO0_63
   * - Foot switch lug 2
     - P2.21 (GND)
     -
   * - Encoder A
     - P2.18
     - GPIO0_53
   * - Encoder B
     - P2.20
     - GPIO0_49
   * - Encoder C (middle)
     - ground rail
     -
   * - Encoder push pin 1
     - P2.24
     - GPIO0_51
   * - Encoder push pin 2
     - ground rail
     -
   * - Ground rail
     - P2.15 (GND)
     -

On the breadboard, make sure no two encoder pins share a connected strip. Placing the encoder across the center gap usually does this.

These pins do not overlap with the McASP0 audio pins used by the Yocto image (P1.04, P1.10, P1.12 and P2.01).

The overlay
-----------

The header pins start out with no pull up, so an open switch does not read as a steady value. The device tree overlay ``tools/mock_listener/k3-am62-pocketbeagle2-gatorstomp-controls.dts`` fixes that at boot:

* It sets all four pins to GPIO input with the internal pull up, so each pin reads high when its contact is open and low when it is pressed.
* It hands the foot switch and the encoder push to the ``gpio-keys`` driver, which reports them as ``BTN_0`` and ``BTN_TRIGGER_HAPPY1`` input events with a 10 ms debounce. The input device is named ``gatorstomp-controls``.
* It hands encoder A and B to the ``rotary-encoder`` driver, which reports each click as ``+1`` or ``-1`` on ``REL_DIAL``. The input device is named ``gatorstomp-encoder-0``. On a kernel without this driver, the node is left unused and A and B stay free for ``pedal_sender.py`` to read with ``gpiod``.

How the overlay gets loaded depends on the image.

Installing on the Debian image
------------------------------

This assumes the board is set up as described in :doc:`demos/pocketbeagle2_button_demo`, with SSH working and the listener built in ``~/gatorstomp_demo``.

From the repo root in WSL, copy the overlay and the sender to the board:

.. code-block:: bash

    ssh <user>@192.168.7.2 'mkdir -p ~/gatorstomp_demo/overlay'
    scp tools/mock_listener/k3-am62-pocketbeagle2-gatorstomp-controls.dts <user>@192.168.7.2:gatorstomp_demo/overlay/
    scp tools/mock_listener/pedal_sender.py <user>@192.168.7.2:gatorstomp_demo/

On the board, compile the overlay and copy it next to the image's other overlays. The boot partition is writable by the default user, so this does not need ``sudo``:

.. code-block:: bash

    cd ~/gatorstomp_demo/overlay
    dtc -@ -I dts -O dtb -o k3-am62-pocketbeagle2-gatorstomp-controls.dtbo k3-am62-pocketbeagle2-gatorstomp-controls.dts
    cp k3-am62-pocketbeagle2-gatorstomp-controls.dtbo /boot/firmware/overlays/

Then tell the bootloader to load it. Make a backup of ``/boot/firmware/extlinux/extlinux.conf`` and add one line under ``label microSD (default)``, below the commented out ``fdtoverlays`` example:

.. code-block:: text

    label microSD (default)
        ...
        fdtdir /
        #fdtoverlays /overlays/<file>.dtbo
        fdtoverlays /overlays/k3-am62-pocketbeagle2-gatorstomp-controls.dtbo

Reboot with ``sudo reboot``. To check that the overlay loaded:

.. code-block:: bash

    tr -d '\0' < /proc/device-tree/chosen/overlays/gatorstomp-controls; echo
    cat /sys/class/input/event*/device/name
    gpiodetect
    gpioinfo -c gpiochipN 49 51 53 63

The first command prints ``GatorStomp foot switch and encoder``, and the input device list includes ``gatorstomp-controls`` but not ``gatorstomp-encoder-0``, since this kernel has no ``rotary-encoder`` driver. Replace ``N`` with the number of the ``600000.gpio`` chip from ``gpiodetect``. Lines 51 and 63 show ``GatorStomp Encoder 0 Push`` and ``GatorStomp Foot Switch 0`` as their consumers, and lines 49 and 53 are free.

To undo it, delete the ``fdtoverlays`` line and reboot. If the board ever fails to boot after a change here, choose ``microSD (failsafe)`` from the boot menu on the serial console. That entry does not load any overlays.

Installing on the Yocto image
-----------------------------

This assumes the board is running the GatorStomp Yocto image and reachable as described in :doc:`running_os`. Log in as ``root`` with no password. The steps below were tested with the image built on 2026-11-18.

The image's bootloader loads ``/boot/dtb/ti/k3-am62-pocketbeagle2.dtb`` from the root partition and does not read an overlay list that can be edited on the board. For a manual test, the overlay is merged into that file instead.

Run the commands below from the repo root in WSL. Merging needs ``dtc`` and ``fdtoverlay`` from the ``device-tree-compiler`` package (``sudo apt install device-tree-compiler``). The tested setup ran the same two tools on the Debian image instead, which includes them.

.. note::

   This changes one file on the SD card by hand. Reflashing the card puts the original back.

The image has no ``scp`` support, so files are copied by piping them through ``ssh``. Copy the image's own device tree off the board, merge the overlay into it, and keep the original as a backup:

.. code-block:: bash

    ssh root@192.168.7.2 'cat /boot/dtb/ti/k3-am62-pocketbeagle2.dtb' > base.dtb
    dtc -@ -I dts -O dtb -o controls.dtbo tools/mock_listener/k3-am62-pocketbeagle2-gatorstomp-controls.dts
    fdtoverlay -i base.dtb -o k3-am62-pocketbeagle2.dtb controls.dtbo

    ssh root@192.168.7.2 'cp /boot/dtb/ti/k3-am62-pocketbeagle2.dtb /boot/dtb/ti/k3-am62-pocketbeagle2.dtb.orig'
    ssh root@192.168.7.2 'cat > /boot/dtb/ti/k3-am62-pocketbeagle2.dtb' < k3-am62-pocketbeagle2.dtb

The image has no compiler, so the listener has to be built for ARM64 elsewhere. A static build has no library dependencies, so it runs on the Yocto image unchanged. The tested build was made in ``~/gatorstomp_demo`` on the Debian image:

.. code-block:: bash

    g++ -std=c++17 -Wall -Wextra -O2 -static -I include main.cpp -o mock_listener_static

:doc:`cross_compile` describes building with Zig from Windows instead. For C++ use ``zig c++`` in place of ``zig cc``.

Copy the listener and the sender to the board:

.. code-block:: bash

    ssh root@192.168.7.2 'mkdir -p ~/gatorstomp_demo'
    ssh root@192.168.7.2 'cat > ~/gatorstomp_demo/mock_listener && chmod +x ~/gatorstomp_demo/mock_listener' < mock_listener_static
    ssh root@192.168.7.2 'cat > ~/gatorstomp_demo/pedal_sender.py' < tools/mock_listener/pedal_sender.py

Reboot with ``reboot``. To check that the controls loaded:

.. code-block:: bash

    tr -d '\0' < /proc/device-tree/chosen/overlays/gatorstomp-controls; echo
    cat /sys/class/input/event*/device/name
    ls -l /sys/bus/platform/devices/gatorstomp-encoder-0/driver

The input device list includes both ``gatorstomp-encoder-0`` and ``gatorstomp-controls``, and the encoder's driver link points to ``rotary-encoder``.

To undo it, copy ``k3-am62-pocketbeagle2.dtb.orig`` back over ``k3-am62-pocketbeagle2.dtb`` and reboot.

Running the demo
----------------

Open two SSH sessions to the board. In the first, start the listener:

.. code-block:: bash

    cd ~/gatorstomp_demo
    ./mock_listener /tmp/gs.sock

In the second, start the pedal sender:

.. code-block:: bash

    cd ~/gatorstomp_demo
    python3 pedal_sender.py /tmp/gs.sock

The sender first says how it is reading the encoder. On the Yocto image:

.. code-block:: text

    Encoder 0: read from the rotary-encoder driver (/dev/input/event0)

On the Debian image:

.. code-block:: text

    Encoder 0: no rotary-encoder driver, decoding A and B with gpiod

It then reports the foot switch state. Each foot switch press and release shows up in the listener as one ``BUTTON`` line, and each click of the encoder as one ``TWEAK`` line, with clockwise as ``1`` and counterclockwise as ``-1``:

.. code-block:: text

    Client connected
    [1794638402113 ms] BUTTON  id=0  value=0 (released)
    [1794638405730 ms] BUTTON  id=0  value=1 (pressed)
    [1794638405911 ms] BUTTON  id=0  value=0 (released)
    [1794638408264 ms] TWEAK   id=0  value=1
    [1794638408502 ms] TWEAK   id=0  value=1
    [1794638410117 ms] TWEAK   id=0  value=-1

Pushing the encoder only prints a line in the sender's terminal, since the protocol does not have a message type for it yet.

Press Ctrl+C in either session to stop.

How the encoder is read
-----------------------

The encoder has two contacts, A and B, that open and close a quarter cycle apart as the knob turns. One click passes through four states, and the order tells the direction:

.. code-block:: text

    A B
    1 1 resting in a click
    0 1 A closes first when turning clockwise
    0 0
    1 0
    1 1 next click

At startup, ``pedal_sender.py`` looks for the ``gatorstomp-encoder-0`` input device.

* **If it exists,** the kernel's ``rotary-encoder`` driver is doing the decoding. The overlay lists A then B, both active low, and sets one click per full cycle (``rotary-encoder,steps-per-period = <1>``), so each click arrives as one input event of ``+1`` (clockwise) or ``-1``. The sender passes it on as a ``TWEAK`` message.
* **If it does not,** the sender reads A and B with ``gpiod`` and decodes them itself. It watches both pins for every change, looks up each change from one state to the next as +1, -1 or 0, and adds them up. When the knob is back at rest, it sends one ``TWEAK`` message in the direction of the total. Contact bounce only flips between two neighbouring states, so it adds up to zero and never sends a message. ``gpiod`` is only imported in this case, so the sender does not need it on a kernel with the driver.
