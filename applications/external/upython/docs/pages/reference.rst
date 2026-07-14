Reference
=========

This page contains the API documentation of the ``flipperzero`` module and some built-in functions.
The module is also available as a Python package on `PyPI <https://pypi.org/project/flipperzero/>`_.
Install it in your local development environment if you need code completion support inside your IDE.

Vibration
---------

Control the vibration motor of your Flipper.

.. autofunction:: flipperzero.vibro_set

.. _reference-light:

Light
-----

Control the RGB LED and display backlight of your Flipper.

Constants
~~~~~~~~~

.. autodata:: flipperzero.LIGHT_RED
.. autodata:: flipperzero.LIGHT_GREEN
.. autodata:: flipperzero.LIGHT_BLUE
.. autodata:: flipperzero.LIGHT_BACKLIGHT

Functions
~~~~~~~~~

.. autofunction:: flipperzero.light_set
.. autofunction:: flipperzero.light_blink_start
.. autofunction:: flipperzero.light_blink_set_color
.. autofunction:: flipperzero.light_blink_stop

.. _reference-speaker:

Speaker
-------

Full control over the built-in speaker module.

Musical Notes
~~~~~~~~~~~~~

Constant values for all musical notes between C\ :sub:`0` and B\ :sub:`8`.

.. warning::

   You won't be able to find these constants in the REPL using autocompletion from version 1.7.0 onwards.
   But the constants are still available. You just have to type the full name by hand.

..
   for octave in range(9):
      for name in ['C', 'CS', 'D', 'DS', 'E', 'F', 'FS', 'G', 'GS', 'A', 'AS', 'B']:
         print(f'.. autodata:: flipperzero.SPEAKER_NOTE_{name}{octave}')

