Cross Compilation and Script Execution
======================================

**Author:** Danush Singla and Case Zumbrum

This guide explains how to compile and execute C, Python, and MATLAB
programs on the GatorStomp operating system running on the PocketBeagle 2.

.. note::

   Before beginning, ensure that the PocketBeagle 2 is running the
   GatorStomp Yocto image and is connected to the Windows computer
   using a data-capable USB-C cable.

   The examples below use the board's default USB network address,
   ``192.168.7.2``.


C/C++ Code
------

Since the PocketBeagle has an Arm64 architecture, and most development is done on x86 processors it is necessary to cross-compile to Arm64.

Luckily, since we are using CMake, this is pretty easy!

Setup
+++++

You will need to install a few things to get started.

You can use the `cross_compilation_setup.sh` script to do all of this for you if you wish, or follow below for more specific instructions.

.. code-block:: bash

   source ./cross_compilation_setup.sh

First is installing the actual cross compiler

.. code-block:: bash

   # install the cross compiler
   sudo apt install g++-aarch64-linux-gnu -y

The biggest thing necessary is the Arm64 ALSA binaries, the easiest way to do this is to download them pre-compiled from ALSA (but of course you can also compile your own)

.. code-block:: bash

   # make the cross dependencies directory
   mkdir -p ./cross_deps/alsa
   cd ./cross_deps/alsa

   # download the binaries
   wget http://ports.ubuntu.com/ubuntu-ports/pool/main/a/alsa-lib/libasound2-dev_1.2.6.1-1ubuntu1_arm64.deb
   wget http://ports.ubuntu.com/ubuntu-ports/pool/main/a/alsa-lib/libasound2_1.2.6.1-1ubuntu1_arm64.deb

   # install them locally
   dpkg-deb -x libasound2-dev_*.deb .
   dpkg-deb -x libasound2_*.deb .

Lastly is setting up CMake

.. code-block:: bash

   cmake -S . -B cross_build -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DALSA_INCLUDE_DIR=$PWD/cross_deps/alsa/usr/include -DALSA_LIBRARY=$PWD/cross_deps/alsa/usr/lib/aarch64-linux-gnu/libasound.so


Building
++++++++

With cmake setup it is real easy to build now!

.. code-block:: bash

   cd cross_build
   # optionally you can specify a target here!
   make

Transferring to the Board
+++++++++++++++++++++++++

Ensure that the PocketBeagle 2 is powered on and connected to the
Windows computer through USB-C.

You can use scp to send files directly

.. code-block:: bash

   scp -O [file to transfer] root@192.168.7.2:/home/root/hello

.. note::

   You can also use something like FileZilla to make this process easier!


Executing on the Board
+++++++++++++++++++++++++

.. code-block:: bash

   ssh root@192.168.7.2
   chmod +x [file to execute]
   ./[file to execute]


.. note:: 

   there is a full demo in `demos/beagle_demo.sh` that will run do a full audio io demo!


Python
------

The yocto build that GatorStomp uses has a python interpreter on it already, so you can just transfer your files (as seen above) and run them normally via `python3`

.. note::

   There is no external dependencies in this, to use them you will need to transfer the wheelhouses over to the beagle


MATLAB / GNU Octave
-------------------

The GatorStomp Yocto operating system includes GNU Octave,
an open-source numerical computing environment that supports
MATLAB-compatible ``.m`` files.

MATLAB scripts do not need to be cross-compiled. They can be
transferred directly to the PocketBeagle 2 and executed using
the installed GNU Octave interpreter.

.. note::

   GNU Octave supports many MATLAB functions, but some proprietary
   MATLAB toolboxes and functions may not be compatible.

.. code-block:: bash

   octave-cli --quiet --no-history [file to execute]



Troubleshooting
---------------

SSH Host Identification Has Changed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When reconnecting to the PocketBeagle 2 after flashing a new
operating system image, SSH may display the following warning:

.. code-block:: text

   WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!

This occurs when the SSH host key presented by the board differs
from the key previously saved on the Windows computer.

If you have verified that you are connecting to your own PocketBeagle 2,
remove the previously saved key:

.. code-block:: powershell

   ssh-keygen -R 192.168.7.2

Then reconnect:

.. code-block:: powershell

   ssh root@192.168.7.2

Verify the new host key before accepting it.

Connection Timed Out
~~~~~~~~~~~~~~~~~~~~

If SSH or SCP cannot connect, first verify that the board is
powered on and recognized by Windows as a USB network adapter.

Test the network connection:

.. code-block:: powershell

   ping 192.168.7.2

If the ping fails, check the USB-C cable and confirm that Windows
has received an address on the ``192.168.7.x`` subnet.

You can inspect network addresses using:

.. code-block:: powershell

   ipconfig