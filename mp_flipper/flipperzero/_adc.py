def adc_read_pin_value(pin: int) -> int:
    """
    Read the raw value from the ADC channel.

    :param pin: The pin to read (e.g. :const:`GPIO_PIN_PC1`).
    :returns: The raw value between 0 and 4095.

    .. versionadded:: 1.3.0

    .. hint::

        Don't forget to initialize the pin first.
    """
    pass


def adc_read_pin_voltage(pin: int) -> float:
    """
    Read the voltage from the ADC channel.

    :param pin: The pin to read (e.g. :const:`GPIO_PIN_PC1`).
    :returns: The voltage between 0 - 2.048 V with a precision of ~0.1%.

    .. versionadded:: 1.3.0

    .. hint::

        Don't forget to initialize the pin first.
    """
    pass