.. autodata:: flipperzero.SPEAKER_NOTE_C0
.. autodata:: flipperzero.SPEAKER_NOTE_CS0
.. autodata:: flipperzero.SPEAKER_NOTE_D0
.. autodata:: flipperzero.SPEAKER_NOTE_DS0
.. autodata:: flipperzero.SPEAKER_NOTE_E0
.. autodata:: flipperzero.SPEAKER_NOTE_F0
.. autodata:: flipperzero.SPEAKER_NOTE_FS0
.. autodata:: flipperzero.SPEAKER_NOTE_G0
.. autodata:: flipperzero.SPEAKER_NOTE_GS0
.. autodata:: flipperzero.SPEAKER_NOTE_A0
.. autodata:: flipperzero.SPEAKER_NOTE_AS0
.. autodata:: flipperzero.SPEAKER_NOTE_B0
.. autodata:: flipperzero.SPEAKER_NOTE_C1
.. autodata:: flipperzero.SPEAKER_NOTE_CS1
.. autodata:: flipperzero.SPEAKER_NOTE_D1
.. autodata:: flipperzero.SPEAKER_NOTE_DS1
.. autodata:: flipperzero.SPEAKER_NOTE_E1
.. autodata:: flipperzero.SPEAKER_NOTE_F1
.. autodata:: flipperzero.SPEAKER_NOTE_FS1
.. autodata:: flipperzero.SPEAKER_NOTE_G1
.. autodata:: flipperzero.SPEAKER_NOTE_GS1
.. autodata:: flipperzero.SPEAKER_NOTE_A1
.. autodata:: flipperzero.SPEAKER_NOTE_AS1
.. autodata:: flipperzero.SPEAKER_NOTE_B1
.. autodata:: flipperzero.SPEAKER_NOTE_C2
.. autodata:: flipperzero.SPEAKER_NOTE_CS2
.. autodata:: flipperzero.SPEAKER_NOTE_D2
.. autodata:: flipperzero.SPEAKER_NOTE_DS2
.. autodata:: flipperzero.SPEAKER_NOTE_E2
.. autodata:: flipperzero.SPEAKER_NOTE_F2
.. autodata:: flipperzero.SPEAKER_NOTE_FS2
.. autodata:: flipperzero.SPEAKER_NOTE_G2
.. autodata:: flipperzero.SPEAKER_NOTE_GS2
.. autodata:: flipperzero.SPEAKER_NOTE_A2
.. autodata:: flipperzero.SPEAKER_NOTE_AS2
.. autodata:: flipperzero.SPEAKER_NOTE_B2
.. autodata:: flipperzero.SPEAKER_NOTE_C3
.. autodata:: flipperzero.SPEAKER_NOTE_CS3
.. autodata:: flipperzero.SPEAKER_NOTE_D3
.. autodata:: flipperzero.SPEAKER_NOTE_DS3
.. autodata:: flipperzero.SPEAKER_NOTE_E3
.. autodata:: flipperzero.SPEAKER_NOTE_F3
.. autodata:: flipperzero.SPEAKER_NOTE_FS3
.. autodata:: flipperzero.SPEAKER_NOTE_G3
.. autodata:: flipperzero.SPEAKER_NOTE_GS3
.. autodata:: flipperzero.SPEAKER_NOTE_A3
.. autodata:: flipperzero.SPEAKER_NOTE_AS3
.. autodata:: flipperzero.SPEAKER_NOTE_B3
.. autodata:: flipperzero.SPEAKER_NOTE_C4
.. autodata:: flipperzero.SPEAKER_NOTE_CS4
.. autodata:: flipperzero.SPEAKER_NOTE_D4
.. autodata:: flipperzero.SPEAKER_NOTE_DS4
.. autodata:: flipperzero.SPEAKER_NOTE_E4
.. autodata:: flipperzero.SPEAKER_NOTE_F4
.. autodata:: flipperzero.SPEAKER_NOTE_FS4
.. autodata:: flipperzero.SPEAKER_NOTE_G4
.. autodata:: flipperzero.SPEAKER_NOTE_GS4
.. autodata:: flipperzero.SPEAKER_NOTE_A4
.. autodata:: flipperzero.SPEAKER_NOTE_AS4
.. autodata:: flipperzero.SPEAKER_NOTE_B4
.. autodata:: flipperzero.SPEAKER_NOTE_C5
.. autodata:: flipperzero.SPEAKER_NOTE_CS5
.. autodata:: flipperzero.SPEAKER_NOTE_D5
.. autodata:: flipperzero.SPEAKER_NOTE_DS5
.. autodata:: flipperzero.SPEAKER_NOTE_E5
.. autodata:: flipperzero.SPEAKER_NOTE_F5
.. autodata:: flipperzero.SPEAKER_NOTE_FS5
.. autodata:: flipperzero.SPEAKER_NOTE_G5
.. autodata:: flipperzero.SPEAKER_NOTE_GS5
.. autodata:: flipperzero.SPEAKER_NOTE_A5
.. autodata:: flipperzero.SPEAKER_NOTE_AS5
.. autodata:: flipperzero.SPEAKER_NOTE_B5
.. autodata:: flipperzero.SPEAKER_NOTE_C6
.. autodata:: flipperzero.SPEAKER_NOTE_CS6
.. autodata:: flipperzero.SPEAKER_NOTE_D6
.. autodata:: flipperzero.SPEAKER_NOTE_DS6
.. autodata:: flipperzero.SPEAKER_NOTE_E6
.. autodata:: flipperzero.SPEAKER_NOTE_F6
.. autodata:: flipperzero.SPEAKER_NOTE_FS6
.. autodata:: flipperzero.SPEAKER_NOTE_G6
.. autodata:: flipperzero.SPEAKER_NOTE_GS6
.. autodata:: flipperzero.SPEAKER_NOTE_A6
.. autodata:: flipperzero.SPEAKER_NOTE_AS6
.. autodata:: flipperzero.SPEAKER_NOTE_B6
.. autodata:: flipperzero.SPEAKER_NOTE_C7
.. autodata:: flipperzero.SPEAKER_NOTE_CS7
.. autodata:: flipperzero.SPEAKER_NOTE_D7
.. autodata:: flipperzero.SPEAKER_NOTE_DS7
.. autodata:: flipperzero.SPEAKER_NOTE_E7
.. autodata:: flipperzero.SPEAKER_NOTE_F7
.. autodata:: flipperzero.SPEAKER_NOTE_FS7
.. autodata:: flipperzero.SPEAKER_NOTE_G7
.. autodata:: flipperzero.SPEAKER_NOTE_GS7
.. autodata:: flipperzero.SPEAKER_NOTE_A7
.. autodata:: flipperzero.SPEAKER_NOTE_AS7
.. autodata:: flipperzero.SPEAKER_NOTE_B7
.. autodata:: flipperzero.SPEAKER_NOTE_C8
.. autodata:: flipperzero.SPEAKER_NOTE_CS8
.. autodata:: flipperzero.SPEAKER_NOTE_D8
.. autodata:: flipperzero.SPEAKER_NOTE_DS8
.. autodata:: flipperzero.SPEAKER_NOTE_E8
.. autodata:: flipperzero.SPEAKER_NOTE_F8
.. autodata:: flipperzero.SPEAKER_NOTE_FS8
.. autodata:: flipperzero.SPEAKER_NOTE_G8
.. autodata:: flipperzero.SPEAKER_NOTE_GS8
.. autodata:: flipperzero.SPEAKER_NOTE_A8
.. autodata:: flipperzero.SPEAKER_NOTE_AS8
.. autodata:: flipperzero.SPEAKER_NOTE_B8

