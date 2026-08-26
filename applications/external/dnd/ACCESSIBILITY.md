# Accessibility conventions

- High-contrast monochrome rendering uses filled selection rows and text labels rather than color.
- Important states use text, symbols, and numeric values; they never rely on a sprite alone.
- Saving throws, skills, passive statistics, spell values, HP, Armor Class, and resource counts use abbreviated labels when needed to keep values visible.
- Dice animation is short, code-drawn, and can be skipped with Back. No rapid full-screen flashing is used.
- Every multi-die roll retains an individual-results view and a numerical sum.
- Long-press alternatives are documented in headers, status text, README controls, or the relevant detail screen.
- Truncation preserves the start of a name and uses an ellipsis. Full custom descriptions remain stored even when a compact list row is shortened.
- Navigation labels are compiled directly into the FAP so accessibility does not require a runtime string table or extra heap.

Physical legibility, repeat timing, and all long-name cases remain listed in `DEVICE_TEST_MATRIX.md` until tested on hardware.
