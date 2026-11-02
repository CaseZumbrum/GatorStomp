Setting up Yocto to work
========================

Follow the `Yocto Project Quick Build guide <https://docs.yoctoproject.org/brief-yoctoprojectqs/index.html>`_ (latest version) for general commands relating to Yocto.

To start editing the Yocto OS layer on your computer, please follow these exact steps:

Step 1: Setting up dependencies
-------------------------------

Use Ubuntu on Linux or WSL2. Run this command to install basic functionalities:

.. code-block:: bash

   sudo apt install build-essential chrpath cpio debianutils diffstat file gawk gcc git iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 python3-pexpect python3-pip python3-subunit socat texinfo unzip wget xz-utils zstd

Also install kas with these commands:

.. code-block:: bash

   sudo apt install pipx
   pipx ensurepath
   pipx install kas

.. note::

   After running ``pipx ensurepath``, you may need to restart your terminal before running ``pipx install kas``.

Step 2: Setting up the repository
---------------------------------

Clone the GatorStomp Project:

.. note::

   Use the SSH link instead of the HTTPS one if you need it for authentication.

.. code-block:: bash

   git clone https://github.com/CaseZumbrum/GatorStomp.git GatorStomp

Run this command to restore the Yocto environment.

This will install dependencies like Poky, and the ARM, TI, and other layers necessary for the OS.

.. code-block:: bash

   kas checkout GatorStomp/kas.yml

Step 3: Resume development
--------------------------

You may start working in the ``GatorStomp/meta-gatorstomp/`` directory, where the OS layer lives.

Building the OS
---------------

To build, run:

.. code-block:: bash

   kas build GatorStomp/kas.yml

This will use BitBake within Kas to produce the GatorStomp Linux image.

The ``.wic`` disk image is generated from the first build, which can be flashed to a microSD on the physical PocketBeagle 2.

Important things to note
------------------------

- Please do not commit Yocto build directories, caches, or any downloaded dependency repositories. Add them to the ``.gitignore`` whenever possible.
- ``kas.lock.yml`` just exists to specify commits for reproducibility from dependency layers.
- The PocketBeagle 2 OS image is only meant for the physical board, and not to be used on any PC operating system.