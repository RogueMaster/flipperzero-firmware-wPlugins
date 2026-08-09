# Straight Key Trainer

The Straight key trainer is the timing trainer for manual sending. [Listening practice](200-koch-listening-practice.md) checks whether you heard the letters. This screen checks whether you can send one clean character with a straight key: marks the right length, gaps in the right place, and dahs roughly three times as long as dits.

`Straight key trainer` lives under `Training`. Its settings live under `Settings -> Straight key trainer`.

## What It Trains

The trainer plays one target character, then waits for you to send the same character back. It compares your key-down times with the expected dit and dah lengths for the selected speed. A dit wants to be one unit, a dah wants to be three units, and the gap between elements inside a character wants to be one unit.

This is deliberately narrower than [Free Practice mode](220-free-practice.md). Free Practice decodes whatever you send and gives you a general workbench. The Straight key trainer gives you a small repeatable drill and points at the timing error.

The character pool is `A-Z` and `0-9`. Very short letters such as `E`, `T`, and `I` are less common, because single-element characters do not give much timing information.

## Settings

Open `Settings -> Straight key trainer` before running the drill.

- `WPM`: the reference speed used for the target character and for grading your answer. The range is 10-30 WPM.
- `Answer timeout`: how long the app waits for a completely blank answer before marking the round as missed. It can be set from 3 to 10 seconds.
- `Next delay`: how long the result stays on screen before the next character starts. It can be set from 3 to 15 seconds.

The trainer uses the normal [input settings](100-input-methods-keys-and-gpio.md). With `Input` set to `buttons`, `OK` is the straight key. With `Input` set to `straight`, use the configured GPIO straight-key input. This is a straight-key timing drill, so automatic keyer modes are disabled: the practice screen forces `Straight`, measures raw key-down and key-up timing, then decides whether each mark was a dit or a dah. If you normally use a GPIO paddle/keyer setup, it is treated as straight-key input here; when the GPIO probe has forced straight-key input, the `dit/SK` side is the contact used. Set [sidetone](102-audio-sidetone.md) before you start; audible feedback matters when you are training timing.

## Round Flow

Start the screen from `Training -> Straight key trainer`. With button input, press `OK` to begin. With an external straight key, press either `OK` or the key.

The Flipper plays a single character and shows the target on screen. After playback, send the same character back by hand. Once you stop keying, the app waits briefly for the answer to settle, scores the round, shows the timing result, and starts the next round after `Next delay`.

If the pause is too long, press a key or button during the result countdown. The next round will start after about one second instead of waiting for the full delay. `Back` leaves the screen.

## Screen Layout

At the start, the screen says `Straight key trainer` and prompts you to press `OK` or your key. During a round, the large character at the left is the target.

After your answer, the two timing strips show the comparison:

- the upper strip is the target timing.
- the lower strip is what you sent.

Long marks appear as wider high segments. Gaps appear as low segments. If the lower strip is visibly longer, shorter, or squeezed compared with the target, use that as the first clue about what to correct.

The bottom right shows the session count and percentage, such as `7/10 70%`. That percentage is the share of rounds where the keyed Morse symbols matched the target exactly. It is not a complete quality score; a round can match the right dits and dahs while still showing poor timing.

## Feedback Metrics

When the round can be graded, the result line shows four fields:

- `S`: the worst element-gap score.
- `di`: the worst dit timing score.
- `da`: the worst dah timing score.
- `r`: the average dah-to-dit ratio.

`OK` means that part was close enough. A number means it was not. The lower the number, the further that part of the answer was from the target timing. `r` should hover around `3.00`, because a dah is three dits. If it climbs, your dahs are too long or your dits are too short. If it falls, your dahs are too short or your dits are too long.

`FAIL` means the number of elements did not match the target. If the target was `K` and you sent something shaped like `C`, fix the character shape before worrying about the detailed timing metrics.

## How To Practise

Keep sessions short. A few focused minutes with a straight key are more useful than a long run where fatigue teaches your hand bad timing. Use a speed where you can send deliberately, then raise it only when the metrics are mostly stable.

Watch the worst field, not just the percentage. If `di` is bad, make dits more even. If `da` is bad, work on dah length. If `S` is bad, leave cleaner gaps between elements. If `r` drifts away from `3.00`, work on the relationship between dits and dahs rather than just the absolute speed.

Do not use this as the main way to learn new characters. Learn to hear them first in [Listening practice](200-koch-listening-practice.md), then use this trainer to make your sending honest. When you want to send longer text without being scored element by element, use [Free Practice mode](220-free-practice.md).

I will __repeat this__: if you are looking at the signal trace and thinking "Right, so Z is two dahs and two dits", you are not ready for sending. Keep doing listening drills.

## Related Pages

- [Input methods, keys, and GPIO](100-input-methods-keys-and-gpio.md) - button, straight-key, and GPIO input setup.
- [Audio sidetone](102-audio-sidetone.md) - buzzer, vibration, and `P2 (HD)` sidetone.
- [Listening practice](200-koch-listening-practice.md) - the main trainer for learning to copy characters.
- [Free Practice mode](220-free-practice.md) - unscripted live keying and decoder feedback.
- [Glossary](901-glossary.md) - Morse timing words such as dit, dah, fist, and WPM.
