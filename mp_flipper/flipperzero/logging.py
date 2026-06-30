import typing

TRACE: int = 6
"""
Constant value for the `trace` log level.

.. versionadded:: 1.5.0
"""

DEBUG: int = 5
"""
Constant value for the `debug` log level.

.. versionadded:: 1.5.0
"""

INFO: int = 4
"""
Constant value for the `info` log level.

.. versionadded:: 1.5.0
"""

WARN: int = 3
"""
Constant value for the `warn` log level.

.. versionadded:: 1.5.0
"""

ERROR: int = 2
"""
Constant value for the `error` log level.

.. versionadded:: 1.5.0
"""

NONE: int = 1
"""
Constant value for logging disabled.

.. versionadded:: 1.5.0
"""

level: int
"""
The threshold log level, as set by the :func:`setLevel` function.
The initial value is set to the :const:`INFO` level.

.. versionadded:: 1.5.0

.. hint::

    Don't change the value of this variable, use :func:`setLevel` instead.
"""


def setLevel(level: int) -> None:
    """
    Set the current log level of the application.

    :param level: The log level to set (e.g. :const:`INFO`).

    .. versionadded:: 1.5.0

    .. hint::

        This doesn't change the Flipper's effective log level settings.
        Check out the Flipper's `documentation <https://docs.flipper.net/basics/settings#d5TAt>`_ for details on this topic.
    """
    pass


def getEffectiveLevel() -> int:
    """
    Get the effective log level from the Flipper's settings.

    :returns: The effective log level.

    .. versionadded:: 1.5.0
    """
    pass


def trace(message: str, *args) -> None:
    """
    Log a message with level :const:`TRACE`.
    The ``message`` argument can be a format string with ``%`` placeholders.
    No % formatting operation is performed when ``args`` is empty.

    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0

    .. code-block::

        import logging

        value = 42

        logging.trace('value is %d', value)
    """
    pass


def debug(message: str, *args) -> None:
    """
    Log a message with level :const:`DEBUG`.
    See :func:`trace` for details on the usage.

    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0
    """
    pass


def info(message: str, *args) -> None:
    """
    Log a message with level :const:`INFO`.
    See :func:`trace` for details on the usage.

    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0
    """
    pass


def warn(message: str, *args) -> None:
    """
    Log a message with level :const:`WARN`.
    See :func:`trace` for details on the usage.

    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0
    """
    pass


def error(message: str, *args) -> None:
    """
    Log a message with level :const:`ERROR`.
    See :func:`trace` for details on the usage.

    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0
    """
    pass


def log(level: int, message: str, *args) -> None:
    """
    Log a message with the given log level.
    See :func:`trace` for details on the usage.

    :param level: The log level to use (e.g. :const:`INFO`).
    :param message: The message to log.
    :param args: Values for the % formatting.

    .. versionadded:: 1.5.0
    """
    pass
