from typing import Callable

GPIO_PIN_PC0: int
"""
Constant identifier for GPIO pin PC0.

* This pin can be used as ADC input.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PC1: int
"""
Constant identifier for GPIO pin PC1.

* This pin can be used as ADC input.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PC3: int
"""
Constant identifier for GPIO pin PC3.

* This pin can be used as ADC input.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PB2: int
"""
Constant identifier for GPIO pin PB2.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PB3: int
"""
Constant identifier for GPIO pin PB3.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PA4: int
"""
Constant identifier for GPIO pin PA4.

* This pin can be used as ADC input.
* This pin can be used as PWM output.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PA6: int
"""
Constant identifier for GPIO pin PA6.

* This pin can be used as ADC input.
    
.. versionadded:: 1.2.0
"""

GPIO_PIN_PA7: int
"""
Constant identifier for GPIO pin PA7.

* This pin can be used as ADC input.
* This pin can be used as PWM output.
* This pin can be used to transmit an infrared signal with an IR LED.

    
.. versionadded:: 1.2.0
"""

GPIO_MODE_INPUT: int
"""
Constant configuration value for the GPIO input mode.
    
.. versionadded:: 1.2.0
"""

GPIO_MODE_OUTPUT_PUSH_PULL: int
"""
Constant configuration value for the GPIO output as push-pull mode.
    
.. versionadded:: 1.2.0
"""

GPIO_MODE_OUTPUT_OPEN_DRAIN: int
"""
Constant configuration value for the GPIO output as open-drain mode.
    
.. versionadded:: 1.2.0
"""

GPIO_MODE_ANALOG: int
"""
Constant configuration value for the GPIO analog mode.
    
.. versionadded:: 1.2.0
"""

GPIO_MODE_INTERRUPT_RISE: int
"""
Constant configuration value for the GPIO interrupt on rising edges mode.
    
.. versionadded:: 1.2.0
"""

GPIO_MODE_INTERRUPT_FALL: int
"""
Constant configuration value for the GPIO interrupt on falling edges mode.
    
.. versionadded:: 1.2.0
"""

GPIO_PULL_NO: int
"""
Constant configuration value for the GPIO internal pull resistor disabled.
    
.. versionadded:: 1.2.0
"""

GPIO_PULL_UP: int
"""
Constant configuration value for the GPIO internal pull-up resistor enabled.
    
.. versionadded:: 1.2.0
"""

GPIO_PULL_DOWN: int
"""
Constant configuration value for the GPIO internal pull-down resistor enabled.
    
.. versionadded:: 1.2.0
"""

GPIO_SPEED_LOW: int
"""
Constant configuration value for the GPIO in low speed.
    
.. versionadded:: 1.2.0
"""

GPIO_SPEED_MEDIUM: int
"""
Constant configuration value for the GPIO in medium speed.
    
.. versionadded:: 1.2.0
"""

GPIO_SPEED_HIGH: int
"""
Constant configuration value for the GPIO in high speed.
    
.. versionadded:: 1.2.0
"""

GPIO_SPEED_VERY_HIGH: int
"""
Constant configuration value for the GPIO in very high speed.
    
.. versionadded:: 1.2.0
"""


def gpio_init_pin(pin: int, mode: int, pull: int = None, speed: int = None) -> bool:
    """
    Initialize a GPIO pin.

    :param pin: The pin to initialize (e.g. :const:`GPIO_PIN_PA4`).
    :param mode: The mode to use (e.g. :const:`GPIO_MODE_INPUT`).
    :param pull: The pull resistor to use. Default is :const:`GPIO_PULL_NO`.
    :param speed: The speed to use. Default is :const:`GPIO_SPEED_LOW`.
    :returns: :const:`True` on success, :const:`False` otherwise.

    .. versionadded:: 1.2.0
    .. versionchanged:: 1.3.0
       The return value changed from ``None`` to ``bool``.

    .. hint::

        The interrupt modes :const:`GPIO_MODE_INTERRUPT_RISE` and :const:`GPIO_MODE_INTERRUPT_FALL` can be combined using bitwise OR.
        This allows you to handle rising `and` falling edges.
    """
    pass


def gpio_deinit_pin(pin: int) -> None:
    """
    Deinitialize a GPIO pin.

    :param pin: The pin to deinitialize (e.g. :const:`GPIO_PIN_PA4`).

    .. versionadded:: 1.3.0

    .. note::

        It's not strictly necessary to deinitialize your GPIO pins upon script termination, this is already covered by the interpreter.
    """
    pass


def gpio_set_pin(pin: int, state: bool) -> None:
    """
    Set the state of an output pin.

    :param pin: The pin to set (e.g. :const:`GPIO_PIN_PA4`).
    :param state: The state to set.

    .. versionadded:: 1.2.0

    .. hint::

        Don't forget to initialize the pin first.
    """
    pass


def gpio_get_pin(pin: int) -> bool:
    """
    Read the state of an input pin.

    :param pin: The pin to read (e.g. :const:`GPIO_PIN_PA4`).
    :returns: :const:`True` if the pin is high, :const:`False` on a low signal.

    .. versionadded:: 1.2.0

    .. hint::

        Don't forget to initialize the pin first.
    """
    pass


def on_gpio() -> Callable[[int], None]:
    """
    Decorate a function to be used as GPIO interrupt handler. The decorated function will be invoked upon a GPIO interrupt.

    .. versionadded:: 1.0.0

    .. code-block::

        import flipperzero as f0

        f0.gpio_init_pin(f0.GPIO_PIN_PC0, f0.GPIO_MODE_INTERRUPT_RISE, f0.GPIO_PULL_UP)

        @f0.on_gpio
        def interrupt_handler(pin):
            if pin == f0.GPIO_PIN_PC0:
                ...

    .. warning::

        You can only decorate one function per application.
    """
    pass
