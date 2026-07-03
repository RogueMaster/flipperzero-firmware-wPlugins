# Keyers And Paddle Settings

A keyer is the timing engine between your hand and the [Morse output](102-audio-sidetone.md). In code terms, it is a small finite state machine with opinions. You press the dit side or the dah side; the keyer turns that into clean elements at the selected WPM. It also decides what happens when you hold a paddle, tap one while the other is being held, or squeeze both sides at once. That is where the useful part lives, and also where the little traps are kept.

Treat the keyer like a tiny musical instrument. Timing, muscle memory, and patience matter more than menu-hopping.

To change keying, open `Settings → Keying`.

## Contents

- [Settings](#settings)
- [Start With Iambic B](#start-with-iambic-b)
- [Iambic Timing](#iambic-timing)
- [Keyer Modes](#keyer-modes)
- [Single-Lever And Dual-Lever Paddles](#single-lever-and-dual-lever-paddles)
- [Paddle Side](#paddle-side)
- [Practice Notes](#practice-notes)

## Settings

- `WPM`: element speed for generated keying. A dit is one unit, a dah is three units, and the keyer fills the gaps. 20 WPM is considered normal traffic, and 15 WPM is a reasonable target to send at. Do not train receiving at 15 WPM; that is where counting comes back with a shovel.
- `Input`: where the paddle or straight-key signal comes from: `buttons` for Flipper's buttons, `straight` for a single GPIO pin, or `paddle` for two GPIO pins and a keyer engine.
- `Keyer`: the keying rule set. This matters regardless of what input you chose.
- `Swap paddles`: swaps dit and dah sides. Set this early and leave it alone unless the wiring is plainly wrong. I key with my left hand so I can type and write with my right hand; a long-standing tradition says that regardless of your choice of hand you should use the index finger for dahs and the thumb for dits. I'm not sure whether it is just superstition, tradition, or a choice that has any measured benefit.

With `Input` set to `buttons`, `OK` and `Back` act as paddles when the keyer mode is not straight. When `Back` is being used as a paddle, long-press `Left` to leave the live screen. The app shows ⏴ on the right as the reminder. The reasoning for this is quite silly: you can't press two buttons at once on the joystick, so another button must be used. The only remaining button is `Back`. It is ugly, but deliberate; that is why the hold-Left hint appears wherever Back becomes a paddle.

## Start With Iambic B

Use `iambic b` unless you are already familiar with another keyer. It is not magically better; it is simply the default here, it is a common modern option on real keyers and rigs, and it gives you the squeeze-keying behaviour people usually expect. Do not keep changing modes while learning. Mode A, Mode B, ultimatic, and single-lever technique all build different hand habits. The receiving station cannot tell which keyer mode you used. Your fingers can, and they will complain.

## Iambic Timing

Iambic keying needs a dual-lever paddle. Press one side and the keyer sends that element. Press both sides and it alternates: • — • — or — • — •, depending on which side made contact first.

Mode B is the eager one. When you release both paddles during a squeeze, it commits one extra opposite element. It does not interrupt the current sound. The keyer finishes the element already being sent, waits the normal one-dit element gap, then sends the extra element. That extra element is useful when you meant it, and mildly treacherous when you did not.

The clean example is the letter C, which is — • — •. Start with dah, squeeze both paddles, and release the dah during the second dah. The keyer finishes that dah, waits the normal element gap, and then adds the final dit. That same small release-timing detail is the difference between K and C: K is — • —; hold the squeeze a little too long in Mode B and the keyer quite reasonably gives you — • — • instead. It is not being clever. It is obeying you, which is sometimes worse. The useful detail is release timing: release the dah paddle during the second dah, late enough for Mode B to queue the final dit, while the dit side is still held.

CQ is a good iambic example. C benefits directly from squeezing, and Q, — — • —, can also be sent with less separate paddle motion than treating every element as a separate press. The gain is real, but not enormous. Analyses often quote about 11% fewer strokes for iambic keying over non-iambic paddle keying across full character sets, but normal on-air text is not a full-character treadmill. Common letters are already short, so the practical gain is smaller. CQ is also one of the very few examples.

## Keyer Modes

`straight` treats the input as one manual contact. Hold it down, it keys. Release it, it stops. Use this for straight-key practice or when you want no generated timing; the setting is mostly a convenience.

`iambic b` alternates during a squeeze and adds the extra opposite element after release. This is the recommended starting point here.

`iambic a` alternates during a squeeze and stops after the current element when both paddles are released.

`ultimatic` is not iambic with a funny hat. When both paddles are held, the last paddle pressed wins. Some operators like it because it feels direct and has fewer surprise alternations.

`keyahead` is a Vail-world mode, not a normal rig/keyer mode you should expect to find everywhere. It behaves least like a traditional keyer and most like a small two-key keyboard: each tap adds one dit or dah to a FIFO queue, and the sound catches up at the selected WPM. The timing between taps is ignored, so the input is not fussy in the usual iambic way. That is exactly why it is here; Morse Flipper can behave like the field keyer I wanted to carry, not merely the one the menu happened to inherit. The trade-off is spacing: because the queue contains only dits and dahs, not letter gaps, you can only key ahead inside the current character. Tap, do not hold, unless you actually want repeats. If you are serious about clean transmitting, learn this last. It is a useful trick, but relying on it is a poor habit unless the Flipper is the keyer you will actually have with you.

`bug` repeats dits automatically from the dit side and leaves dahs manual. That imitates the important part of a mechanical bug: the machine helps with dits, and the operator still owns the dahs.

`plain iambic` alternates when both paddles are held, with little extra memory. It is mostly useful as a simple reference behaviour.

## Single-Lever And Dual-Lever Paddles

A dual-lever paddle has two independent levers, so both contacts can be closed at once. That is what makes true iambic squeeze keying possible.

A single-lever paddle has two contacts but one lever. Push one way for dit, the other way for dah. It still needs a keyer for ordinary paddle operation, because the keyer is what generates the timed dits and dahs. What it does not need is an iambic keyer mode; mechanically, it cannot close both contacts at the same time.

Single-lever paddles are often reported as common among very high-speed operators and competition senders. The reason usually given is accuracy: less squeeze timing, fewer ways to accidentally ask for an extra element. Dual-lever iambic can use less motion for some letters, especially alternating ones such as `C`, `F`, `K`, `L`, `Q`, `R`, and `Y`, but it demands cleaner timing. That trade is personal. There is no useful public census of what every CW operator uses, and anyone claiming one is selling certainty by the kilo.

For regular 15-20 WPM traffic, a double paddle can work fine. If you want to learn high speed, you are probably better off with a single paddle.

There is a related thing called a sideswiper or cootie key. That can be used without a keyer, but then you are timing everything by hand. Do not mix those up unless you enjoy arguments with people who own too many brass objects.

## Paddle Side

The usual wiring is tip for dit and ring for dah. Morse Flipper follows that layout in its default GPIO wiring: `P7` is `dit`, `P5` is `dah`, and `P3` is the virtual gnd. Read more about [GPIO wiring](100-input-methods-keys-and-gpio.md).

If the sides feel backwards, change `Swap paddles`. Then stop touching it. A slightly awkward mapping learned once is better than a perfect mapping changed every week.

## Practice Notes

Do not practise paddle sending ahead of your listening. A keyer can make rubbish sound tidy. That is still rubbish, only better dressed.

For first paddle practice, use `Input: buttons`, `Keyer: iambic b`, and listen to what `OK` and `Back` do before plugging in hardware. Then use a real paddle. Keep sessions short enough that your hand stays relaxed. Force is not a missing feature.
