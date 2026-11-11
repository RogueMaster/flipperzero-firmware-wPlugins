# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-11

### Added
- Read plain-text (`.txt`) books from the SD card, streamed a page at a time
  so large books don't run out of memory.
- File browser that starts in `SD:/books/` (created on first launch) or on the
  last book you read, and can open `.txt` files anywhere on the card.
- Word wrapping, with very long words split across lines.
- Next/previous page with Right/Down and Left/Up; hold to keep paging.
- Jump forward/back 5% by holding Right/Left.
- Status bar with the book title and percent read (OK to toggle).
- Two font sizes: small (7 lines) and large (6 lines); hold OK to switch.
  The choice is remembered.
- A position bar on the right edge of the screen.
- Saved reading position for each book, stored in `SD:/apps_data/flip_eread/`.
- UTF-8 support: curly quotes, dashes, ellipses and accented Latin letters
  are converted to characters the Flipper fonts can draw; Windows line endings
  and byte-order marks are handled.
