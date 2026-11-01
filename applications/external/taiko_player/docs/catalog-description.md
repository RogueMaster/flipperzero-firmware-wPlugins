# Taiko Player

A lightweight TJA chart player for Flipper Zero. Play backing music through the built-in speaker, watch scrolling notes, and optionally send drum-hit pulses to external hardware.

## Features

- Normal and big Don/Ka notes, drumrolls, balloons, and GOGO TIME effects.
- Automatic drum-sound mixing with alternating left/right drum indicators; Don/Ka pitch stays fixed across playback speeds and same-sound hits replace old tails.
- Difficulty selection, pause, restart, song selection, and adjustable time-based or bar-based seeking; bar seeking is the default.
- Scrolling measure lines with support for hidden and empty measures.
- Independent playback speed, scroll speed, and volume controls, including 0-200% app master volume.
- Target display refresh rate from 15 to 60 FPS, with 50 FPS as the default.
- Per-note scroll speed changes, including reverse scrolling.
- Four configurable GPIO outputs, disabled at startup.
- Saved volume, speed, frame-rate, seeking, GPIO pin and pulse-width preferences.

## Before playing

An SD card and your own TJA charts with matching audio are required. Convert audio on your computer with Python 3, FFmpeg, and the provided conversion scripts. The device plays unsigned 8-bit mono PCM WAV and does not decode OGG directly.

Catalog installation provides the app. Songs and optional drum sounds must be copied to the SD card separately. Place prepared songs under /ext/apps_data/taiko_player/songs/ and drum sounds under /ext/apps_data/taiko_player/sfx/. Without drum sounds, backing audio still plays.

Follow the [English setup instructions](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/README.en.md) to prepare songs and drum sounds. No additional hardware is needed for speaker playback.

## Controls

- OK: play or pause; replay after the song finishes.
- Left / Right: seek backward or forward.
- Up / Down: adjust app volume.
- Short Back: open the menu or return.
- Hold Back during playback: exit.

GPIO is optional. It uses 3.3 V logic and 70 ms high pulses by default (25–100 ms in 5 ms steps, set in GPIO Setting → Pulse Width); connect the receiver to Flipper's GND. Choose pins in GPIO Setting before enabling GPIO Output.

## Limitations

This is an automatic chart player, without hit judgment. Branching charts and Dan courses are not supported. Playback-speed changes also change pitch. Precise audio, display, and GPIO synchronization is not guaranteed.

The project is licensed under GPL-3.0-or-later. Default drum sounds are from TJAPlayer3's SimpleStyle skin under MIT; retain their attribution and license when redistributing them.
