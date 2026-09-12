# BEEPBACK

A memory game for Flipper Zero. Five buttons, five tones. The Flipper plays
a sequence and you play it back, one step longer every time.

Then it starts changing the rules on you.

## Modes

- **CLASSIC** — the sequence grows a step at a time. Clear a round and a
  longer one begins. Three lives.
- **RULES** — the same game, except every round hands you a rule to obey.
  SKIP DOWN means the sequence plays DOWN and you do not press it. SWAP,
  BACKWARDS, NO DOUBLES, EVERY OTHER, DOUBLE, LAST TWICE. The rule holds for
  the whole round and changes when the round clears.
- **REFLEX** — one cue at a time. Hit the matching button before the bar
  empties. The window shrinks with every hit and one miss ends the run.
- **CHALLENGE** — pick a rule and keep it. No rounds, no reset: one sequence
  that never stops growing.
- **DAILY** — one rule, one seeded sequence, one attempt a day. Every Flipper
  in the world gets the same run, so a score means something. It resets at
  midnight.

## Assists

Play it however you can. The assist setting decides what the game gives you
besides the sound:

- **EARS** — tone only. The hardest way, and the one the game was built for.
- **LED** — each button lights its own colour.
- **SHAPES** — a shape per button on screen.
- **ARROWS** — the button itself, drawn.

Records are kept separately for each one, so playing by ear competes against
playing by ear.

**HAPTIC** is separate and works alongside any of them: the motor traces every
note of every pattern, so you can feel a sequence as well as hear it. With the
volume at zero it becomes an assist in its own right.

## Scoring

Ten points a note, a hundred for clearing a round. That is the whole system.
TIME and SPEED change how hard a run is, not what it pays, so the number on
screen is always one you could have worked out yourself.

Every combination of mode, time, speed and assist keeps its own record, and
NEW BEST is judged against the one you actually played.

## Controls

- **UP, DOWN, LEFT, RIGHT, OK** — the five game buttons
- **BACK** — pause during a run, and the way back everywhere else
- From the menu, **RIGHT** walks along to your stats, your records and the
  credits, and **LEFT** walks back

## Records and stats

STATS counts what you have done: runs, playtime, rounds cleared, notes played
back right, your longest sequence and the mode and assist you reach for most.

RECORDS is a table per mode. For CLASSIC, RULES and REFLEX it shows every TIME
against every SPEED at once, and OK turns it to the next assist.

## Source

Built with uFBT against the release firmware. The game logic is separate from
the Flipper API and runs on a host machine under a test suite of over 250 checks,
including a fake canvas that fails the build if any screen draws outside
128x64 or one string lands on another.
