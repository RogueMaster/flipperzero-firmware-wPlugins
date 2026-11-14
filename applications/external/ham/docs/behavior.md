# Behavior

The next episode depends on the previous episode:

| After | Type | Read | Move | Edit |
|---|---:|---:|---:|---:|
| Type | 0% | 65% | 30% | 5% |
| Read | 55% | 5% | 37% | 3% |
| Move | 50% | 50% | 0% | 0% |
| Edit | 10% | 65% | 25% | 0% |

Edits become typing when there is no known recent text. Each movement episode is
an interlude of several movements before the next episode is selected.

- Typing begins at a session pace between 100–130 WPM, varying gradually by one WPM.
  Burst targets are 85% at 3–20 words, 12% at 21–35, and 3% at 36–50.
  Sentence punctuation does not prematurely terminate the burst. Very long words
  can hit the 507-character burst buffer before reaching the word target.
- Keys are held for 22–48 ms. Inter-key intervals are scheduled from the key-down
  time with bounded variation around the session pace, compensating for the
  main loop's 10 ms timing resolution. Word boundaries have an 8% chance of an
  extra 60–180 ms pause. A burst has at most one hesitation, with a 2% chance per
  word and duration 250–700 ms. Punctuation has a 25% chance of 100–350 ms extra.
- Lowercase letters have a 0.5% typo chance, capped at one per burst, followed by
  backspace and the intended letter. Edit episodes erase and retype 1–5 known
  recent characters. Caret-changing clicks and manual pauses invalidate that
  recent-text history.
- Reading pauses at 50% activity: 80% are 3–12 s and 20% are 15–35 s.
  After a burst of 20+ words this becomes 55%/45%. A reading pause has a 20%
  chance of one tiny incidental pointer movement halfway through.
- Mouse interludes contain one movement 15% of the time, 2–3 movements 60% of the
  time, or 4–5 movements 25% of the time, with 60–220 ms between legs.
  Destinations are 70% nearby offsets (up to 16% of the configured radius),
  25% medium jumps within the central 55% of the radius, and 5% broad sweeps
  within 90% of the radius. Local moves gently favor the center to reduce outward
  wandering. The default radius is still 600 relative HID counts; it is a maximum
  estimated area rather than the typical movement distance. Cubic Bezier paths
  use smooth acceleration/deceleration with updates scheduled every 10–20 ms.
  Durations are 70–150 / 100–200 / 130–240 ms for local / medium / broad moves.
  Ten percent add a small final correction over 80–200 ms.
- After each movement, an 80–220 ms settling pause precedes an optional single
  click (default 25% chance). Clicks last 60–140 ms. No double-clicking or dragging.

These are tunable simulation heuristics, not a validated model of human behavior.
The engine never inspects or reacts to ChatGPT responses; reading is a timed pause.
