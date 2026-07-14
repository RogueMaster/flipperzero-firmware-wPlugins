def pwm_start(pin: int, frequency: int, duty: int) -> bool:
    """
    Start or change the PWM signal on the corresponding GPIO pin.

    :param pin: The pin to read (e.g. :const:`GPIO_PIN_PA7`).
    :param frequency: The frequency to set in Hz.
    :param duty: The duty cycle per period in percent.
    :returns: :const:`True` on success, :const:`False` otherwise.

    .. versionadded:: 1.3.0

    .. warning::

        You don't have to initialize the pin first.
    """
    pass


def pwm_stop(pin: int) -> None:
    """
    Stop the PWM signal on the corresponding GPIO pin.

    :param pin: The pin to use (e.g. :const:`GPIO_PIN_PA7`).

    .. versionadded:: 1.3.0
    """
    pass


def pwm_is_running(pin: int) -> bool:
    """
    Check if the corresponding GPIO pin has a PWM signal output.

    :param pin: The pin to check (e.g. :const:`GPIO_PIN_PA7`).
    :returns: :const:`True` on success, :const:`False` otherwise.

    .. versionadded:: 1.3.0
    """
    pass
