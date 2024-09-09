from typing import Callable

INPUT_BUTTON_UP: int
'''
Constant value for the `up` button.

.. versionadded:: 1.0.0
'''

INPUT_BUTTON_DOWN: int
'''
Constant value for the `down` button.

.. versionadded:: 1.0.0
'''

INPUT_BUTTON_RIGHT: int
'''
Constant value for the `right` button.

.. versionadded:: 1.0.0
'''

INPUT_BUTTON_LEFT: int
'''
Constant value for the `left` button.

.. versionadded:: 1.0.0
'''

INPUT_BUTTON_OK: int
'''
Constant value for the `ok` button.

.. versionadded:: 1.0.0
'''

INPUT_BUTTON_BACK: int
'''
Constant value for the `back` button.

.. versionadded:: 1.0.0
'''

INPUT_TYPE_PRESS: int
'''
Constant value for the `press` event of a button.

.. versionadded:: 1.0.0
'''

INPUT_TYPE_RELEASE: int
'''
Constant value for the `release` event of a button.

.. versionadded:: 1.0.0
'''

INPUT_TYPE_SHORT: int
'''
Constant value for the `short` press event of a button.

.. versionadded:: 1.0.0
'''

INPUT_TYPE_LONG: int
'''
Constant value for the `long` press event of a button.

.. versionadded:: 1.0.0
'''

INPUT_TYPE_REPEAT: int
'''
Constant value for the `repeat` press event of a button.

.. versionadded:: 1.0.0
'''

def on_input() -> Callable[[int, int], None]:
    '''
    Decorate a function to be used as input handler. The decorated function will be invoked upon interaction with one of the buttons on the Flipper.

    .. versionadded:: 1.0.0

    .. code-block::

        import flipperzero as f0

        @f0.on_input
        def input_handler(button, type):
            if button == f0.INPUT_BUTTON_BACK:
                if type == f0.INPUT_TYPE_LONG:
                    ...
    
    .. warning::

        You can only decorate one function per application.
    '''
    pass
