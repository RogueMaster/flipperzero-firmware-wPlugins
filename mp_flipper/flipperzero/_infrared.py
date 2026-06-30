from typing import List


def infrared_receive(timeout: int = 1000000) -> List[int]:
    """
    Receive an infrared signal. This is a blocking method.
    The method blocks until a timeout occurs or the internal
    signal buffer (capacity is 1024 timings) is filled.

    :param timeout: The timeout to use in microseconds.
    :returns: A list of timings in microseconds, starting with high.

    .. versionadded:: 1.3.0
    """
    pass


def infrared_transmit(
    signal: List[int],
    repeat: int = 1,
    use_external_pin: bool = False,
    frequency: int = 38000,
    duty: float = 0.33,
) -> bool:
    """
    Transmit an infrared signal. This is a blocking method.
    The method blocks until the whole signal is sent.
    The signal list has the same format as the return value
    of :func:`infrared_receive`. Hence you can directly re-send
    a received signal without any further processing.

    :param signal: The signal to use.
    :param repeat: How many times the signal should be sent.
    :param use_external_pin: :const:`True` to use an external IR LED on GPIO pin :const:`flipperzero.GPIO_PIN_PA7`.
    :param frequency: The frequency to use for the PWM signal.
    :param duty: The duty cycle to use for the PWM signal.
    :returns: :const:`True` on success, :const:`False` otherwise.

    .. versionadded:: 1.3.0
    """
    pass


def infrared_is_busy() -> bool:
    """
    Check if the infrared subsystem is busy.

    :returns: :const:`True` if occupied, :const:`False` otherwise.

    .. versionadded:: 1.3.0
    """
    pass
