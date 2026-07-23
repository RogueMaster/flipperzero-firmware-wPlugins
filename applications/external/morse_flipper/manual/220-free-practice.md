# Free Practice Mode

Free Practice is the live keying screen. There is no prompt, score, lesson, or pass condition. You key Morse, the app gives you sidetone and tries to decode your sending into text on the screen. If [USB output](310-usb-adapters.md) is enabled, this is the screen you use to operate Vail, VBand, or Morse games.

`Free Practice` is available directly from the main menu.

## Useful settings

Free Practice uses the normal keyer settings from `Settings → Keying`.

- `WPM`: saved default speed, 10-30 WPM.
- `Input`: buttons, straight, or paddle.
- `Keyer`: `straight`, `bug`, `plain iambic`, `iambic a`, `iambic b`, `ultimatic`, or `keyahead`.
- `Swap paddles`: swaps dit and dah sides when using paddles.

Set [sidetone](102-audio-sidetone.md) as well. Silent practice is rarely the point, unless you are training for a mime exam.

## Controls

- `Up` / `Down`: change WPM for this Free Practice session. This does not change the saved value in `Settings`; it is session only.
- `Left`: clear the decoded text.
- `Back`: leave the screen, unless `Back` is currently being used as a paddle.
- `Left` long press: leave the screen when `Back` is being used as a paddle.

With `Input` set to `buttons` and `Keyer` set to `straight`, hold `OK` to key. With `Input` set to `buttons` and a paddle keyer such as `iambic b`, `OK` and `Back` act as paddles. If the sides feel backwards, change `Swap paddles` in `Settings → Keying`, then settle on that mapping.

With external `straight` or `paddle` input, use the configured GPIO key or paddle. `Back` exits normally, and `Left` still clears the text.

This mode is useful if you want to practise pangrams, CWA-style assignments, QSOs, or plain text. The decoder is deliberately lenient and tolerant of mistakes. Do not use it to train your sending; use the dedicated [straight-key](210-straight-key-practice.md) and [groups-of-five](211-groups-of-five-drills.md) trainers for that.

## What The Screen Shows

The top rows show the app's decoded copy of your sending. Treat it as feedback, not a judge. If your timing is rough, the decoder will show rough copy or might still decode it. It accepts dits from half to twice the current dit estimate, dahs from two to five dits, letter gaps from 2.5 dits, word gaps from 6 dits, and resets timing after 12 dits of silence.

The lower rows show current WPM, input source, keyer mode, USB state, and a short control hint. If USB is enabled, Free Practice sends those keying events to the computer while also doing local sidetone and display.

## How To Use It

Use Free Practice for short checks: paddle feel, speed, sidetone, USB output, and whether your sending is roughly copyable. For learning new letters, use Listening practice first. For measuring straight-key accuracy, use the straight key trainer. Free Practice is a workbench, not a curriculum. It is also the mode you use when the Flipper is emulating a Morse adapter.
