# Ham Keyer And Logger

Ham Keyer turns Morse Flipper into a pocket field keyer and logger for a real amateur radio transceiver. It is not the same thing as the [Flipper Sub-GHz transceiver](300-transceiver.md): the Flipper radio is not involved. In this mode the Flipper sits between your key, your rig, and a text file. It keys the rig through GPIO and can send short stored messages from the buttons, but memory CW is not the interesting part; plenty of rigs and keyers already have that trick. The useful bit is that Morse Flipper decodes what you actually send, from either a button macro or the paddle, and writes it to the SD card as plain text. That means the log lives in the same small box that is already doing the keying, not on the phone you have just sacrificed to logging, a spare phone you remembered to charge, or a notebook demanding another hand at exactly the wrong moment.

The intended use is simple: you are `YO3GND`, sitting somewhere damp enough to count as portable, and one Flipper button is assigned to transmit `CQ CQ CQ POTA DE YO3GND K`; someone answers, you receive `FL1PPR` by ear, and you confirm him by keying `FL1PPR` back with the paddle; that callsign is decoded into the on-screen history and, if logging is enabled, into the log; then you press the button assigned to your report macro, for example `Down`, to send `UR 5NN 5NN TU BK`. So the shape of the contact is button CQ, received callsign, paddle confirmation, button report, and one continuous log of what actually went out. The app is not trying to run the QSO for you. It keeps the repetitive sending and the field log in one place while leaving the received callsign and any judgement calls with the operator, where they belong. The paddle stays available for callsigns, park references, notes, corrections, and anything else that would make a rotten macro.

## What It Controls

Ham Keyer uses the configured [key or paddle input](100-input-methods-keys-and-gpio.md) for manual sending. The Flipper buttons are not the key in this mode; they are the controls for stored messages. If the app is still set to button input, Ham Keyer refuses to start and asks you to connect a paddle or straight key.

The rig side is fixed:

- `P15`: key output to the transceiver.
- `P16`: PTT output, if enabled.

Read the GPIO notes in [Input methods, keys, and GPIO](100-input-methods-keys-and-gpio.md#changing-gpio-pins) before wiring this to a radio. Do not connect a Flipper GPIO pin directly to an unknown keying circuit and call it bravery. Use a transistor or optocoupler, verify polarity and levels, and think about ground loops before the field day has already become electrical archaeology.

## Set Up The Mode

Before opening Ham Keyer, set the input and keyer behaviour:

1. Open `Settings -> Keying`.
2. Set `Input` to `straight` or `paddle`.
3. Pick the keyer mode and speed you want. Paddle behaviour is covered in [Keyers and paddle settings](101-keyers-and-paddle-settings.md).
4. Open `Settings -> Keying -> GPIO` and check the key/paddle pins, `P15` key output, and `P16` PTT output if you use PTT.
5. Open `Ham Keyer`.

The Ham Keyer menu has `Start`, `Logging`, `Configure messages`, and `View key assignments`. Logging is normally on. If it is off, the screen still works as a keyer, but you lose the reason this mode exists: the SD-card trail you can clean up later.

## Messages And Buttons

Messages are short stored CW lines. You can add, edit, copy, delete, and assign them from `Ham Keyer -> Configure messages`. The five assignable controls are:

- `Up`
- `Down`
- `Left`
- `Right`
- `OK`

In the run screen, a short press on an assigned button sends that message. If a macro is already sending, `Back` stops it. Keep messages short and tactical: CQ calls, reports, park references, `P2P`, and sign-off lines. Anything that requires thought should be sent with the paddle.

The defaults are POTA-shaped rather than sacred. They are there to give you a usable starting layout, not to define how your activation should sound. A typical setup might put a CQ on `OK`, a report on `Down`, a park reference on `Left`, and a thank-you line on `Right`.

## Break-In

`Back` toggles break-in during the run screen. Long-press `Left` exits.

With break-in on, Ham Keyer keys the rig. `P15` follows the Morse being sent, whether it comes from a stored message or from your key, and `P16` handles PTT if enabled. In normal use, the rig provides its own sidetone while it is being keyed.

With break-in off, the app stops keying the rig. Manual paddle or straight-key input is still decoded and, if logging is enabled, logged, but `P15` and `P16` stay idle. Morse Flipper generates sidetone on the internal buzzer because the rig no longer will. The log also records `[BKOFF]` and `[BKON]` markers when you toggle the state.

That makes break-in off useful for field notes. If you copied `FL1PPR` badly, or you want to add a comment without transmitting it, press `Back`, key the correction or note, then press `Back` again before transmitting. With logging on, the note lands in the file without being sprayed across the band.

## Logs

Ham logs are plain text files on the SD card:

```text
/ext/apps_data/morse_flipper/morse-flipper-ham-keyer-YYYY-MM-DD.txt
```

The app uses one file per day. Each logged block starts with a timestamp, then the decoded text and any break-in markers. The app flushes log text after the keyer goes idle for a few seconds, and also when you leave or the app shuts down cleanly.

After the activation, read that file from the SD card and process it into whatever your logging target expects. For POTA or SOTA, treat the Morse Flipper file as field notes: callsigns, reports, and sequence are there, but you should still review the log before submission. CW copy in the field is allowed to contain evidence that the field was, regrettably, real.

## Audio Notes

When break-in is on, expect the transceiver to provide sidetone. The rig is already doing the grunt work of the QSO, and most rigs make perfectly decent keying audio while they are being keyed. When break-in is off, Morse Flipper uses the internal buzzer for local notes. I do not expect anyone to bring a separate `P2 (HD)` speaker for the notes case; the rig handles the operating audio, and the buzzer is enough for quick local feedback. Ham Keyer therefore does not use `P2 (HD)` or vibration in the run screen. If you do bring a speaker and specifically want Flipper-generated sidetone in this mode, [open a GitHub issue](https://github.com/yo3gnd/morse-flipper/issues); that is a feature request, not a wiring puzzle.

## Related Pages

- [Input methods, keys, and GPIO](100-input-methods-keys-and-gpio.md) - key/paddle input and rig-control pins.
- [Keyers and paddle settings](101-keyers-and-paddle-settings.md) - iambic modes, paddle swap, and timing behaviour.
- [Audio sidetone](102-audio-sidetone.md) - buzzer, vibration, and high-quality sidetone notes.
- [Flipper as a Morse transceiver](300-transceiver.md) - the separate Sub-GHz experiment mode.
- [Glossary](901-glossary.md) - CW operating words such as BK, CQ, QSO, and RST.
