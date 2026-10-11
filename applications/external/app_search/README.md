# App Search

One list for every app on your Flipper Zero — installed `.fap`s **and** the
built-in tools — with fuzzy search and most-used-first ranking. Find and launch
anything without digging through the category menus.

![App Search](screenshots/ss0.png)

## Features

- **Everything in one list.** Scans `/ext/apps` (including sub-folders) for every
  installed app and adds the built-in tools: Sub-GHz, NFC, Infrared, 125 kHz RFID,
  iButton, GPIO, Bad USB and U2F.
- **Real names.** Each app's display name is read from its own manifest on the
  device (the `.fapmeta` section), not guessed from the filename.
- **Fuzzy search.** Press **Left** to open the keyboard and type; the list filters
  by a subsequence match with word-boundary bonuses.
- **Most-used first.** Every launch is counted and remembered, so your favorites
  rise to the top (and are what you see with an empty query).
- **Fast.** Scanned names are cached (`/ext/apps_data/appsearch/names.cache`), so
  after the first build the list is ready in well under a second; only new or
  changed apps are re-read.
- **Comes back.** When the app you launched exits, App Search reopens.

## Controls

| Key | Action |
|-----|--------|
| Up / Down | Move the selection |
| OK | Launch the selected app |
| Left | Open the search keyboard |
| Back | Return to the list (from search) / exit (from the list) |

## Build & install

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt            # build app_search.fap into dist/
ufbt launch     # build, upload to a connected Flipper and run
```

Or copy `dist/appsearch.fap` to `/ext/apps/Tools/` on the SD card.

The app targets the Official Firmware API and also builds on Unleashed; CI builds
both.

## Notes

- Reading an app's manifest via the firmware's loader API deadlocks when called
  from inside a running app, so App Search parses the `.fapmeta` ELF section
  directly with plain file reads.
- Usage counts live in `/ext/apps_data/appsearch/usage.txt`; the name cache lives
  in `/ext/apps_data/appsearch/names.cache`. Both are plain text and safe to delete.

## License

MIT — see [LICENSE](LICENSE).
