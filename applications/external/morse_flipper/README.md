# Morse Flipper

Morse Flipper is a CW trainer, keyer, hardware adapter, portable ham helper, and Sub-GHz Morse transceiver for the Flipper Zero. [Download version 0.3.0 here](https://github.com/yo3gnd/morse-flipper/releases/download/v0.3.0/morse_flipper.fap).

It is built around one opinion: do not learn Morse by staring at dots and dashes. Learn the sound. Hear the character, type the character, and keep the counting part of your brain out of it.

It works with nothing more than the Flipper buttons and internal speaker. Add a simple jack adapter and it becomes a real key/paddle interface. Add the bigger board and it can key a rig, drive PTT, output cleaner sidetone, and log a portable contact without needing a laptop balanced on a damp picnic table.

It began as a 2024 experiment and has since grown into a tested set of listening, sending, keying and radio tools. Timing, GPIO behaviour, audio routing and memory use are covered by host tests and repeated physical-device acceptance runs.

Morse Flipper also includes a fairly extensive help manual on the Flipper itself, under `Help`. It covers how to learn and practise Morse Code, how to connect straight keys and paddles, what is worth practising, and which outside resources are worth your time. Please read it; there is more useful Morse guidance in there than fits comfortably in a README.

The full Morse Flipper manual lives here: [manual/README.md](manual/README.md).

**After installing or updating Morse Flipper, allow up to 15 seconds for assets to unpack before pressing buttons or launching another app.**

## What it does

- Sends and receives Flipper-to-Flipper Morse over Sub-GHz radio, decodes compatible Morse within the Flipper's supported bands, and transmits 700 Hz CWFM that ordinary FM handhelds can hear.
- Trains receiving with LCWO/Koch progressions and adaptive [Instant Character Recognition](manual/225-instant-character-recognition.md), recording lesson progress, scores, recent sessions and daily streaks. Callsign practice generates plausible callsigns from 20 entities with independent speed, spacing and length controls.
- Runs Morse Ninja-style Passive Listening with callsigns or lesson groups, playing CW and then speaking the answer in NATO phonetics through the internal speaker or `P2/A7`. It can instead transmit the complete exercise over FM to a nearby handheld. It works well for listening on a Baofeng while driving, doing chores or walking the dog, or as a club activity.
- Combines a field keyer with an SD-card log for POTA, SOTA and contest-style operating, with stored exchanges, live paddle sending, rig keying on `P15` and PTT on `P16`.
- Turns the joystick or a simple GPIO jack adapter into a straight-key or paddle interface, with Iambic, Elekey-A/B, Ultimatic, bug, keyahead and Vail-compatible keying.
- Provides [ARDF Foxhunting](manual/302-ardf-foxhunting.md) in Standard, Sprint and Custom modes using carrier-keyed CW or CWFM, with optional GPIO and audio output.

### And plenty of other features

- Provides straight-key timing practice, five-character sending drills and free practice with a straight key, paddle, Flipper buttons, USB, MIDI, mouse or keyboard input.
- Corrects listening answers before scoring: `Down` deletes the last character and `Up` clears the entire answer.
- Maps the Flipper buttons to either a straight key or an `OK`/`Back` paddle.
- Provides Vail-compatible USB MIDI control for browser practice, including remote speed, tone and keyer mode.
- Offers selectable sinewave or square-wave Morse on the internal speaker, shaped sinewave sidetone on `P2/A7` and vibration fallback.
- Includes built-in help, saved settings, custom training character files, decoder views, compact run history and startup checks for suspicious GPIO wiring.

It also falls back sensibly when a straight key is plugged into a stereo paddle jack, because that mistake is not hypothetical. Ask me how I know.

## Why another CW app?

<p align="center">
  <img src="docs/images/ss1.png" alt="Morse Flipper screenshot 1" width="19%">
  <img src="docs/images/ss2.png" alt="Morse Flipper screenshot 2" width="19%">
  <img src="docs/images/ss4.gif" alt="Animated Morse Flipper final score" width="19%">
  <img src="docs/images/ss5.png" alt="Morse Flipper screenshot 5" width="19%">
  <img src="docs/images/ss6.png" alt="Morse Flipper screenshot 6" width="19%"><br>
  <img src="docs/images/ss7.png" alt="Morse Flipper screenshot 7" width="19%">
  <img src="docs/images/ss8.png" alt="Morse Flipper screenshot 8" width="19%">
  <img src="docs/images/ss9.png" alt="Morse Flipper screenshot 9" width="19%">
  <img src="docs/images/ss10.png" alt="Morse Flipper screenshot 10" width="19%">
  <img src="docs/images/ss3.png" alt="Morse Flipper screenshot 3" width="19%">
</p>

There are already Flipper Morse apps, but many of them teach the most common bad habit first: looking at dots and dashes. That is fine for a code table and rubbish for copying real CW at speed.

Morse Flipper trains the useful reflex instead. Keep the character speed high, widen the gaps if needed, and let the sound become the letter. Its Listening trainer follows the same approach and is useful whenever the laptop is not.

The other half of the project is hardware. The Flipper already provides GPIO, USB and Sub-GHz radio; Morse Flipper turns them into a CW adapter, paddle interface, portable keyer and a way to reuse old keys or spare hardware without building a dedicated box first.

## Hardware

### None required

Morse Flipper works out of the box with no extra hardware. Use the joystick as a straight key, switch to the built-in keyers when you want paddle-style timing, choose sinewave or square-wave Morse on the internal speaker, and use the Flipper radio for short-range Morse transmit and receive where that is legal and sensible. The adapters below make it nicer, sturdier, or more useful with real keys and rigs; they are comfort upgrades, not a hard requirement.

### Simple key and paddle jack

<p align="center"><img src="docs/images/howto-basic.webp" alt="Simple 6.5 mm jack adapter wiring" height="540"></p>

The simplest adapter is just a 6.5 mm female jack soldered to header pins. It plugs straight into the Flipper GPIO row and gives you a real key without a PCB. The header spacing is close enough to make the ugly version almost disappointingly easy.

Default wiring:

| Jack / key contact | Flipper GPIO |
| ------------------ | ------------ |
| Dit / straight key | `P7`         |
| Dah                | `P5`         |
| Ground             | `P3`         |

Why is ground `P3` and not `GND`/`P8`? Because the GPIO is assignable. Use whatever pins make sense for your key, and one pin can even pretend to be ground when that makes wiring simpler. `P3` is the default because a 6.5 mm audio jack lands neatly on every second header pin. Since that ground is under software control, the app can do useful little tricks, such as spotting `P5` and `P3` shorted together when a straight key has been shoved into a paddle jack, then falling back accordingly. If you prefer a real ground, use `GND`; the app will cope.

A small GPIO board / Flipper add-on board is in production (drop me a message if you want an early prototype!) but this ugly little jack adapter is the easy first build. It is cheap, obvious, and hard to debug incorrectly. You likely have the parts around your shack anyway.

### Expanded keyer board

<p align="center"><img src="docs/images/morse-flipper-schematic.webp" alt="Expanded GPIO keyer schematic" height="360"> <img src="docs/images/howto-full.webp" alt="Expanded GPIO audio, PTT and keying board" height="360"></p>

The second board simply brings more Flipper GPIO out to sensible connectors: key input, sidetone audio, rig keying and PTT.

For audio, `P2/A7` carries the high quality sidetone. It is generated on a 256kHz square carrier, with the tone modulated as PWM, and it approximates a variable sinewave with fade-in and fade-out rather well. Tie the left and right contacts of a 6.5 mm audio jack together and feed them from `P2/A7`; use ground for the sleeve. Add a small `1-50µF` capacitor from the signal to ground if the carrier whine needs taming. You can probably skip the filter entirely, since most speakers will do enough of it for you. If you hear a constant high-pitched whine, your speaker does not filter it. Headphones are especially guilty of this.

For rig control, the keying outputs are simple low-side switches. A `BC817` NPN transistor shorts the rig signal to ground when the Flipper asserts the output: emitter to ground, collector to the rig key/PTT line, base driven from the Flipper GPIO through a resistor. Ham Keyer mode uses `P15` for key and `P16` for PTT. Check your rig first; if you do not know what the key/PTT line expects, add proper isolation rather than letting optimism be the smoke test. You may also want to avoid joining the rig ground and Flipper ground, or optocouple them. I did not isolate it on my FT-891, because I occasionally make choices future Richard has to inspect carefully.

## Learning CW

Small daily practice beats the grand weekly binge. Morse likes repetition, not theatre.

Start by copying by ear, not by diagram. If you catch yourself thinking `dash-dot-dash` and then deciding it is `K`, slow down the *gaps*, not the character. Farnsworth spacing exists for this reason. The letter should arrive as one sound.

Use the Flipper for quick sessions, pocket practice, button/key experiments and portable operating. Use Vail or V-Band when you want live humans to make things less tidy. Use a real key as soon as you can; the Flipper buttons work, but they are still rubber buttons on a toy dolphin.

## Code and engineering notes

This is not just a beep demo with a menu stapled on. The app has host-tested C cores for keying, CW token handling, training sessions, straight-key scoring, TX-group timing, RF timing helpers, GPIO rules, run-history layout, and the markdown-ish help renderer.

The firmware side uses stock Flipper `SceneManager` and `ViewDispatcher` flow, GPIO preflight checks for awkward key/paddle wiring, USB HID/MIDI modes, Sub-GHz RX/TX plumbing, and DMA-backed PWM sidetone on both the internal speaker and `P2/A7`. The tests run on the host, and the final FAP build goes through the real Flipper toolchain. The code is inspectable, tested, and built to be carried rather than merely screenshotted.

I wanted the sidetone to be something you can live with for more than a minute: no sharp pops, less buzzer-like rasp, and no waveform chopped off mid-swing. The Flipper has neither a bipolar speaker supply nor an audio amp, so the shaped sidetone rides on a high-frequency PWM carrier. On startup the sampled outputs begin at their quiet midpoint instead of stepping there; each tone is shaped, and release waits for the virtual zero crossing instead of snapping the output off wherever it happens to be.

The help section is fairly comprehensive, which means reading plain text on a 128x64 screen became annoying almost immediately. There is a custom renderer involved now: scrolling text, inline formatting, best-effort justified text, and tiny inline icons for things the Flipper has no business typesetting, including `µ`, arrows and bullets.

## Build

Latest checked build: Flipper firmware 1.4.3 with fbt API 87.1.

## Notes

- RF TX is jurisdiction-dependent. Know what you are transmitting, where and why.
- The Flipper radio is a short-range UHF tool, not a replacement for a proper HF CW rig.
- Ham-rig keying deserves boring electrical caution. Verify levels, polarity and isolation before connecting to equipment you like.
- Some 2026 cleanup was LLM-assisted, but the design decisions and launch behaviour are human-reviewed, tested, and checked on real hardware.

Idea and prototype by Richard, YO3GND.
