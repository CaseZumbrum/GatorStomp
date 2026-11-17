
Cross Compilation and Script Execution
======================================

**Author:** Danush Singla

This guide explains how to compile and execute C, Python, and MATLAB
programs on the GatorStomp operating system running on the PocketBeagle 2.

These instructions are intended for **Windows computers**.

The development computer uses the x86-64 architecture, while the
PocketBeagle 2 uses ARM64 (AArch64). C programs must therefore be
cross-compiled for ARM64 Linux before execution.

Python and MATLAB-compatible scripts can be transferred directly
to the board and executed using their respective interpreters.

.. note::

   Before beginning, ensure that the PocketBeagle 2 is running the
   GatorStomp Yocto image and is connected to the Windows computer
   using a data-capable USB-C cable.

   The examples below use the board's default USB network address,
   ``192.168.7.2``.


C Code
------

C programs must be cross-compiled into ARM64 Linux executables before
they can run on the PocketBeagle 2.

We use the Zig compiler for cross-compilation. Zig can generate ARM64
Linux executables directly from Windows without requiring WSL or a
separate Yocto SDK.

Step 1: Download Zig
~~~~~~~~~~~~~~~~~~~~

Download the Windows x86-64 version of Zig from the official website:

`Download Zig <https://ziglang.org/download/>`_

Extract the downloaded ZIP archive to a directory on your computer.

Note the location of ``zig.exe``, which will be referred to as
``PATH_TO_ZIG\zig.exe`` throughout this guide.

.. note::

   Zig is only required on the Windows development computer.
   It does not need to be installed on the PocketBeagle 2.


Step 2: Cross-Compile the C File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open **Windows PowerShell** and navigate to the directory containing
the C file you want to compile.

For example, if your file is named ``hello.c``, run:

.. code-block:: powershell

   & "PATH_TO_ZIG\zig.exe" cc -target aarch64-linux-musl -static hello.c -o hello

Replace ``PATH_TO_ZIG`` with the actual directory containing Zig.

Replace ``hello.c`` with your source filename and ``hello`` with
the desired executable name.

The command uses the following options:

- ``cc`` invokes Zig's C compiler.
- ``-target aarch64-linux-musl`` targets ARM64 Linux using musl libc.
- ``-static`` produces a statically linked executable.
- ``-o hello`` specifies the output executable filename.

The resulting executable is intended for ARM64 Linux, not Windows.

.. note::

   Static compilation is useful for simple C programs because
   the executable does not depend on a separately installed C
   standard-library runtime.

   Programs using additional libraries or hardware-specific
   functionality may require extra dependencies or a different
   cross-compilation configuration.


Step 3: Transfer the Executable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure that the PocketBeagle 2 is powered on and connected to the
Windows computer through USB-C.

From PowerShell, navigate to the directory containing the compiled
executable.

Transfer it using SCP:

.. code-block:: powershell

   scp -O .\hello root@192.168.7.2:/home/root/hello

Replace ``hello`` with your executable's filename.

You may also change ``/home/root/hello`` to another destination
on the PocketBeagle 2.

The ``-O`` option uses the legacy SCP transfer protocol, which
is supported by our current Dropbear SSH configuration.


Step 4: Execute the Program
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Connect to the PocketBeagle 2 through SSH:

.. code-block:: powershell

   ssh root@192.168.7.2

Once connected, navigate to the executable's directory if necessary.

Grant executable permissions:

.. code-block:: bash

   chmod +x /home/root/hello

Then execute the program:

.. code-block:: bash

   /home/root/hello

Alternatively, from within ``/home/root``, run:

.. code-block:: bash

   ./hello

The C program will now execute on the PocketBeagle 2's ARM64 processor.


Python
------

Python does not require cross-compilation for ordinary Python scripts.

The GatorStomp Yocto image includes Python 3 and its standard-library
modules. Python files can therefore be transferred directly to the
PocketBeagle 2 and executed using the installed interpreter.

Step 1: Transfer the Python File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open Windows PowerShell in the directory containing your Python file.

For a file named ``hello.py``, run:

.. code-block:: powershell

   scp -O .\hello.py root@192.168.7.2:/home/root/

Replace ``hello.py`` with the filename you want to transfer.

Change ``/home/root/`` if you want the file stored in a different
directory on the board.


Step 2: Execute the Python File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Connect to the board:

.. code-block:: powershell

   ssh root@192.168.7.2

Execute the Python script:

.. code-block:: bash

   python3 /home/root/hello.py

Python will interpret and execute the file directly.

.. note::

   Python 3 and its standard-library modules are included in the
   GatorStomp image.

   Scripts requiring external packages, such as NumPy or SciPy,
   will also require those packages to be included in the OS image.


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


Step 1: Transfer the MATLAB File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open Windows PowerShell in the directory containing your ``.m`` file.

For a file named ``hello.m``, run:

.. code-block:: powershell

   scp -O .\hello.m root@192.168.7.2:/home/root/

Replace ``hello.m`` with your MATLAB filename.

The destination path may be changed if desired.


Step 2: Execute the MATLAB File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Connect to the PocketBeagle 2:

.. code-block:: powershell

   ssh root@192.168.7.2

Run the script using GNU Octave:

.. code-block:: bash

   octave-cli --quiet --no-history /home/root/hello.m

The command consists of:

- ``octave-cli``: Runs GNU Octave from the command line without
  launching its graphical interface.
- ``--quiet``: Suppresses Octave's startup messages.
- ``--no-history``: Disables command-history recording.
- ``/home/root/hello.m``: Specifies the script to execute.

The ``--quiet`` option is optional and does not suppress normal
script output or error messages.

You can also run the script without it:

.. code-block:: bash

   octave-cli /home/root/hello.m


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


Summary
-------

The GatorStomp operating system supports three different workflows:

- **C:** Cross-compile on Windows, transfer the ARM64 executable,
  and execute it on the board.
- **Python:** Transfer the ``.py`` file and execute it using Python 3.
- **MATLAB:** Transfer the ``.m`` file and execute it using GNU Octave,
  once Octave support is verified.

C and Python workflows have been tested on the physical
PocketBeagle 2. GNU Octave support remains pending final verification.

All execution occurs on the PocketBeagle 2 using the GatorStomp
Yocto operating system.