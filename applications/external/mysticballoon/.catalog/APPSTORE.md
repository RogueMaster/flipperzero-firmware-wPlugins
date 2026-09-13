# Mystic Balloon

An atmospheric 2D platformer in 39 hand-built levels, ported from the Arduboy to the Flipper Zero. The kid cannot fly, but he can hang under balloons: press jump again in mid-air and one unfolds above him, so he sinks slowly instead of falling. Fans blow columns of air that carry a floating kid across a room, and everything a level asks for - the key, the coins, the walkers - has to be reached that way.

This port is a fork of [Mystic Balloon](https://github.com/Team-ARG-Museum/ID-34-Mystic-Balloon/) by TEAM a.r.g. - GAVENO, CastPixel, JO3RI and Martian220 - released under the MIT license. The Flipper version is rewritten on top of a native 1-bit framebuffer and the Flipper input, speaker and storage APIs, with no Arduboy emulation layer underneath.

## A level

Each of the 39 levels is a 24x24 tile map, larger than the screen, with the camera trailing the kid. Somewhere in it are a key, a door and up to six coins. The door stays shut until the key is picked up; standing in the doorway and pressing Up ends the level.

Every level starts you with three balloons, and they are your health and your air time at once. Spikes and walkers take one away, falling off the bottom of the map takes one and drops you back at the start, and losing the last one ends the run. The more balloons you still carry, the slower you sink while floating: a full set of three barely drifts down, the last one drops fast.

## Gameplay

- **Balloons** - OK jumps, OK again in the air opens a balloon, releasing it lets go. You keep steering left and right while hanging.
- **Vacuum** - hold Back to inhale. Coins slide toward you, and a walker held in the stream is worn down and swallowed: 50 points and a balloon back, or 150 points if you already carry three.
- **Fans** - blow up, left or right, and push only a kid who is currently on a balloon. Aimed well, one carries you the whole width of a room.
- **Spikes** - sit on any of the four sides of a block and run in rows along walls, floors and ceilings. A touch costs a balloon.
- **Score** - 200 per coin, and 500 for the one that clears a level's set. On the level screen every collected coin adds 20 more and every surviving balloon 30. The best run and its coin count are kept as the high score; collect all 234 coins in one run for the super badge.

Progress - current level, coins, score and high score - is written to the SD card, so Continue picks the run up where you left it. Sound effects can be turned off in the menu, and the game stays quiet on its own when the Flipper is in stealth mode.

## Controls

In a level:

- **Left / Right** - walk, and steer in the air
- **Up / Down** - look further up or down
- **Up** - in the doorway, holding the key: finish the level
- **OK** - jump; in the air, hold to hang on a balloon
- **Back** - hold to vacuum in coins and walkers
- **Back + Down** - pause; OK resumes, Back returns to the menu

In the menu the arrows move the selector, OK confirms and Back steps back. Back on the title screen closes the game.
