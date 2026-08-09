# Changelog

## 1.4

- Compute the picture on the app thread, only when the view actually changed, and hand it to the draw callback as a ready-made bitmap. It used to be computed inside the draw callback, on the GUI service thread, and requested afresh at least ten times a second whether or not anything had moved; once enough pixels reached the iteration limit the app could no longer keep up with its own redraws and the device stopped responding to buttons
- Show a coarse preview first and refine it band by band, checking for key presses between scanlines, so a slow picture at high zoom can be interrupted instead of holding up the event loop
- Skip the iteration loop for points in the main cardioid and the period-2 bulb, the two large solid regions that always cost the full iteration budget
- Zoom around the middle of the screen; zooming used to shrink the picture towards its top left corner
- Halve or double the visible span per zoom step. The old step subtracted a fixed amount from the span, which drove it through zero and mirrored the picture after about two dozen presses
- Hold Ok to zoom back out, which the app previously could not do at all, and stop zooming in at 16384x, where single precision floats run out of mantissa
- Show the zoom factor for a moment after each step, so reaching either limit is visible instead of looking like the app stopped reacting
- Hold a direction key to keep panning, and pan by a fixed fraction of the screen so the step stays usable at every zoom level
- Keep the visible window within reach of the set, so panning cannot strand the view in empty space with no way back
- Use square pixels and raise the iteration budget with the zoom level: the picture used to be squashed horizontally by about a tenth, and deep views collapsed into a solid blob
- Point the app link at this repository, since the original author's GitHub account no longer exists

## 1.3

- Version bump for catalog compatibility (no functional changes)

## 1.2

- Version bump for catalog compatibility (no functional changes)

## 1.1

- Release the state mutex before asking for a redraw, instead of holding it across the request

## 1.0

- Initial release: browse the Mandelbrot set with the arrow keys, Ok to zoom in
