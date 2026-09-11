# The browser build is the specification

BEEPBACK exists twice: a browser prototype and this firmware, and they
have to be the same game. The browser build is where every timing and
scoring number was settled and where the screens were argued over, so
where the two disagree, the browser is right unless we change both.

Keep a copy of the shipped `beepback.html` next to this file. It is the
only complete statement of what the game does - HANDOFF.md gave the
constants and the navigation map, but not the screens, and building from
constants alone produced coherent screens that were not the agreed ones.

## Checking the two against each other

- `tools/trace.sh` dumps every draw call this firmware makes, per screen.
  The browser has a matching tracer. Diff the two.
- Small x-offsets on centred text are expected and are not bugs: the host
  width table only approximates the device font. Compare the shape.
- `test/test_rules.c` pins the scoring maths, and `test_challenge.c` pins
  the daily step for step. Those numbers are shared and must not drift.
