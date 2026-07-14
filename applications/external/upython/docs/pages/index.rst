.. toctree::
   :hidden:
   :maxdepth: 2

   quickstart
   reference
   features
   examples
   roadmap
   Changelog </changelog>
   License </license>

MicroPython on Flipper Zero
===========================

.. image:: https://img.shields.io/github/license/ofabel/mp-flipper
   :alt: License

.. image:: https://img.shields.io/github/v/tag/ofabel/mp-flipper
   :alt: Version

.. image:: https://img.shields.io/github/issues/ofabel/mp-flipper
   :alt: Issues

A `MicroPython <https://github.com/micropython/micropython>`_ port for the famous `Flipper Zero <https://flipperzero.one/>`_.
No need to learn C: Use your favourite programming language to create apps, games and scripts.

.. image:: ./assets/repl.gif
   :align: center

Features
--------

* Support for basic language constructs like functions, classes, loops, ...
* Access the Flipper's hardware: buttons, speaker, LED, GPIO, ADC, PWM ...
* No custom firmware is required, so no risk to brick your Flipper.

A complete list can be found in the :doc:`features </features>` section.

How to Start
------------

1. Install the application from the `Flipper Lab <https://lab.flipper.net/apps/upython>`_ on your Flipper device.
2. Write some Python code or use one of the provided `examples <https://github.com/ofabel/mp-flipper/tree/master/examples>`_.
3. Use the `qFlipper <https://flipperzero.one/update>`_ application to upload the code to your Flipper's SD card.
4. Start the **uPython** application on your Flipper to execute your Python script.

Checkout the :doc:`reference </reference>` section for an in-depth API documentation.

License
-------

The uPython application is published under the :doc:`MIT </license>` license.

