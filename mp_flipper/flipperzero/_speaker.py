"""
Python script for notes generation

# coding: utf-8
# Python script for notes generation

from typing import List

note_names: List = ['C', 'CS', 'D', 'DS', 'E', 'F', 'FS', 'G', 'GS', 'A', 'AS', 'B']

for octave in range(9):
    for name in note_names:
        print("SPEAKER_NOTE_%s%s: float" % (name, octave))
        print('\'\'\'')
        print('The musical note %s\\ :sub:`%s` as frequency in `Hz`.\n' % (name if len(name) == 1 else (name[0]+'#'), octave))
        print('.. versionadded:: 1.2.0')
        print('.. versionchanged:: 1.7.0')
        print('   The constant is replaced by a delegator function.')
        print('\'\'\'\n')
"""

SPEAKER_NOTE_C0: float
"""
The musical note C\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS0: float
"""
The musical note C#\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D0: float
"""
The musical note D\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS0: float
"""
The musical note D#\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E0: float
"""
The musical note E\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F0: float
"""
The musical note F\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS0: float
"""
The musical note F#\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G0: float
"""
The musical note G\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS0: float
"""
The musical note G#\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A0: float
"""
The musical note A\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS0: float
"""
The musical note A#\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B0: float
"""
The musical note B\ :sub:`0` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C1: float
'''
The musical note C\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS1: float
'''
The musical note C#\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D1: float
'''
The musical note D\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS1: float
'''
The musical note D#\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E1: float
'''
The musical note E\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F1: float
'''
The musical note F\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS1: float
'''
The musical note F#\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G1: float
'''
The musical note G\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS1: float
'''
The musical note G#\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A1: float
'''
The musical note A\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS1: float
'''
The musical note A#\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B1: float
'''
The musical note B\ :sub:`1` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C2: float
'''
The musical note C\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS2: float
'''
The musical note C#\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D2: float
'''
The musical note D\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS2: float
'''
The musical note D#\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E2: float
'''
The musical note E\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F2: float
'''
The musical note F\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS2: float
'''
The musical note F#\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G2: float
'''
The musical note G\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS2: float
'''
The musical note G#\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A2: float
'''
The musical note A\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS2: float
'''
The musical note A#\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B2: float
'''
The musical note B\ :sub:`2` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C3: float
'''
The musical note C\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS3: float
'''
The musical note C#\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D3: float
'''
The musical note D\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS3: float
'''
The musical note D#\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E3: float
'''
The musical note E\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F3: float
'''
The musical note F\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS3: float
'''
The musical note F#\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G3: float
'''
The musical note G\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS3: float
'''
The musical note G#\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A3: float
'''
The musical note A\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS3: float
'''
The musical note A#\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B3: float
'''
The musical note B\ :sub:`3` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C4: float
'''
The musical note C\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS4: float
'''
The musical note C#\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D4: float
'''
The musical note D\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS4: float
'''
The musical note D#\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E4: float
'''
The musical note E\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F4: float
'''
The musical note F\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS4: float
'''
The musical note F#\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G4: float
'''
The musical note G\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS4: float
'''
The musical note G#\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A4: float
'''
The musical note A\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS4: float
'''
The musical note A#\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B4: float
'''
The musical note B\ :sub:`4` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C5: float
'''
The musical note C\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS5: float
'''
The musical note C#\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D5: float
'''
The musical note D\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS5: float
'''
The musical note D#\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E5: float
'''
The musical note E\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F5: float
'''
The musical note F\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS5: float
'''
The musical note F#\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G5: float
'''
The musical note G\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS5: float
'''
The musical note G#\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A5: float
'''
The musical note A\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS5: float
'''
The musical note A#\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B5: float
'''
The musical note B\ :sub:`5` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C6: float
'''
The musical note C\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS6: float
'''
The musical note C#\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D6: float
'''
The musical note D\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS6: float
'''
The musical note D#\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E6: float
'''
The musical note E\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F6: float
'''
The musical note F\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS6: float
'''
The musical note F#\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G6: float
'''
The musical note G\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS6: float
'''
The musical note G#\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A6: float
'''
The musical note A\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS6: float
'''
The musical note A#\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B6: float
'''
The musical note B\ :sub:`6` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C7: float
'''
The musical note C\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS7: float
'''
The musical note C#\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D7: float
'''
The musical note D\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS7: float
'''
The musical note D#\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E7: float
'''
The musical note E\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F7: float
'''
The musical note F\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS7: float
'''
The musical note F#\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G7: float
'''
The musical note G\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS7: float
'''
The musical note G#\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A7: float
'''
The musical note A\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS7: float
'''
The musical note A#\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B7: float
'''
The musical note B\ :sub:`7` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_C8: float
'''
The musical note C\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_CS8: float
'''
The musical note C#\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_D8: float
'''
The musical note D\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_DS8: float
'''
The musical note D#\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_E8: float
'''
The musical note E\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_F8: float
'''
The musical note F\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_FS8: float
'''
The musical note F#\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_G8: float
'''
The musical note G\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_GS8: float
'''
The musical note G#\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_A8: float
'''
The musical note A\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_AS8: float
'''
The musical note A#\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_NOTE_B8: float
'''
The musical note B\ :sub:`8` as frequency in `Hz`.

.. versionadded:: 1.2.0
.. versionchanged:: 1.7.0
   The constant is replaced by a delegator function.
'''

SPEAKER_VOLUME_MIN: float
"""
The minimal volume value.

.. versionadded:: 1.2.0
"""

SPEAKER_VOLUME_MAX: float
"""
The maximum volume value.

.. versionadded:: 1.2.0
"""


def speaker_start(frequency: float, volume: float) -> bool:
    """
    Output a steady tone of a defined frequency and volume on the Flipper's speaker.
    This is a non-blocking operation. The tone will continue until you call :func:`speaker_stop`.
    The ``volume`` parameter accepts values from :py:const:`SPEAKER_VOLUME_MIN` (silent) up to :py:const:`SPEAKER_VOLUME_MAX` (very loud).

    :param frequency: The frequency to play in `Hz <https://en.wikipedia.org/wiki/Hertz>`_.
    :param volume: The volume to use.
    :returns: :const:`True` if the speaker was acquired.

    .. versionadded:: 1.0.0

    .. code-block::

        import flipperzero as f0

        f0.speaker_start(50.0, 0.8)
    """
    pass


def speaker_set_volume(volume: float) -> bool:
    """
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
    """
    pass


def speaker_stop() -> bool:
    """
    Stop the speaker output.

    :returns: :const:`True` if the speaker was successfully released.

    .. versionadded:: 1.0.0
    """
    pass
