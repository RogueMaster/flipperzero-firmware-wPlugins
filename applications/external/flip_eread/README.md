# eReader for Flipper Zero

Read plain-text (`.txt`) books from the SD card.

- Streams the file from the SD card a page at a time, so large books work fine
- Word wrap, paging forwards and backwards, 5% jumps
- Remembers your place in every book, and reopens on the last book you read
- Two font sizes (small: 7 lines per page, large/bold: 6 lines)
- UTF-8 aware: curly quotes, dashes, ellipses, accented Latin letters, etc. are
  mapped to ASCII the Flipper fonts can draw; CRLF line endings are handled

## Install

```sh
ufbt            # build -> dist/flip_eread.fap
ufbt launch     # build, upload and run on a USB-connected Flipper
```

Or copy `dist/flip_eread.fap` to `SD:/apps/Tools/`.

Put books in `SD:/books/` (created on first launch). The file browser can
open `.txt` files anywhere on the card.

## Controls

| Button            | Action                               |
|-------------------|--------------------------------------|
| Right / Down      | Next page (hold to keep paging)      |
| Left / Up         | Previous page (hold Up to keep paging) |
| Hold Right / Left | Jump forward / back 5% (keep holding to repeat) |
| OK                | Show / hide status bar (title + %)   |
| Hold OK           | Toggle small / large font            |
| Back              | Return to file browser (saves place) |
| Hold Back         | Exit app                             |

The bar on the right edge shows where you are in the book.

## Data

Saved in `SD:/apps_data/flip_eread/`:
- `<hash>.pos`: the saved position for each book (a byte offset)
- `last_book.txt`: the path of the book you read last
- `settings.txt`: your font choice

## License

MIT. See [LICENSE](LICENSE).
