# Changelog

## 1.4

- Fix the pen getting stuck down after pressing Ok together with a direction key: drawing mode was toggled by a long Ok press with no visual indication, and a short Ok press could not lift it
- Replace the hidden drawing flag with three modes cycled by a long Ok press: Move, Draw and Erase; the cursor shape shows the active mode (small dot, full cell, hollow frame)
- A short Ok press toggles the cell under the cursor in Move mode and lifts the pen or eraser in Draw and Erase modes
- Hold a direction key to keep moving (with the pen down this draws or erases a line); previously held keys did nothing
- Save the canvas on exit and restore it on the next start; a failed save shows a storage error message instead of silently losing the drawing
- Hold Back to clear the canvas and lift the pen
- Start with the cursor in the center of the canvas instead of the top-left corner

## 1.3

- Version bump for catalog compatibility (no functional changes)

## 1.2

- Version bump for catalog compatibility (no functional changes)

## 1.1

- Version bump for catalog compatibility (no functional changes)

## 1.0

- Initial release: draw on a 32x16 board with a dot cursor, hold Ok to toggle continuous drawing, hold Back to clear the screen
