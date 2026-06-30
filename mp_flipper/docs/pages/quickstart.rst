Quickstart
==========

This page provides some details on how to start and use the application.

Basics
------

How to install the application and run a script:

1. Install the application from the `Flipper Lab <https://lab.flipper.net/apps/upython>`_ on your Flipper device.
2. Write some Python code or use one of the provided `examples <https://github.com/ofabel/mp-flipper/tree/master/examples>`_.
3. Use the `qFlipper <https://flipperzero.one/update>`_ application to upload the code to your Flipper's SD card.
4. Start the **uPython** application on your Flipper to run your Python script.

Instead of running a script, you could also use the interactive MicroPython shell from the terminal.
Visit the `Flipper documentation <https://docs.flipper.net/development/cli>`_ for details about the CLI in general.

.. hint::

   Looking for a more efficient solution to copy your files or start a CLI session?
   Try the `Flipper Zero Script SDK <https://github.com/ofabel/fssdk>`_ helper.

In case your Flipper is not responding or a script doesn't behave as expected and won't finish: `do a reboot <https://docs.flipper.net/basics/reboot>`_ by pressing and holding **Left** and **Back** for 5 seconds.

Usage
-----

Due to the occasional crashes upon application start, the application is especially designed to run in the background while working on the CLI from the computer.

You can use the CLI to start the application:

.. code-block:: shell

   loader open /ext/apps/Tools/upython.fap

You can also use the CLI to start the application and run a Python script:

.. code-block:: shell

   loader open /ext/apps/Tools/upython.fap /ext/scripts/tic_tac_toe.py

Once the application is up an running, it registers the **py** command in the Flipper's CLI command registry.
You can use this command to access the interactive MicroPython shell or start a script by passing the path as an argument:

.. code-block:: shell

   py /ext/scripts/tic_tac_toe.py

Be aware, that the **py** command is only available as long as the application is running on the Flipper.
Furthermore, you can only run one script at the time.
