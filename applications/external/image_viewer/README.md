# Image viewer

Image viewer for flipper zero!

# How to use:

1. Create image with [img2fbm](https://github.com/atomofiron/img2fbm)
2. Drop image to SD card
3. Run image viewer app
4. Select image
5. Done! :)

The RM port accepts a full-screen raw bitmap or an uncompressed `.bm` file
with a zero format byte, including the bundled examples. Invalid sizes and
short reads display an error instead of drawing incomplete data.

Rendering update based on [PR #3 by B0L1V3RS](https://github.com/polioan/flipper-zero-image-viewer/pull/3),
with bounded reads and an RM bitmap API adaptation.
