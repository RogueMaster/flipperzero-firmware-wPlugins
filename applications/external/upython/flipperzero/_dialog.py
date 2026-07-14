def dialog_message_set_header(text: str, x: int, y: int, h: int, v: int) -> None:
    """
    Set a header text on the dialog box.

    :param text: The text to set.
    :param x: The x coordinates to use.
    :param y: The y coordinates to use.
    :param h: The horizontal alignment.
    :param v: The vertical alignment.

    .. versionadded:: 1.0.0
    """
    pass


def dialog_message_set_text(text: str, x: int, y: int, h: int, v: int) -> None:
    """
    Set a text on the dialog box.

    :param text: The text to set.
    :param x: The x coordinates to use.
    :param y: The y coordinates to use.
    :param h: The horizontal alignment.
    :param v: The vertical alignment.

    .. versionadded:: 1.0.0
    """
    pass


def dialog_message_set_button(text: str, button: int) -> None:
    """
    Set the text of a dialog box button.

    :param text: The text to set.
    :param button: The button to use (e.g. :const:`INPUT_BUTTON_UP`).

    .. versionadded:: 1.0.0
    """
    pass


def dialog_message_show() -> int:
    """
    Display the dialog box with the configured settings.
    This function is blocking.

    :returns: The button code, used to close the dialog (e.g. :const:`INPUT_BUTTON_OK`)

    .. versionadded:: 1.0.0

    .. code-block::

        import flipperzero as f0

        f0.dialog_message_set_header('Important',64, 12)
        f0.dialog_message_set_text('It this awesome?', 64, 24)
        f0.dialog_message_set_button('Yes', f0.INPUT_BUTTON_LEFT)
        f0.dialog_message_set_button('No', f0.INPUT_BUTTON_RIGHT)

        while f0.dialog_message_show() is not f0.INPUT_BUTTON_LEFT:
            pass
    """
    pass
