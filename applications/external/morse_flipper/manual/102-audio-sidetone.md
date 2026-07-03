# Audio Sidetone

Sidetone is the sound that follows your Morse input, whether that [input](100-input-methods-keys-and-gpio.md) comes from Flipper buttons, a straight key, or paddles. It is feedback for your ear and hand, not proof that RF went out or USB data made it to the computer.

Good sidetone needs three things:

1. Low latency.
2. A tolerable sound.
3. A keying state that matches the RF signal being generated.

The sidetone can be configured from `Settings → Audio output`.

Low latency means the delay from key press to sound should be as short as possible. Every part of the chain adds delay: paddle, Flipper input, USB stack, computer, browser audio mixer, audio card, and speakers. Morse trainers and games suffer quickly from that: you hear your own input late, so you correct late. Morse Flipper therefore generates its own sidetone, first through the internal buzzer.

The buzzer is immediate and convenient, but it is a square wave and gets tiring during long practice. `P2 (HD)` generates a pure sinewave sidetone on GPIO P2 for an external audio input, small amplifier, or speaker circuit. Some Bluetooth speakers still have line-in, which is useful when the gods of product design briefly look away. Add a small 1-50µF capacitor between P2 and ground if the carrier whine needs taming.

The P2 output is still a 0-3.3 V GPIO signal, not a real audio amplifier. The app builds a software midpoint around 1.65 V, ramps into that midpoint before starting audio, starts the tone at a zero crossing, and fades in and out. That avoids the hard jumps which otherwise turn into speaker pops. This is fussy, but it is exactly the sort of fussy that makes repeated practice tolerable.

Most screens do not generate RF. In [Transmit](300-transceiver.md#transmit), Sub-GHz keying and sidetone are driven from the same note state, so they track closely enough for this sort of experiment.

## Settings

- `Audio path`: chooses where sidetone goes. `Buzzer` uses the internal Flipper speaker. `P2 (HD)` sends high-quality audio on the P2 pin. `Vibration` uses the vibration motor instead of audio.
- `Frequency`: chooses the sidetone pitch, from `G2` up to `B6`. This is shown as `n/a` when `Audio path` is `Vibration`. Hams, by convention, set up sidetone to 700 Hz, which is roughly an F5 note.
- `P2 Volume`: sets the `P2 (HD)` level from 10% to 100%, in 5% steps. It does not control the internal buzzer volume.

Use `Buzzer` when you need the simple option. Use `P2 (HD)` when you can wire proper audio. The difference is not subtle.

## Where It Applies

This setting affects practice screens, Free Practice, the Sub-GHz monitor, and normal keying feedback.

Ham Keyer mode is the practical exception. It keys an external rig on `P15` and controls PTT on `P16`. In normal break-in use, the rig is expected to provide its own sidetone while it is being keyed. With break-in off, Morse Flipper uses the internal buzzer rather than `P2 (HD)`: carrying a separate speaker just for the few times you want local sidetone without TX is a poor bargain, and most rigs already have their own break-in-off sidetone setting.

When using MIDI with software such as Vail, the host may temporarily set speed, keyer mode, or sidetone pitch. That does not rewrite your saved audio setting; it is session control from the browser.

## Practice Notes

Choose a pitch that is easy to hear without being sharp. Most people settle somewhere in the middle. Too low becomes mush. Too high becomes tiring. If in doubt, start around 700 Hz, roughly F5. That is where many rigs and operators already live.

For learning, keep sidetone enabled. Silent keying is useful only when you already know what your hand is doing, and even then it is a bit theatrical.
