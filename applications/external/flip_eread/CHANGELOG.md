# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-11-16

### Added
- Bookmarks. Hold OK while reading to open a menu where you can bookmark the
  current page (or remove its bookmark) and list all bookmarks for the book.
  The list shows each bookmark's percentage and first line of text; OK jumps
  to it and holding OK deletes it. Up to 256 bookmarks per book, stored in
  `SD:/apps_data/flip_eread/<hash>.bmk`.
- Bookmark ticks beside the position bar, and a ribbon in the top-right
  corner when the current page is bookmarked.

### Changed
- Hold OK now opens the menu; the font size switch has moved into it.

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
