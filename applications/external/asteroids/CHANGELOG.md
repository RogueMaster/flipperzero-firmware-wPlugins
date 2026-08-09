# Changelog

## 1.3

- Fix the stray lines running across the screen when an asteroid, the ship or a bullet moves over an edge. An off-screen point reached the display as a coordinate of about 65000, and the line drawn to it was walked pixel by pixel over the full width or height of the screen, which is also what left the game, and the Flipper along with it, unable to keep up. Every line is now clipped to the screen first
- Draw an object that straddles an edge on both sides of the screen, so it comes into view opposite as it leaves, instead of being flattened against the border
- Play the thruster buzz from the game loop instead of from the drawing callback. The callback runs on the GUI service thread, where waiting for room in the vibration queue holds up input for the whole device
- Cap the ship speed. Holding thrust used to build up a velocity that moved the ship a third of the screen between frames, straight through asteroids and far too fast to steer
- Fire the drone buddy every two seconds, as its fire rate was meant to be read: it was counted in game ticks but compared against the millisecond clock, so the drone fired on every frame
- Play one longer crash buzz per collision, instead of restarting a short one on every frame of the hit animation
- Buzz once per tick when the shield breaks a cluster of asteroids, rather than once per asteroid
- Keep the shield around the ship at the edges of the screen, instead of snapping it eight pixels inwards
- Save the high score on its own, rather than dumping the whole game structure to the card with its live pointers in it. That file had to match the structure byte for byte, so any change to the game quietly reset everyone's score and left a random number in its place; this release resets it one last time, and reports a failed save instead of losing it silently
- Stop the game starting up paused, which it did if it was last left paused when you held Back to quit
- Set the game up before the screen is handed to the system, instead of a moment after
- Drop the per-frame trigonometry and the per-tick log lines, so less work happens between frames

## 1.2

- Add a splash screen, the shield, extra life, nuke and drone buddy power-ups, and the high score screen

## 1.1

- Version bump for catalog compatibility (no functional changes)

## 1.0

- Initial release: the classic asteroids game, with rotation on Left/Right, thrust on Up, brake on Down and rapid fire on Ok
