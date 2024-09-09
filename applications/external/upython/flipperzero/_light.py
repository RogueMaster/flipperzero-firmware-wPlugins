from typing import Callable

LIGHT_RED: int
'''
Constant value for the red LED light.

.. versionadded:: 1.0.0
'''

LIGHT_GREEN: int
'''
Constant value for the green LED light.

.. versionadded:: 1.0.0
'''

LIGHT_BLUE: int
'''
Constant value for the blue LED light.

.. versionadded:: 1.0.0
'''

LIGHT_BACKLIGHT: int
'''
Constant value for the display backlight.

.. versionadded:: 1.0.0
'''

def light_set(light: int, brightness: int) -> None:
    '''
    Control the RGB LED on your Flipper. You can also set the brightness of multiple channels at once using bitwise operations.
    The ``brightness`` parameter accepts values from 0 (light off) to 255 (very bright).

    :param light: The RGB channels to set.
    :param brightness: The brightness to use.

    .. versionadded:: 1.0.0

    .. code-block::
    
        import flipperzero as f0
        
        f0.light_set(f0.LIGHT_RED | f0.LIGHT_GREEN, 250)

    .. tip::

        You can use  up to seven colors using `additive mixing <https://en.wikipedia.org/wiki/Additive_color>`_.
    '''
    pass

def light_blink_start(light: int, brightness: int, on_time: int, period: int) -> None:
    '''
    Let the RGB LED blink. You can define the total duration of a blink period and the duration, the LED is active during a blink period.
    Hence, ``on_time`` must be smaller than ``period``. This is a non-blocking operation. The LED will continue to blink until you call :func:`light_blink_stop`.

    :param light: The RGB channels to set.
    :param brightness: The brightness to use.
    :param on_time: The LED's active duration in milliseconds.
    :param period: Total duration of a blink period in milliseconds.

    .. versionadded:: 1.0.0

    .. code-block::
    
        import flipperzero as f0
        
        f0.light_blink_start(f0.LIGHT_RED, 150, 100, 200)
    '''
    pass

def light_blink_set_color(light: int) -> None:
    '''
    Change the RGB LED's color while blinking. This is a non-blocking operation.
    Be aware, that you must start the blinking procedure first by using the :func:`light_blink_start` function.
    Call the :func:`light_blink_stop` function to stop the blinking LED.

    :param light: The RGB channels to set.

    .. versionadded:: 1.0.0
    '''
    pass

def light_blink_stop() -> None:
    '''
    Stop the blinking LED.

    .. versionadded:: 1.0.0
    '''
    pass