Volume
~~~~~~

.. autodata:: flipperzero.SPEAKER_VOLUME_MIN
.. autodata:: flipperzero.SPEAKER_VOLUME_MAX

Functions
~~~~~~~~~

.. autofunction:: flipperzero.speaker_start
.. autofunction:: flipperzero.speaker_set_volume
.. autofunction:: flipperzero.speaker_stop

.. _reference-input:

Input
-----

Make your application interactive with full control over the Flipper's hardware buttons.

Buttons
~~~~~~~

.. autodata:: flipperzero.INPUT_BUTTON_UP
.. autodata:: flipperzero.INPUT_BUTTON_DOWN
.. autodata:: flipperzero.INPUT_BUTTON_RIGHT
.. autodata:: flipperzero.INPUT_BUTTON_LEFT
.. autodata:: flipperzero.INPUT_BUTTON_OK
.. autodata:: flipperzero.INPUT_BUTTON_BACK

Events
~~~~~~

.. autodata:: flipperzero.INPUT_TYPE_PRESS
.. autodata:: flipperzero.INPUT_TYPE_RELEASE
.. autodata:: flipperzero.INPUT_TYPE_SHORT
.. autodata:: flipperzero.INPUT_TYPE_LONG
.. autodata:: flipperzero.INPUT_TYPE_REPEAT

Functions
~~~~~~~~~

.. autodecorator:: flipperzero.on_input

.. _reference-canvas:

Canvas
------

Write text and draw dots and shapes on the the display.

Basics
~~~~~~

.. autofunction:: flipperzero.canvas_update
.. autofunction:: flipperzero.canvas_clear
.. autofunction:: flipperzero.canvas_width
.. autofunction:: flipperzero.canvas_height

Colors
~~~~~~

.. autodata:: flipperzero.COLOR_BLACK
.. autodata:: flipperzero.COLOR_WHITE
.. autofunction:: flipperzero.canvas_set_color

Alignment
~~~~~~~~~

.. autodata:: flipperzero.ALIGN_BEGIN
.. autodata:: flipperzero.ALIGN_END
.. autodata:: flipperzero.ALIGN_CENTER
.. autofunction:: flipperzero.canvas_set_text_align

Text
~~~~

.. autodata:: flipperzero.FONT_PRIMARY
.. autodata:: flipperzero.FONT_SECONDARY
.. autofunction:: flipperzero.canvas_set_font
.. autofunction:: flipperzero.canvas_set_text

Shapes
~~~~~~

.. autofunction:: flipperzero.canvas_draw_dot
.. autofunction:: flipperzero.canvas_draw_box
.. autofunction:: flipperzero.canvas_draw_frame
.. autofunction:: flipperzero.canvas_draw_line
.. autofunction:: flipperzero.canvas_draw_circle
.. autofunction:: flipperzero.canvas_draw_disc

