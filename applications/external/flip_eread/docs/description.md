# eReader

Read plain-text (`.txt`) books on your Flipper Zero.

## Features

- Streams books from the SD card a page at a time, so large books work fine
- Word wrap, paging forwards and backwards, and 5% jumps
- Remembers your place in every book, and reopens on the last book you read
- Two font sizes: small (7 lines per page) and large (6 lines)
- Bookmarks: mark any page, then list a book's bookmarks and jump to one
- UTF-8 aware: curly quotes, dashes, ellipses and accented Latin letters are
  converted to characters the Flipper fonts can draw

## Getting started

Copy your books to `SD:/apps_data/ereader/books/` (created on first
launch). The file browser can also open `.txt` files anywhere on the card.

## Controls

- **Right / Down**: next page (hold to keep paging)
- **Left / Up**: previous page
- **Hold Right / Left**: jump forward / back 5%
- **OK**: show / hide the status bar (title and percent read)
- **Hold OK**: open the menu to add or remove a bookmark, list bookmarks, or
  switch font size
- **Back**: return to the file browser (your place is saved)
- **Hold Back**: exit the app

In the bookmark list, OK jumps to a bookmark and holding OK deletes it.
