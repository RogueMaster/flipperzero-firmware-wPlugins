# FM TX

Can a Flipper Zero decode an MP3 and transmit it as FM? This little spike is here to find out.

This started as an experiment for [Morse Flipper](https://github.com/yo3gnd/morse-flipper), a Morse code trainer, but proved far too interesting to leave buried there, so it gets a slightly unreasonable spike of its own.

It decodes MP3 audio, gives it a blunt DSP haircut, then uses first-order PDM to wag the CC1101 between two FSK states. A handheld hears something recognisably FM-ish, which is the useful bit.

It works surprisingly well for such an unreasonable arrangement. It also makes a cheerful mess of Carson's rule and occupies rather more spectrum than it ought to. This is a proof of concept, not a polite transmitter.
