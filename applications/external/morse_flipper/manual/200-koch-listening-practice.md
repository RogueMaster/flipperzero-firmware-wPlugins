# Listening Practice

`Listening` plays short groups from the current lesson pool, waits for your answer, checks the letters you heard, and repeats until the session is done. This is where most of the early work belongs. Sending practice is useful later; first you need to hear the letters without reaching for a chart.

This is worth repeating: __if you want to learn CW, learn to decode early on__. You can hone your transmitting skills, but do not use a CW table or notes to read the message you want to transmit; keep it in your head. If you need to look up a letter before sending it, you are not ready to send that letter.

From the main menu, use `Training → Listening` to run the trainer. Use `Settings → Listening` to change the lesson, speed, spacing, and session length. The idle screen says `Listening`; press `OK` to start. With external key input selected, pressing the key can also start the session.

<img align="right" src="images/ss7.png" alt="Listening result example" height="256">

In this example the question was PWRU, four letters. The answer entered was RWRU, so the first answer cell is black: R was the mistake. The other three letters still count, because scoring is by letter position, not all-or-nothing. The session score is now 50%. This is question 2 of 4; at four letters per question, that means 4 correct letters out of 8 so far. Since this group missed only one letter, the previous group must have missed three. The centre `3` is the pause countdown, so the next group starts in 3 seconds. We are currently at lesson 12, letter I. The triangle on the right means the Flipper buttons are being used as paddle input. Back is a paddle here, not an exit. Hold Left to leave.

## Contents

- [Settings](#settings)
- [Question Flow](#question-flow)
- [Screen Layout](#screen-layout)
- [Scoring](#scoring)
- [How To Progress](#how-to-progress)

## Settings

The trainer and its settings are separate screens. Run it from `Training → Listening`; configure it from `Settings → Listening`.

- `Lesson`: the Koch lesson. Lesson 1 starts with K and M; later lessons add one new character at a time. This follows the LCWO.net lesson order.
- `WPM`: character speed. This is the speed inside each letter. Use `20` only for the first shakedown if the controls are new; for real progress, use 25-30 WPM.
- `Farnsworth`: spacing speed. This slows the gaps between letters without making the letters themselves soggy. A gentle first setting is `12`.
- `Answer timeout`: how long the app waits after playback before recording a blank answer as a miss. It can be set from 3 to 10 seconds.
- `Group pause`: delay after a result before the next question. It can be set from 3 to 15 seconds.
- `Group size`: number of characters in each question, from 1 to 9.
- `Groups`: number of questions in the session, from 3 to 30.
- `Chars`: `lesson` uses the normal Koch pool. Custom sets are covered in [Custom lessons and character sets](201-custom-lessons-and-character-sets.md).

The first time you open this screen, a session is 10 questions of 3 letters each. That gets boring quickly. Raise the `Groups` setting until a run takes about 2-3 minutes. Do not reduce `Group size` early on. Three letters train your "brain's buffer": hear each letter, keep the group in your head, then enter it. Do not dump letters the instant they arrive, and do not use a computer, pen, or paper. Remembering what you copy is part of copying. Annoying, but there we are. Remember LCWO.net sends groups of _5 letters_ by default.

## Question Flow

When a question starts, the app plays the group and hides the letters. You answer by keying the group back. With `Input: buttons` and `Keyer: straight`, hold `OK` for the sound and release it for silence. With straight-key input, the exact speed of your answer matters less here than in the straight-key trainer: this page is trying to decode the letters you sent, not grade your fist. You still need to answer before the timeout. The machine is lenient, not asleep.

After playback, the answer timeout is the blank-answer limit. If the app still has no answer before it expires, the group is missed and scores zero. Four missed groups in a row abort the session. After each result, the app waits for the next group. The centre of the status line shows the countdown. If the pause is too long for you, or you feel like you're getting in the flow, press the key or paddle; it starts the next question in one second. That is useful when you are in rhythm and the machine is being leisurely.

If `OK` and `Back` are acting as paddles, `Back` no longer exits. The escape hint is the same as in the other live screens: ⏴ long-press `Left` to leave. Remember this before blaming the button for doing exactly what you told it to do.

## Screen Layout

The top row is the question row. During playback it shows placeholders as letters are played. During answer entry it stays hidden. After the result, it reveals the expected group.

The bottom row is your answer. It updates in real time as you key. After the result, the answer cells under wrong question letters are drawn black. Correct letters stay normal. If a wrong letter turns black, do not argue with it; listen to the sound again next time.

The title line shows the lesson or active character set. With the normal lesson order it looks like `Lesson 1 - K M`. With a custom set, it shows the set name or the active character pool.

The status line has three parts:

- left: current letter score percentage for the session so far.
- centre: seconds until the next group, shown only during the pause.
- right: current question count, such as `4/10`.

The progress bar at the bottom is question progress, not score. It fills as questions are asked.

## Scoring

Each group is scored by letters in the right positions. A three-letter group with two correct letters scores 66 for that group. The final score is the percentage of correct letters across the whole session. At 90% or better, move on. The final screen will say `Try the next lesson`. If you are answering with a straight key, do not use this score as a straight-key quality score. It is a listening and recognition score. Use the straight-key trainer when you want the timing of your sending judged properly.

## How To Progress

For the first few minutes, `WPM 20` and `Farnsworth 12` are fine while you learn the screen. After that, and for the first ten lessons, keep character speed at 25-30 WPM. Use 30 WPM if you can stand it, 25 WPM if 30 is still nonsense. Do not lower WPM just because the lesson is hard. Lowering character speed makes counting easier, which is exactly the habit we are trying not to feed.

Use Farnsworth to adjust difficulty. Start with generous spacing, then raise `Farnsworth` as your score reaches about 90%. When that becomes comfortable, move to the next lesson and give the new letter a bit more spacing again. The letters should stay quick; the gaps can do the mercy work.

The first skill you need when learning Morse is catching yourself counting. If K becomes "dah dit dah" in your head, you are rationalising the sound. If M is only "the short rhythm after K", you have learned the order of the exercise, not the letter. The target is more like multiplication tables: the sound should land as the answer, without a little committee meeting in between.

**Most of your practice time should be listening.** Do not rush into sending drills, and do not perfect sending for letters you cannot reliably copy. Transmitting on the bands while proving you cannot copy reliably is a good way to collect complaints.

However, if you feel like you can copy, do not let perfection stop you from getting on the bands. Many hams are very tolerant of newcomers, especially if they can prove they put in some effort. You can always use a decoder to get a feel for how a QSO goes, but... you're using this trainer to avoid needing an actual decoder, right? Right?
