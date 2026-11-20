# fzreader

Book reader for Flipper Zero. Supports EPUB book format with Cyrillic and Latin

## Controls

| Key | Action |
| --- | --- |
| Left / Right | previous / next page |
| Ok | book info, page jump |
| hold Up / Down | increase / decrease the font size |
| Back | pick another book without leaving the app |
| hold Back | save the position and exit |

## Information screen

OK opens a separate screen with the book title, the chapter title (when the
format knows it), the page in the book, the chapter and the controls:

| Key | Action |
| --- | --- |
| Left / Right | one page back / forward |
| hold Left / Right | fast scroll: the step doubles on every repeat, 1, 2, 4, 8, ... up to 64 pages |
| Ok | go to the chosen page and return to the reader |
| Back | return to the reader without moving |
| hold Back | save the position and exit the app |

## State

The reading state is kept in /ext/apps_data/fzreader/state.txt: the path and size of the
last book, the chapter and the character offset of the first line of the page and
the font size. A book with the same path and size is reopened automatically on
start; the saved page is restored. A missing or unreadable file falls back to the
defaults and the start menu.

## Limitations

* EPUB only
* ZIP entries may be stored or deflated; encrypted archives and ZIP64 are not
  supported.
* Fonts cover Cyrillic and Latin. Cyrillic outside U+0400-U+045F and
  U+0490-U+0493 is not covered.
* Faux bold: the run is drawn twice with a one pixel offset.
* Images and tables are not rendered; the text of a table is read as plain text.
  Text from script and style elements is dropped.