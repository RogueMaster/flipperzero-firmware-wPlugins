Reference
=========

This page contains the API documentation of the ``flipperzero`` module and some built-in functions.

Vibration
---------

Control the vibration motor of your Flipper.

.. autofunction:: flipperzero.vibro_set

Light
-----

Control the RGB LED and display backlight of your Flipper.

.. autodata:: flipperzero.LIGHT_RED
.. autodata:: flipperzero.LIGHT_GREEN
.. autodata:: flipperzero.LIGHT_BLUE
.. autodata:: flipperzero.LIGHT_BACKLIGHT
.. autofunction:: flipperzero.light_set
.. autofunction:: flipperzero.light_blink_start
.. autofunction:: flipperzero.light_blink_set_color
.. autofunction:: flipperzero.light_blink_stop

Speaker
-------

Full control over the built-in speaker module.

.. autofunction:: flipperzero.speaker_start
.. autofunction:: flipperzero.speaker_set_volume
.. autofunction:: flipperzero.speaker_stop

Input
-----

Make your application interactive with full control over the Flipper's hardware buttons.

.. autodata:: flipperzero.INPUT_BUTTON_UP
.. autodata:: flipperzero.INPUT_BUTTON_DOWN
.. autodata:: flipperzero.INPUT_BUTTON_RIGHT
.. autodata:: flipperzero.INPUT_BUTTON_LEFT
.. autodata:: flipperzero.INPUT_BUTTON_OK
.. autodata:: flipperzero.INPUT_BUTTON_BACK
.. autodata:: flipperzero.INPUT_TYPE_PRESS
.. autodata:: flipperzero.INPUT_TYPE_RELEASE
.. autodata:: flipperzero.INPUT_TYPE_SHORT
.. autodata:: flipperzero.INPUT_TYPE_LONG
.. autodata:: flipperzero.INPUT_TYPE_REPEAT
.. autodecorator:: flipperzero.on_input

Canvas
------

Write text and draw dots and shapes on the the display.

.. autofunction:: flipperzero.canvas_update
.. autofunction:: flipperzero.canvas_clear
.. autofunction:: flipperzero.canvas_width
.. autofunction:: flipperzero.canvas_height
.. autodata:: flipperzero.COLOR_BLACK
.. autodata:: flipperzero.COLOR_WHITE
.. autofunction:: flipperzero.canvas_set_color
.. autodata:: flipperzero.ALIGN_BEGIN
.. autodata:: flipperzero.ALIGN_END
.. autodata:: flipperzero.ALIGN_CENTER
.. autofunction:: flipperzero.canvas_set_text_align
.. autodata:: flipperzero.FONT_PRIMARY
.. autodata:: flipperzero.FONT_SECONDARY
.. autofunction:: flipperzero.canvas_set_font
.. autofunction:: flipperzero.canvas_set_text
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

Built-In
--------

The functions in this section are `not` part of the ``flipperzero`` module.
They're members of the global namespace instead.

.. py:function:: print(*objects, sep=' ', end='\n', file=None, flush=False) -> None

   The standard Python `print <https://docs.python.org/3/library/functions.html#print>`_ function.

   :param objects: The objects to print (mostly a single string).
   :param sep: The separator to use between the objects.
   :param end: The line terminator character to use.

   .. versionadded:: 1.0.0

   .. attention::
      
      This function prints to the internal log buffer.
      Check out the `Flipper Zero docs <https://docs.flipper.net/development/cli#_yZ2E>`_ on how to reveal them in the CLI interface.

