# Anim Previewer

Browse and preview Flipper Zero dolphin animations directly on the device. Opens at "/ext/dolphin" by default — navigate anywhere on the SD card to find animations.

# Controls

## Browser
| Button | Action |
|--------|--------|
| ↑ / ↓ | Scroll |
| OK | Enter folder / play animation |
| Back | Go up / exit app |

## Player
| Button | Action |
|--------|--------|
| OK | Play / Pause |
| ↑ / ↓ | Faster / Slower |
| ← / → | Step one frame *(paused only)* |
| Back (short) | Return to browser |
| Back (long) | Exit app |

# How it works

Uses the Flipper's built-in file browser. Navigate into an animation folder and tap "meta.txt" to play it. Back from the player returns to the same location in the browser.

Supports both compressed and uncompressed ".bm" frame files. Loads up to 128 frames per animation (~120 KB cap).

