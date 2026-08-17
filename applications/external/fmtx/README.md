# FM TX

FMTX transmits audio from MP3 files or USB audio through the Flipper's internal CC1101 or an external transmitter. Tune a handheld radio or walkie-talkie to the same frequency and have a listen.

This started as an experiment for [Morse Flipper](https://github.com/yo3gnd/morse-flipper), a Morse code trainer, but proved far too interesting to leave buried there, so it gets a slightly unreasonable spike of its own.

It decodes MP3 or appears to a host as a 48 kHz mono USB speaker, gives the audio a blunt DSP haircut, then uses first-order PDM to wag the CC1101 between two FSK states. A receiver hears something recognisably FM-ish, which is the useful bit.

It works surprisingly well for such an unreasonable arrangement. It also makes a cheerful mess of Carson's rule, so this remains a proof of concept rather than a polite transmitter.
