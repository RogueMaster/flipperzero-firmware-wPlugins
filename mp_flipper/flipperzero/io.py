import typing
import io

_open = io.open

SEEK_SET: int = 0
"""
Set the pointer position relative to the beginning of the stream.

.. versionadded:: 1.5.0
"""

SEEK_CUR: int = 1
"""
Set the pointer position relative to the current position.

.. versionadded:: 1.5.0
"""

SEEK_END: int = 2
"""
Set the pointer position relative to the end of the stream.

.. versionadded:: 1.5.0
"""


class BinaryFileIO:
    """
    Represents a file, opened in binary mode.

    .. versionadded:: 1.5.0
    """

    name: str
    """
    The name of the file.

    .. versionadded:: 1.5.0
    """

    readable: bool
    """
    Read-only attribute, indicating if the file is readable.
    
    .. versionadded:: 1.5.0
    """

    writable: bool
    """
    Read-only attribute, indicating if the file is writable.
    
    .. versionadded:: 1.5.0
    """

    def read(self, size: int = -1) -> bytes:
        """
        Read from the file.
        The method will read up to ``size`` bytes and return them.
        If ``size`` is not specified, all content up to EOF will be returned.
        If the internal pointer is already at EOF, an empty byte string ``b''`` will be returned.

        :param size: The maximum number of bytes to read.
        :returns: Up to ``size`` bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def readline(self, size: int = -1) -> bytes:
        """
        Read and return one line from the file.
        If ``size`` is specified, at most ``size`` bytes will be read.
        The line terminator is defined as ``b'\\n'``.
        The new line character is included in the return value.
        If the internal pointer is at EOF, an empty byte string ``b''`` will be returned.

        :param size: The maximum number of bytes to read.
        :returns: Up to ``size`` bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def readlines(self) -> typing.List[bytes]:
        """
        Read and return a list of lines from the file.
        The line terminator is defined as ``b'\\n'``.
        The new line character is included in the return value.
        If the internal pointer is at EOF, an empty list will be returned.

        :returns: A list of bytes.

        .. versionadded:: 1.5.0
        """
        pass

    def write(self, data: bytes) -> int:
        """
        Write the given bytes to the file.
        The number of written bytes will be returned.
        This can be less than the length of the provided data.

        :param data: The data to write.
        :returns: The number of bytes written.

        .. versionadded:: 1.5.0
        """
        pass

    def flush(self) -> None:
        """
        Write the contents of the file buffer to the file on the SD card.

        .. versionadded:: 1.5.0
        """
        pass

    def seek(self, offset: int, whence: int = SEEK_SET) -> int:
        """
        Set the pointer position by the given ``offset``, relative to the position indicated by ``whence``.
        The new absolute position will be returned.

        :param offset: The offset to use.
        :param whence: How to interpret the offset (e.g. :const:`SEEK_SET`).
        :returns: The new absolute position.

        .. versionadded:: 1.5.0
        """
        pass

    def tell(self) -> int:
        """
        Get the current pointer position.

        :returns: The absolute position of the pointer.

        .. versionadded:: 1.5.0
        """
        pass

    def close(self) -> None:
        """
        Close the file handle.

        .. versionadded:: 1.5.0
        """
        pass

    def __enter__(self) -> "BinaryFileIO":
        """
        This method is invoked, when the instance enters a runtime context.

        :returns: The :class:`BinaryFileIO` instance.

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


class TextFileIO:
    """
    Represents a file, opened in text mode.

    .. versionadded:: 1.5.0
    """

    name: str
    """
    The name of the file.

    .. versionadded:: 1.5.0
    """

    readable: bool
    """
    Read-only attribute, indicating if the file is readable.
    
    .. versionadded:: 1.5.0
    """

    writable: bool
    """
    Read-only attribute, indicating if the file is writable.
    
    .. versionadded:: 1.5.0
    """

    def read(self, size: int = -1) -> str:
        """
        Read from the file.
        The method will read up to ``size`` characters and return them.
        If ``size`` is not specified, all content up to EOF will be returned.
        If the internal pointer is already at EOF, an empty string will be returned.

        :param size: The maximum number of characters to read.
        :returns: Up to ``size`` characters.

        .. versionadded:: 1.5.0
        """
        pass

    def readline(self, size: int = -1) -> str:
        """
        Read and return one line from the file.
        If ``size`` is specified, at most ``size`` characters will be read.
        The line terminator is defined as ``'\\n'``.
        The new line character is included in the return value.
        If the internal pointer is at EOF, an empty string will be returned.

        :param size: The maximum number of characters to read.
        :returns: Up to ``size`` characters.

        .. versionadded:: 1.5.0
        """
        pass

    def readlines(self) -> typing.List[str]:
        """
        Read and return a list of lines from the file.
        The line terminator is defined as ``'\\n'``.
        The new line character is included in the return value.
        If the internal pointer is at EOF, an empty list will be returned.

        :returns: A list of strings.

        .. versionadded:: 1.5.0
        """
        pass

    def write(self, data: str) -> int:
        """
        Write the given string to the file.
        The number of written characters will be returned.
        This can be less than the length of the provided data.

        :param data: The data to write.
        :returns: The number of characters written.

        .. versionadded:: 1.5.0
        """
        pass

    def flush(self) -> None:
        """
        Write the contents of the file buffer to the file on the SD card.

        .. versionadded:: 1.5.0
        """
        pass

    def seek(self, offset: int, whence: int = SEEK_SET) -> int:
        """
        Set the pointer position by the given ``offset``, relative to the position indicated by ``whence``.
        The new absolute position will be returned.

        :param offset: The offset to use.
        :param whence: How to interpret the offset (e.g. :const:`SEEK_SET`).
        :returns: The new absolute position.

        .. versionadded:: 1.5.0
        """
        pass

    def tell(self) -> int:
        """
        Get the current pointer position.

        :returns: The absolute position of the pointer.

        .. versionadded:: 1.5.0
        """
        pass

    def close(self) -> None:
        """
        Close the file handle.

        .. versionadded:: 1.5.0
        """
        pass

    def __enter__(self) -> "TextFileIO":
        """
        This method is invoked, when the instance enters a runtime context.

        :returns: The :class:`BinaryFileIO` instance.

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


def open(path: str, mode: str, *args, **kwargs) -> BinaryFileIO | TextFileIO:
    """
    Open a file on the file system with the specified mode.
    The file path must always be absolute, beginning with ``/ext``.
    The following modifiers are available:

    .. list-table::
        :header-rows: 1
        :width: 90%

        * - Character
          - Description
        * - ``'r'``
          - Open for reading.
            This is the default.
            Will fail, if the file not exists.
        * - ``'w'``
          - Open for writing, truncating an existing file first.
        * - ``'b'``
          - Open the file in binary mode.
            The return value will be a :class:`BinaryFileIO` instance.
        * - ``'t'``
          - Open the in text mode.
            This is the default.
            The return value will be a :class:`TextFileIO` instance.
        * - ``'+'``
          - Open for reading and writing.
            Will create the file, if it not exists.
            The pointer will be placed at the end of the file.

    The modifiers can be combined, e.g. ``'rb+'`` would open a file for reading and writing in binary mode.

    :param path: The path to the file to open.
    :param mode: How the file should be opened.
    :param args: Is ignored at the moment.
    :param kwargs: Is ignored at the moment.

    .. versionadded:: 1.5.0
    """
    return io._open(path, mode, *args, **kwargs)
