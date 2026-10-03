# Morse Flipper

Morse Flipper is a CW trainer, keyer, hardware adapter, portable ham helper, and Sub-GHz Morse transceiver for the Flipper Zero.

Current release: 0.3.0.

It is built around learning Morse by sound rather than by staring at dots and dashes. You can practise copying, send with buttons or external keys, configure keyer timing, and use the Flipper as a small CW hardware adapter.

Morse Flipper also includes a fairly extensive help manual on the Flipper itself, under Help. It covers how to learn and practise Morse Code, how to connect straight keys and paddles, what is worth practising, and which outside resources are worth your time. Please read it; there is more useful Morse guidance in there than fits comfortably in a README. You can find the complete Morse Flipper [user manual on GitHub](https://github.com/yo3gnd/morse-flipper/blob/master/manual/README.md).

**After installing or updating Morse Flipper, allow up to 15 seconds for assets to unpack before pressing buttons or launching another app.**

## Features

- Sends and receives Flipper-to-Flipper Morse over Sub-GHz radio, decodes compatible Morse within the Flipper's supported bands, and transmits 700 Hz CWFM that ordinary FM handhelds can hear.
- Trains receiving with LCWO/Koch progressions and adaptive Instant Character Recognition, recording lesson progress, scores, recent sessions and daily streaks. Callsign practice generates plausible callsigns from 20 entities with independent speed, spacing and length controls.
- Runs Morse Ninja-style Passive Listening with callsigns or lesson groups, playing CW and then speaking the answer in NATO phonetics through the internal speaker or P2/A7. It can instead transmit the complete exercise over FM to a nearby handheld. It works well for listening on a Baofeng while driving, doing chores or walking the dog, or as a club activity.
- Combines a field keyer with an SD-card log for POTA, SOTA and contest-style operating, with stored exchanges, live paddle sending, rig keying on P15 and PTT on P16.
- Turns the joystick or a simple GPIO jack adapter into a straight-key or paddle interface, with Iambic, Elekey-A/B, Ultimatic, bug, keyahead and Vail-compatible keying.
- Provides ARDF Foxhunting in Standard, Sprint and Custom modes using carrier-keyed CW or CWFM, with optional GPIO and audio output.

**And plenty of other features**

- Provides straight-key timing practice, five-character sending drills and free practice with a straight key, paddle, Flipper buttons, USB, MIDI, mouse or keyboard input.
- Corrects listening answers before scoring: ↓ deletes the last character and ↑ clears the entire answer.
- Maps the Flipper buttons to either a straight key or an OK/Back paddle.
- Provides Vail-compatible USB MIDI control for browser practice, including remote speed, tone and keyer mode.
- Offers selectable sinewave or square-wave Morse on the internal speaker, shaped sinewave sidetone on P2/A7 and vibration fallback.
- Includes built-in help, saved settings, custom training character files, decoder views, compact run history and startup checks for suspicious GPIO wiring.

## Hardware

No extra hardware is required. The app works with the Flipper buttons and internal speaker.

For a real key or paddle, use a simple jack adapter wired to the GPIO header. The default wiring is P7 for dit or straight key, P5 for dah, and P3 as the software-controlled ground. A real GND pin can also be used.

For rig control, Ham Keyer mode uses P15 as the key output and P16 as PTT. Verify levels, polarity, and isolation before connecting to radio equipment.

## A note on Morse transmissions

Sub-GHz transmission is jurisdiction-dependent, so check the applicable band plan before transmitting. The Flipper's approximately 16 mW output is suitable for short-range practice and compatible-device communication, not as a replacement for an emergency or amateur radio.

## More

Source and full documentation: https://github.com/yo3gnd/morse-flipper
