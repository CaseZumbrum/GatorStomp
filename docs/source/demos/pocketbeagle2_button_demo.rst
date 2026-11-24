PocketBeagle 2 Button Demo
==========================

This demo runs the :doc:`mock orchestrator listener <mock_listener>` on the PocketBeagle 2 itself and drives it with a real button. Pressing the board's USER button sends ``GS_MSG_BUTTON`` messages over the orchestrator socket, the same way a foot switch will once the input service exists.

The pipeline is::

    USER button -> GPIO0_26 -> gpio_button_sender.py -> Unix socket -> mock_listener

``gpio_button_sender.py`` stands in for the input service. It waits for the GPIO line to change, then sends one ``gs_socket_msg`` per press and release.

Setting up the board
--------------------

The demo was tested on the **PocketBeagle 2 Debian 13.7 2026-10-09 IoT** image from the `BeagleBoard distros page <https://www.beagleboard.org/distros>`_.

1. Write the image to a microSD card (8 GB or larger). The BeagleBoard Imaging Utility works, and so does Raspberry Pi Imager using "Use custom" (answer No to its OS customisation settings).
2. Before the first boot, open ``sysconf.txt`` on the card's ``BOOT`` partition and set ``user_name``, ``user_password`` and ``hostname`` by removing the ``#`` in front of each line. If Windows does not give ``BOOT`` a drive letter, add one in Disk Management.
3. Put the card in the board and connect it to a computer with a USB C data cable. The first boot takes a few minutes. Later boots take about a minute.
4. Log in over the USB network:

.. code-block:: bash

    ssh <user>@192.168.7.2

Which button is which
---------------------

With the board face up and the USB C port on the left, both buttons sit along the bottom edge below the USB C port. The left one is **POWER** and the right one is **USER**.

.. warning::

   A short press of the POWER button tells Linux to shut down, and holding it cuts power. Use ``sudo poweroff`` to turn the board off.

How the USER button is wired
----------------------------

The USER button is not a normal GPIO input. It feeds the boot mode circuit, which is why the stock image has no input device for it. From the schematic in the PocketBeagle 2 documentation (Fig. 3.36 Buttons, Fig. 3.11 Bootstrap pins connection and Fig. 3.12 GPIO GPMC):

* The button pulls ``USER_BTN`` to ground when pressed.
* Transistors Q1 and Q2 invert that signal onto ``BOOTMODE13`` and ``BOOTMODE11``, so those pins read 1 while the button is held and 0 when it is released.
* ``BOOTMODE11`` is the SoC pin ``GPMC0_AD11``, which is ``GPIO0_26``. ``BOOTMODE13`` is ``GPMC0_AD13``, which is ``GPIO0_28``. The demo uses ``GPIO0_26``.

The boot mode pins are only sampled at power up, so after boot they can be read like any other GPIO input. Only ever read them. Driving them as outputs would fight the transistors.

``GPIO0`` is the controller labelled ``600000.gpio``. Its ``gpiochip`` number can change between kernels, so find it with ``gpiodetect`` instead of assuming one. To check the button directly, replace ``N`` with that number and press the USER button:

.. code-block:: bash

    gpiodetect
    gpiomon -c gpiochipN 26

Each press prints a ``rising`` line and each release prints a ``falling`` line.

Copying and building
--------------------

WSL can reach the board over the USB network, but the board has no internet access of its own, so copy the files from the repo root in WSL:

.. code-block:: bash

    ssh <user>@192.168.7.2 'mkdir -p ~/gatorstomp_demo/include'
    scp src/gs_socket_api/include/packet_structure.h <user>@192.168.7.2:gatorstomp_demo/include/
    scp tools/mock_listener/main.cpp tools/mock_listener/gpio_button_sender.py <user>@192.168.7.2:gatorstomp_demo/

The IoT image has ``g++`` but not CMake, so build the listener directly on the board:

.. code-block:: bash

    cd ~/gatorstomp_demo
    g++ -std=c++17 -Wall -Wextra -I include main.cpp -o mock_listener

``gpio_button_sender.py`` uses the ``gpiod`` Python library, which the IoT image already includes.

Running the demo
----------------

Open two SSH sessions to the board. In the first, start the listener:

.. code-block:: bash

    cd ~/gatorstomp_demo
    ./mock_listener /tmp/gs.sock

In the second, start the button sender:

.. code-block:: bash

    cd ~/gatorstomp_demo
    python3 gpio_button_sender.py /tmp/gs.sock

The sender reports the button's current state as soon as it connects, then one message per press and release. The listener's output is the proof that each message crossed the socket:

.. code-block:: text

    Client connected
    [1793687137013 ms] BUTTON  id=0  value=0 (released)
    [1793687160577 ms] BUTTON  id=0  value=1 (pressed)
    [1793687160729 ms] BUTTON  id=0  value=0 (released)

Press Ctrl+C in either session to stop.

Notes
-----

The PocketBeagle 2 has no battery backed clock and no internet access over USB, so its clock starts from the image's build date after every boot. Presses still work, but the ``time_ms`` values will be wrong. To set the clock from the laptop, run this in WSL:

.. code-block:: bash

    ssh -t <user>@192.168.7.2 "sudo date -u -s '$(date -u +'%F %T')'"
