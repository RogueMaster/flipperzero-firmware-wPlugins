## Draw Commands
The Video Game Module sends draw commands over UART to the Flipper Zero, which interprets them and renders them on the screen. Supported commands include:
- `CHAR`: `[CHAR/%d/%d/%d]%d", x, y, color, c` (draw a single character at position `(x, y)` with color `color` and character code `c`)
- `TEXT`: `[TEXT/%d/%d/%d]%s", x, y, color, text` (draw a string of text at position `(x, y)` with color `color` and string `text`)
- `CLEAR`: `[CLEAR]` (clear the screen)
- `BLIT`: `[BLIT/%d/%d/%d/%d]<raw RGB332 pixel bytes>", x, y, width, height` (draw a bitmap at position `(x, y)` with dimensions `width` × `height` using raw RGB332 pixel bytes)
- `BLIT1`: `[BLIT1/%d/%d]%n", width, height, offset` (draw a bitmap at position `(x, y)` with dimensions `width` × `height` using raw 1-bit-per-pixel bitmap data)
- `ROW`: `[ROW/%d]<128 raw RGB332 bytes>\n` (draw a row of pixels with width `width` using 128 raw RGB332 bytes)