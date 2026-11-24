Mock Orchestrator Listener
==========================

The mock orchestrator listener stands in for the audio orchestrator's I/O handler. It creates the orchestrator's Unix socket, accepts a client, and prints every ``gs_socket_msg`` it receives. This lets the input service be built and tested before the real orchestrator is ready.

It lives in ``tools/mock_listener`` along with ``test_sender.py``, a small Python script that pretends to be the input service.

Building
--------

From the root of the repo:

.. code-block:: bash

    cmake -S . -B build
    cmake --build build --target mock_listener

Running
-------

The default socket path, ``/run/gatorstomp/orchestrator.sock``, needs root on a dev machine, so pass a path in ``/tmp`` instead. Start the listener in one terminal:

.. code-block:: bash

    ./build/bin/mock_listener /tmp/gs.sock

Then run the test sender in a second terminal with the same path:

.. code-block:: bash

    python3 tools/mock_listener/test_sender.py /tmp/gs.sock

Press Ctrl+C in the first terminal to stop the listener. It removes the socket file on the way out.

Interactive demo
----------------

``interactive_sender.py`` turns the keyboard into a pretend pedal, which is useful for live demos. Start the listener as above, then run this in the second terminal:

.. code-block:: bash

    python3 tools/mock_listener/interactive_sender.py /tmp/gs.sock

The left and right arrow keys turn the selected encoder by one step, and the up and down arrow keys choose which encoder (0 to 3) is selected. Keys 1 to 4 press and release foot switches 0 to 3. A terminal can't tell when a key is let go, so each press toggles the switch between pressed and released. Press ``b`` to send a broken message and ``q`` to quit.

Like the real input service, it sends the state of all 4 foot switches as soon as it connects. Each key press shows up as a line in the listener's terminal.

Example output
--------------

.. code-block:: text

    Mock orchestrator listening on /tmp/gs.sock (Ctrl+C to stop)
    Expecting 11 byte messages
    Waiting for a client...
    Client connected
    [1793680174617 ms] TWEAK   id=2  value=-1
    [1793680174818 ms] TWEAK   id=0  value=5
    [1793680175018 ms] BUTTON  id=1  value=1 (pressed)
    [1793680175218 ms] BUTTON  id=1  value=0 (released)
    [1793680175418 ms] PROTOCOL ERROR: type 3 (VOLUME) is not expected from the input service
    [1793680175619 ms] PROTOCOL ERROR: type 0 (ACK) is not expected from the input service
    [1793680175819 ms] PROTOCOL ERROR: type 9 (UNKNOWN) is not expected from the input service
    [1793680176019 ms] PROTOCOL ERROR: BUTTON id 7 is outside 0 to 3
    [1793680176220 ms] PROTOCOL ERROR: BUTTON value 2 should be 0 or 1
    PROTOCOL ERROR: got a 3 byte message, expected 11 bytes
    PROTOCOL ERROR: got a 12 byte message, expected 11 bytes
    Client disconnected
    Waiting for a client...

What it checks
--------------

Only ``GS_MSG_BUTTON`` and ``GS_MSG_TWEAK`` are expected from the input service. The listener logs a protocol error and keeps running when it receives:

* a message that is not exactly ``sizeof(gs_socket_msg)`` bytes
* any other message type, including ``GS_MSG_VOLUME`` and ``GS_MSG_ACK``
* an ``id`` outside 0 to 3
* a ``GS_MSG_BUTTON`` value other than 0 (released) or 1 (pressed)

When the client disconnects, the listener logs it and waits for a new connection, so the input service's reconnect behavior can be tested by stopping and restarting the sender. Any socket file left behind by an earlier run is removed at startup.
