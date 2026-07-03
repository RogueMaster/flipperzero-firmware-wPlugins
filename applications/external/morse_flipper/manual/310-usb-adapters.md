# USB Adapters

Morse Flipper can pretend to be the little USB box normally sitting between a telegraph key and a computer. That means a Flipper button, paddle, straight key, or GPIO input can drive browser practice sites and desktop software without buying yet another small box. Useful, and slightly rude to the small-box industry.

[Vail](https://vail.woozle.org/) is the cleanest target when using MIDI: it is a browser repeater for live Morse practice, echo testing, fortunes, and QSOs over the internet. [VBand](https://hamradio.solutions/vband/) is a virtual CW band with channels, a practice room, decoding help, and QSOBot. There are plenty more: websites, games, phone apps, and desktop tools. They mostly want one of three things from an adapter: keyboard keys, mouse buttons, or MIDI note events.

## How It Works

A normal adapter watches the key contacts and sends USB events to the computer. Morse Flipper does the same in software. Your selected input source still matters: buttons, straight key, paddle, and the active keyer mode decide what the app considers a straight note, dit, or dah. USB then decides how those events leave the Flipper.

Keyboard and mouse modes are compatibility modes. They work with almost anything, but the browser or game usually needs focus and can treat your lovely fist as ordinary key presses. MIDI is cleaner: the host receives explicit note-on/note-off events, and in Vail-style use the host can also talk back. Less pretending, fewer surprises.

## Flipper Settings

Open `Settings → USB`.

- `Connection`: `None` leaves the computer alone. `Keyboard` sends HID key presses. `Mouse` sends mouse button presses. `MIDI` exposes Flipper as `Morse Flipper MIDI`. Changing this reconnects USB, so give the computer a moment to notice.
- `Paddle keys`: used only in `Keyboard` mode for dit and dah. Pick the pair the site expects. VBand uses `[ ]`; other tools may prefer `X Z`, `Ctrl-s`, `. /`, or another pair. This setting does not change the real paddle wiring, only the fake keyboard output.
- `Straight key`: used only in `Keyboard` mode when the app sends a straight-key note. Choose `Space`, `X`, `Z`, `C`, `Enter`, or `Num enter` to match the target app.
- `Invert mouse`: used only in `Mouse` mode. It swaps left and right mouse buttons, because some tools make the obvious choice and some apparently had a longer lunch.

## Vail MIDI

In `MIDI` mode, Flipper sends USB MIDI note events: note `0` for straight key, note `1` for dit, and note `2` for dah. Presses are note-on, releases are note-off. That gives the browser proper edge timing instead of asking it to infer Morse from keyboard text, which is how tiny timing sins become large ones.

The useful bit is that the link is bidirectional. Vail-style software can send control data back: `CC1` sets speed, `CC2` sets sidetone pitch, and Program Change sets the keyer mode. Morse Flipper applies those live, so the browser can drive WPM, tone, and mode while the Flipper still does local keying and sidetone. This is the sensible way round.

## Sites And Games

Start with these:

- [Vail](https://vail.woozle.org/) - live internet repeater, echo testing, fortunes, and MIDI-aware practice.
- [VBand](https://hamradio.solutions/vband/) - virtual CW band, practice channel, QSOBot, and keyboard-adapter workflow.
- [Morse Invaders](https://morseinvaders.com/) - arcade sending practice.
- [Morse Battleship](https://tools.hamradioduo.com/morse-battleship/) - Battleship controlled by Morse.
- [Morse ATC](http://morseatc.s3-website.eu-central-1.amazonaws.com/) - air-traffic-control game using Morse commands.

The public tool pile changes often. If a site asks for a Vail or VBand adapter, try `MIDI` first if it supports WebMIDI; otherwise use `Keyboard` and set the key presets to whatever the site wants. If nothing happens, check browser focus before blaming the Flipper. Browser focus is a stupid failure mode, but it is a popular one.