Dialog
------

Display message dialogs on the display for user infos and confirm actions.

.. autofunction:: flipperzero.dialog_message_set_header
.. autofunction:: flipperzero.dialog_message_set_text
.. autofunction:: flipperzero.dialog_message_set_button
.. autofunction:: flipperzero.dialog_message_show

.. _reference-gpio:

GPIO
----

Access to the GPIO pins of your Flipper.

Pins
~~~~

.. autodata:: flipperzero.GPIO_PIN_PC0
.. autodata:: flipperzero.GPIO_PIN_PC1
.. autodata:: flipperzero.GPIO_PIN_PC3
.. autodata:: flipperzero.GPIO_PIN_PB2
.. autodata:: flipperzero.GPIO_PIN_PB3
.. autodata:: flipperzero.GPIO_PIN_PA4
.. autodata:: flipperzero.GPIO_PIN_PA6
.. autodata:: flipperzero.GPIO_PIN_PA7

Modes
~~~~~

.. autodata:: flipperzero.GPIO_MODE_INPUT
.. autodata:: flipperzero.GPIO_MODE_OUTPUT_PUSH_PULL
.. autodata:: flipperzero.GPIO_MODE_OUTPUT_OPEN_DRAIN
.. autodata:: flipperzero.GPIO_MODE_ANALOG
.. autodata:: flipperzero.GPIO_MODE_INTERRUPT_RISE
.. autodata:: flipperzero.GPIO_MODE_INTERRUPT_FALL

Pull 
~~~~

.. autodata:: flipperzero.GPIO_PULL_NO
.. autodata:: flipperzero.GPIO_PULL_UP
.. autodata:: flipperzero.GPIO_PULL_DOWN

Speed
~~~~~

.. autodata:: flipperzero.GPIO_SPEED_LOW
.. autodata:: flipperzero.GPIO_SPEED_MEDIUM
.. autodata:: flipperzero.GPIO_SPEED_HIGH
.. autodata:: flipperzero.GPIO_SPEED_VERY_HIGH

Functions
~~~~~~~~~

.. autofunction:: flipperzero.gpio_init_pin
.. autofunction:: flipperzero.gpio_deinit_pin
.. autofunction:: flipperzero.gpio_set_pin
.. autofunction:: flipperzero.gpio_get_pin
.. autodecorator:: flipperzero.on_gpio

.. _reference-adc:

ADC
---

Read analog values from selected GPIO pins:

* :const:`flipperzero.GPIO_PIN_PC0`
* :const:`flipperzero.GPIO_PIN_PC1`
* :const:`flipperzero.GPIO_PIN_PC3`
* :const:`flipperzero.GPIO_PIN_PA4`
* :const:`flipperzero.GPIO_PIN_PA6`
* :const:`flipperzero.GPIO_PIN_PA7`

The corresponding pin must be initialized in the analog mode:

.. code-block::

   import flipperzero as f0

   f0.gpio_init_pin(f0.GPIO_PIN_PC0, f0.GPIO_MODE_ANALOG)

This configures the pin as ADC input with the following settings:

* Reference voltage is set to 2.048 V.
* Clock speed is at 64 MHz in synchronous mode.
* Oversample rate is set to 64.

`This default configuration is best for relatively high impedance circuits with slowly or or not changing signals.`

Functions
~~~~~~~~~

.. autofunction:: flipperzero.adc_read_pin_value
.. autofunction:: flipperzero.adc_read_pin_voltage

.. _reference-pwm:

PWM
---

Output a PWM signal on selected GPIO pins:

* :const:`flipperzero.GPIO_PIN_PA4`
* :const:`flipperzero.GPIO_PIN_PA7`

Functions
~~~~~~~~~

.. autofunction:: flipperzero.pwm_start
.. autofunction:: flipperzero.pwm_stop
.. autofunction:: flipperzero.pwm_is_running

.. _reference-infrared:

Infrared
--------

Send and receive infrared signals.

