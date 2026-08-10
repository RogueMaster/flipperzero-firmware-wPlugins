# Morse Flipper

Morse Flipper is a CW trainer, keyer, hardware adapter, portable ham helper, and Sub-GHz Morse experiment bench for the Flipper Zero.

It is built around learning Morse by sound rather than by staring at dots and dashes. You can practise copying, send with buttons or external keys, experiment with keyer timing, and use the Flipper as a small CW hardware adapter.

Morse Flipper also includes a fairly extensive help manual on the Flipper itself, under `Help`. It covers how to learn and practise Morse Code, how to connect straight keys and paddles, what is worth practising, and which outside resources are worth your time. Please read it; there is more useful Morse guidance in there than fits comfortably in a README. You can find the complete Morse Flipper [user manual on GitHub](https://github.com/yo3gnd/morse-flipper/blob/master/manual/README.md).

## Features

- Flipper-to-Flipper Sub-GHz Morse TX/RX, plus receive and decode experiments for compatible OOK Morse signals inside supported Flipper bands.
- Listening practice (LCWO/Koch progressions), straight-key timing practice, and five-character sending drills.
- Free practice with a straight key, paddle, Flipper buttons, USB, MIDI, mouse, or keyboard input.
- Iambic, Elekey-A/B, Ultimatic, bug, keyahead, and Vail-compatible keying modes.
- Built-in help for setup, hardware, practice, portable operating, contests, and CW operating notes.
- GPIO key and paddle input using a simple jack adapter, with configurable pins and startup checks for suspicious wiring.
- Ham keyer mode with rig keying on P15 and PTT on P16.
- Smoother internal Soft Buzz sidetone, high-quality sidetone on P2/A7, and vibration fallback.
- Field keyer/logger for portable operating and canned replies.
- Saved settings, compact run history, custom training character files on SD, and a small CW decoder.

## Hardware

No extra hardware is required. The app works with the Flipper buttons and internal speaker.

For a real key or paddle, use a simple jack adapter wired to the GPIO header. The default wiring is P7 for dit or straight key, P5 for dah, and P3 as the software-controlled ground. A real GND pin can also be used.

For rig control, Ham Keyer mode uses P15 as the key output and P16 as PTT. Verify levels, polarity, and isolation before connecting to radio equipment.

## A note on Morse transmissions

Sub-GHz TX remains as jurisdiction-dependent as any ordinary Flipper Zero transmission, which is the polite way of saying: know your band plan before pressing send. It is useful for experiments, practice, and keying a ham transceiver. Do not trust the Flipper's 100 mW for emergency Morse communications. Treat it as an entry-level teaser, or a very bad QRP rig, not a replacement for a proper radio.

## More

Source and full documentation: https://github.com/yo3gnd/morse-flipper
