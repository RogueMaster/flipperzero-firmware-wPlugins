def speaker_start(frequency: float, volume: float) -> bool:
    '''
    Output a steady tone of a defined frequency and volume on the Flipper's speaker.
    This is a non-blocking operation. The tone will continue until you call :func:`speaker_stop`.
    The ``volume`` parameter accepts values from 0.0 (silent) up to 1.0 (very loud).

    :param frequency: The frequency to play in `hertz <https://en.wikipedia.org/wiki/Hertz>`_.
    :param volume: The volume to use.
    :returns: :const:`True` if the speaker was acquired.

    .. versionadded:: 1.0.0

    .. code-block::
        
        import flipperzero as f0
        
        f0.speaker_start(50.0, 0.8)
    '''
    pass

def speaker_set_volume(volume: float) -> bool:
    '''
    Set the speaker's volume while playing a tone. This is a non-blocking operation.
    The tone will continue until you call :func:`speaker_stop`.
    The ``volume`` parameter accepts values from 0.0 (silent) up to 1.0 (very loud).
    
    :param volume: The volume to use.
    :returns: :const:`True` if the speaker was acquired.

    .. versionadded:: 1.0.0

    This function can be used to play `nice` sounds:

    .. code-block::

        import time
        import flipperzero as f0
        
        volume = 0.8

        f0.speaker_start(100.0, volume)

        for _ in range(0, 150):
            volume *= 0.9945679

            f0.speaker_set_volume(volume)

            time.sleep_ms(1)
        
        f0.speaker_stop()
    '''
    pass

def speaker_stop() -> bool:
    '''
    Stop the speaker output.

    :returns: :const:`True` if the speaker was successfully released.

    .. versionadded:: 1.0.0
    '''
    pass
