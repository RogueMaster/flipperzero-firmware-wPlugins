# FlipperDrivin
'Ard Drivin' is an Arduboy racing game

Coding: Rem
Graphics: LP
Port: apfxtech

Technical info so far:
- Game runs at a constant rate of 67 frames per second
- Rendered in 3 colors: black, gray, white
- Grayscale is achieved by flickering quickly between black and white, and also alternating the dithering pattern, so it looks more stable. The resulting grayscale is almost perfectly stable, except for a sort of visible subtle scanline glitch
- Sprite scaling (pre-rendered scaled sprites, to be precise)
- Sound (using ArduboyTones library)
- It uses Arduboy2 lib, although most of the drawing routines have been written from scratch
- Custom font!
- png to Arduboy conversion made using Processing 3


## Original

**rveilleux**
[ard-drivin](https://github.com/rveilleux/ard-drivin.git)

Link on Arduboy.com: http://community.arduboy.com/t/ard-drivin-is-coming/3178