Signal Format
~~~~~~~~~~~~~

The format to represent infrared signals uses a simple list of integers.
Each value represents the duration between two signal edges in microseconds.
Since this is a digital signal, there are only two levels: `high` and `low`.
The timing list always starts with a `high` level.

.. literalinclude:: ./assets/pwm_signal.txt
   :language: text

.. hint::

   This is equal to the raw signal format of the `IR file <https://developer.flipper.net/flipperzero/doxygen/infrared_file_format.html>`_ specification.

Functions
~~~~~~~~~

.. autofunction:: flipperzero.infrared_receive
.. autofunction:: flipperzero.infrared_transmit
.. autofunction:: flipperzero.infrared_is_busy

UART
----

Connect to UART enabled devices.

Modes
~~~~~

.. autodata:: flipperzero.UART_MODE_LPUART
.. autodata:: flipperzero.UART_MODE_USART

Functions
~~~~~~~~~

.. autofunction:: flipperzero.uart_open

Classes
~~~~~~~

.. autoclass:: flipperzero.UART
   :members: read, readline, readlines, write, flush, close, __enter__, __exit__, __del__

Logging
-------

Log messages to the Flipper's own logging backend.
Check out the `Flipper Zero docs <https://docs.flipper.net/development/cli#_yZ2E>`_ on how to reveal them in the CLI.
Be aware, that you can't change Flipper's global log level from within your script.
Change the `corresponding settings <https://docs.flipper.net/basics/settings#d5TAt>`_ instead or use the **log** command in the CLI with the desired log level as the first argument.

Levels
~~~~~~

.. autodata:: logging.TRACE
.. autodata:: logging.DEBUG
.. autodata:: logging.INFO
.. autodata:: logging.WARN
.. autodata:: logging.ERROR
.. autodata:: logging.NONE
.. autodata:: logging.level

Functions
~~~~~~~~~

.. autofunction:: logging.setLevel
.. autofunction:: logging.getEffectiveLevel
.. autofunction:: logging.trace
.. autofunction:: logging.debug
.. autofunction:: logging.info
.. autofunction:: logging.warn
.. autofunction:: logging.error
.. autofunction:: logging.log

I/O
---

Read and write SD card files.

Constants
~~~~~~~~~

.. autodata:: io.SEEK_SET
.. autodata:: io.SEEK_CUR
.. autodata:: io.SEEK_END

Functions
~~~~~~~~~

.. autofunction:: io.open

Classes
~~~~~~~

.. autoclass:: io.BinaryFileIO
   :members: name, read, readline, readlines, readable, writable, write, flush, seek, tell, close, __enter__, __exit__, __del__

.. autoclass:: io.TextFileIO
   :members: name, read, readline, readlines, readable, writable, write, flush, seek, tell, close, __enter__, __exit__, __del__

Random
------

Generate pseudo-random number for various use cases. 

Functions
~~~~~~~~~

.. autofunction:: random.random
.. autofunction:: random.uniform
.. autofunction:: random.randrange
.. autofunction:: random.getrandbits
.. autofunction:: random.randint
.. autofunction:: random.choice
.. autofunction:: random.seed

Built-In
--------

The functions in this section are `not` part of the ``flipperzero`` module.
They're members of the global namespace instead.

.. py:function:: print(*objects, sep=' ', end='\n') -> None

   The standard Python `print <https://docs.python.org/3/library/functions.html#print>`_ function.
   Where the output of this function will be redirected depends on how the script is invoked:

      * When invoked from the UI, the output will be sent to the Flipper's log buffer.
        Check out the `Flipper Zero docs <https://docs.flipper.net/development/cli#_yZ2E>`_ on how to view them in the CLI interface.
      * In the REPL, the output will be sent to the standard output buffer.
      * When invoked by the **py** command, the output will be sent to the standard output buffer.

   :param objects: The objects to print (mostly a single string).
   :param sep: The separator to use between the objects.
   :param end: The line terminator character to use.

   .. versionadded:: 1.0.0
   .. versionchanged:: 1.5.0

      Output redirection, based on script invocation.
