# JezzBall for Flipper Zero

A compact JezzBall clone built as a standalone Flipper Application Package
(`.fap`). Complete walls to divide the arena. Every new region without a ball
is captured; secure 75% of the field before time runs out.

## Features

- Six levels with increasing ball counts
- Two-ended wall growth and classic unfinished-wall collisions
- Lives, score, level timer, pause, game-over, and victory states
- Compact monochrome graphics designed for the native 128×64 display

## Controls

- **D-pad:** Move the builder. Left/right select a horizontal wall; up/down
  select a vertical wall.
- **OK:** Start building in the selected direction.
- **Back:** Pause or resume.
- **Long Back:** Quit.

A ball touching an unfinished wall destroys it and costs one life. Completed
walls are permanent. The game has six levels, with more balls and a little more
time on each level.

## Build and launch

Install [uFBT](https://pypi.org/project/ufbt/), then from this directory run:

```sh
ufbt
```

The resulting app is written under `dist/`. To install and immediately launch
it on a USB-connected Flipper Zero:

```sh
ufbt launch
```

The manifest targets official Flipper Zero (`f7`) firmware and places the app
in **Apps → Games**.

## License

[MIT](LICENSE)

## Host-side game logic tests

The rules engine has no firmware dependencies and can be checked on a desktop:

```sh
cc -std=c11 -Wall -Wextra -Werror \
  tests/test_jezzball_game.c jezzball_game.c -o /tmp/jezzball-tests
/tmp/jezzball-tests
```
