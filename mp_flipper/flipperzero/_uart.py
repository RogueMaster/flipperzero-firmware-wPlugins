from typing import List

UART_MODE_LPUART: int
"""
Constant value for the low power UART mode.

.. versionadded:: 1.5.0
"""

UART_MODE_USART: int
"""
Constant value for the USART mode.

.. versionadded:: 1.5.0
"""


class UART:
    """
    This represents an UART connection.
    The class has no :const:`__init__` method, use :func:`uart_open` to start an UART connection and receive an instance.

    .. versionadded:: 1.5.0

    An :class:`UART` instance is iterable:

    .. code-block::

        import flipperzero as f0

        with f0.open(f0.UART_MODE_USART, 115200) as uart:
            lines = [line for line in uart]

    An :class:`UART` instance can be used with a `context manager <https://docs.python.org/3/reference/datamodel.html#with-statement-context-managers>`_:

    .. code-block::

        import flipperzero as f0

        with f0.open(f0.UART_MODE_USART, 115200) as uart:
            ...

    .. hint::

        The read and write methods are non-blocking in terms of data availability.
        They don't block code execution upon data is available.
        Just an empty result will be returned.
    """

    def read(self, size: int = -1) -> bytes:
        """
        Read from the connection.
        The method will read up to ``size`` bytes and return them.
        If ``size`` is not specified, all available data will be returned.
        The method will return zero bytes, if no data is available.

        :param size: The maximum number of bytes to read.
        :returns: Up to ``size`` bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def readline(self, size: int = -1) -> bytes:
        """
        Read and return one line from the connection.
        If ``size`` is specified, at most ``size`` bytes will be read.
        The line terminator is always ``b'\\n'``.

        :param size: The maximum number of bytes to read.
        :returns: Up to ``size`` bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def readlines(self) -> List[bytes]:
        """
        Read and return a list of lines from the connection.
        The line terminator is always ``b'\\n'``.

        :returns: A list of bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def write(self, data: bytes) -> int:
        """
        Write the given bytes to the connection stream.
        The number of written bytes will be returned.
        This can be less than the length of the provided data.
        Be aware, that the data is not sent synchronously.
        Call :meth:`flush` if you have to wait for the data to be sent.

        :param data: The data to transmit.
        :returns: The number of bytes sent.

        .. versionadded:: 1.5.0
        """
        pass

    def flush(self) -> None:
        """
        Flush the transmission buffer to the underlying UART connection.
        This method blocks until all data is sent.

        .. versionadded:: 1.5.0
        """
        pass

    def close(self) -> None:
        """
        Close the UART connection.

        .. versionadded:: 1.5.0
        """
        pass

    def __enter__(self) -> "UART":
        """
        This method is invoked, when the instance enters a runtime context.

        :returns: The :class:`UART` connection.

        .. versionadded:: 1.5.0
        """
        pass

    def __exit__(self, *args, **kwargs) -> None:
        """
        This method is invoked, when the instance leavs a runtime context.
        This basically calls :meth:`close` on the instance.

        .. versionadded:: 1.5.0
        """
        pass

    def __del__(self) -> None:
        """
        This method is invoked, when the garbage collector removes the object.
        This basically calls :meth:`close` on the instance.

        .. versionadded:: 1.5.0
        """
        pass


def uart_open(mode: int, baud_rate: int) -> UART:
    """
    Open a connection to an UART enabled device by using the specified mode and baud rate.

    :param mode: The mode to use, either :const:`UART_MODE_LPUART` or :const:`UART_MODE_USART`.
    :param baud_rate: The baud rate to use.
    :returns: A :class:`UART` object on success, :const:`None` otherwise.

    .. versionadded:: 1.5.0

    .. code-block::

        import flipperzero as f0

        with f0.uart_open(f0.UART_MODE_USART, 115200) as uart:
            ...
    """
    pass
