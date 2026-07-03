# FlipNote

**A real text editor for Flipper Zero** — bc 5 buttons and a 128×64 screen are more than enough.

> Made to prove the Flipper forum wrong. They said it couldn't be usable.
> It can. :3

---

## Features

- **Open / Save / Save As** — full file browser powered by Momentum's native file picker
- **Virtual buffer** — handles large files without crashing; only loads ~80 lines into RAM at a time, streams the rest from SD card
- **Find** — search across the entire file, navigate matches with Up/Down
- **Find & Replace** — replaces all occurrences, streams through the whole file
- **Copy / Paste line** — clipboard persists while the app is open
- **Delete row** — removes the current line entirely
- **Goto Row** — jump to any line instantly with the number input
- **Horizontal scroll** — Left/Right scrolls all lines together, row numbers stay fixed
- **Scale** — 8 zoom levels from 0.25× to 2.00×
- **Row numbers** — toggleable, always clipped cleanly
- **First Row / Last Row** — instant jump to start or end of file
- **Extended symbols** — `< > : " / \ | ? *` available in keyboard

---

## Controls

### Editor
| Button | Action |
|--------|--------|
| ↑ / ↓ | Move cursor between lines |
| ← / → | Horizontal scroll |
| OK (short) | Insert blank line below cursor |
| OK (long) | Edit current line |
| Back (short) | Open menu |
| Back (long) | Exit app |

### Menu
| Button | Action |
|--------|--------|
| ← / → | Switch tabs (F / E / V) |
| ↑ / ↓ | Navigate items |
| OK | Execute action |
| Back | Close menu |

### Find mode
| Button | Action |
|--------|--------|
| ↑ / ↓ | Previous / next match |
| Back | Exit find mode |

---

## Menu Reference

**F (File)**
- `New` — create a new empty file
- `Open` — browse and open any file from SD card
- `Save` — save (asks for path if file is new)
- `Save As` — type a full path like `/ext/notes/todo.txt`, or just a filename to pick a folder

**E (Edit)**
- `Find` — search, then navigate with ↑↓, Back to exit
- `Find+Replace` — type query → type replacement → replaces all
- `Copy Line` — copy current line to clipboard
- `Paste Line` — paste clipboard below cursor
- `Delete Row` — delete current line
- `Clear All` — clear all line contents
- `Goto Row` — jump to line number

**V (View)**
- `Scale` — change zoom (0.25× – 2.00×) with ←→
- `Row Numbers` — toggle line number display
- `First Row` — jump to beginning
- `Last Row` — jump to end

---

## Installation

### From Release (easiest)
1. Download `flipnote.fap` from [Releases](../../releases/latest)
2. Copy to `/ext/apps/Tools/` on your Flipper's SD card
3. Launch from `Apps → Tools → FlipNote`

### Build from source
```bash
pip install ufbt
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
git clone https://github.com/YOUR_USERNAME/flipnote
cd flipnote
ufbt
```

---

## Known limitations

- Max **2000 lines** per file (index limit)
- Max **128 characters** per line
- Edit operations (insert/delete) only work within the loaded buffer window (~80 lines around cursor)
- Find+Replace rewrites the entire file — works on large files but takes a moment
- Custom keyboard with full symbol support coming soon™

---

## Built with

- Flipper Zero SDK (Momentum firmware)
- `ufbt` — micro Flipper Build Tool

---

## License

MIT — do whatever you want with it :3
