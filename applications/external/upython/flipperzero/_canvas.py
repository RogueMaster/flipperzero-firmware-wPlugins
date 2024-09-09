def canvas_update() -> None:
    '''
    Updates the display buffer with your drawings from the canvas.

    .. versionadded:: 1.0.0

    .. note::

        Your drawings will only appear on the display after this function call.
    '''
    pass

def canvas_clear() -> None:
    '''
    Clear the whole canvas. This does not affect the current display buffer.
    You need to call :func:`canvas_update` to reveal your changes.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_width() -> int:
    '''
    Get the canvas width in pixels.

    .. versionadded:: 1.0.0

    :returns: The canvas width.
    '''
    pass

def canvas_height() -> int:
    '''
    Get the canvas height in pixels.

    .. versionadded:: 1.0.0

    :returns: The canvas height.
    '''
    pass

COLOR_BLACK: int
'''
Constant value for the color `black`.

.. versionadded:: 1.0.0
'''

COLOR_WHITE: int
'''
Constant value for the color `white`.

.. versionadded:: 1.0.0
'''

def canvas_set_color(color: int) -> None:
    '''
    Set the color to use when drawing or writing on the canvas.

    .. versionadded:: 1.0.0

    :param color: The color to use.
    '''
    pass

ALIGN_BEGIN: int
'''
Align element at `begin` (horizontal or vertical, depends on the context).

.. versionadded:: 1.0.0
'''

ALIGN_END: int
'''
Align element at `end` (horizontal or vertical, depends on the context).

.. versionadded:: 1.0.0
'''

ALIGN_CENTER: int
'''
Align element at `center` (horizontal or vertical, depends on the context).

.. versionadded:: 1.0.0
'''

def canvas_set_text_align(x: int, y: int) -> None:
    '''
    Define how the text should be aligned in relation to the ``x`` and ``y`` coordinates 
    when writing on the canvas, using the :func:`canvas_set_text` function.

    :param x: The horizontal alignment.
    :param y: The vertical alignment.

    .. versionadded:: 1.0.0
    '''
    pass

FONT_PRIMARY: int
'''
Constant value for the primary font.

.. versionadded:: 1.0.0
'''

FONT_SECONDARY: int
'''
Constant value for the secondary font.

.. versionadded:: 1.0.0
'''

def canvas_set_font(font: int) -> None:
    '''
    Change the font to use when writing on the canvas using the :func:`canvas_set_text` function.

    :param font: The font to use.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_set_text(x: int, y: int, text: str) -> None:
    '''
    Write text on the canvas at the position of ``x`` and ``y`` by using the currently active color, font and alignment settings.
    
    :param x: The horizontal position.
    :param y: The vertical position.
    :param text: The text to write.

    .. versionadded:: 1.0.0
    
    .. code-block::

        import flipperzero as f0

        f0.canvas_set_color(f0.COLOR_BLACK)
        f0.canvas_set_text_align(f0.ALIGN_CENTER, f0.ALIGN_BEGIN)
        f0.canvas_set_text(64, 32, 'Hello World!')
        f0.canvas_update()

    .. seealso::

        * :func:`canvas_set_color` to change the canvas color.
        * :func:`canvas_set_text_align` to change the alignment settings.
        * :func:`canvas_set_font` to change the current font.
    '''
    pass

def canvas_draw_dot(x: int, y: int) -> None:
    '''
    Draw a dot on the canvas by using the currently active color settings.

    :param x: The horizontal position.
    :param y: The vertical position.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_draw_box(x: int, y: int, w: int, h: int, r: int) -> None:
    '''
    Draw a box on the canvas. The fill color is defined by the currently active color settings.
    Set the corner radius to zero to draw a rectangle without rounded corners.

    :param x: The horizontal position.
    :param y: The vertical position.
    :param w: The width of the box.
    :param h: The height of the box.
    :param r: The corner radius to use.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_draw_frame(x: int, y: int, w: int, h: int, r: int) -> None:
    '''
    Draw a frame on the canvas. The border color is defined by the currently active color settings.
    Set the corner radius to zero to draw a rectangle without rounded corners.

    :param x: The horizontal position.
    :param y: The vertical position.
    :param w: The width of the box.
    :param h: The height of the box.
    :param r: The corner radius to use.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_draw_line(x0: int, y0: int, x1: int, y1: int) -> None:
    '''
    Draw a line on the canvas. The color is defined by the currently active color settings.

    :param x0: The horizontal start position.
    :param y0: The vertical start position.
    :param x1: The horizontal end position.
    :param y1: The vertical end sposition.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_draw_circle(x: int, y: int, r: int) -> None:
    '''
    Draw a circle on the canvas. The border color is defined by the currently active color settings.

    :param x: The horizontal position.
    :param y: The vertical position.
    :param r: The radius to use.

    .. versionadded:: 1.0.0
    '''
    pass

def canvas_draw_disc(x: int, y: int, r: int) -> None:
    '''
    Draw a disc on the canvas. The fill color is defined by the currently active color settings.

    :param x: The horizontal position.
    :param y: The vertical position.
    :param r: The radius to use.

    .. versionadded:: 1.0.0
    '''
    pass
