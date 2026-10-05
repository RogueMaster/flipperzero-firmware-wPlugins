# The mark

The identity is one mark - the contact plate of a chip module, divided into six
pads - drawn wherever it is needed by
[assets/make_logo.py](../assets/make_logo.py).

```bash
uv run --with pillow python assets/make_logo.py
```

## Why it looks like that

It is designed at 10x10, because that is where it has to survive: one bit per
pixel, no antialiasing, in the launcher. Nothing in it is thinner than two
pixels, so it never degrades into a dither pattern, and every larger form is
the same grid scaled by a whole number. That is the whole constraint, and it is
why the mark is six pads rather than anything drawn.

The banner sets the wordmark in type, so the frame around it is measured from
the font's own advance widths rather than guessed, and each line carries a
`textLength`: a reader whose machine has none of the fonts in the stack gets the
wordmark fitted to the frame instead of cropped at its edge. The ink follows the
reader's colour scheme, because GitHub renders an SVG in a README as an image,
and `currentColor` there is black.

## The files

| File | Purpose |
| --- | --- |
| `images/emrtd_10px.png` | The `fap_icon`; 10x10 and one bit, as the manifest requires |
| `images/EmrtdChip_24x24.png` | The same mark at two pixels per cell, which the read view draws as `I_EmrtdChip_24x24` |
| `assets/logo.svg` | The mark as one even-odd path beside the wordmark, not a traced bitmap; 718x240, measured to fit |
| `assets/logo.png`, `assets/logo.txt` | The raster and text forms of the same mark |

All four are generated from the one grid definition in the script, which checks
the size, the bit depth and the absence of metadata after it writes each file.

One image in `images/` is not the mark: `EmrtdDonateQr_58x58.png`, the QR code
the Donate screen draws, is encoded from the README's Support link by
[assets/make_donate_qr.py](../assets/make_donate_qr.py), which reads the code
back after it writes it.

## Why an unused icon costs something

`fbt` compiles every image in `images/` into `emrtd_icons.h` and does not strip
what goes unused, and a `.fap` is loaded into RAM, so an icon nothing draws
costs the read the memory it occupies. An image is added to `images/` only when
something draws it.

The constraints a change has to satisfy are in
[../CONTRIBUTING.md](../CONTRIBUTING.md).
