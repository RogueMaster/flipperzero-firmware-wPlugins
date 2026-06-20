# Answer Book

A Flipper Zero app that turns the device into a **book of answers**. You hold a
question in mind, press `OK` to open a page, and the book replies with one short
answer drawn from the world around you.

Under the hood the answer is not a plain `rand()` — it is seeded by the live
radio and sensor state of the Flipper at the moment you ask (see
[How answers are generated](#how-answers-are-generated)). The UI never shows a
signal scanner; it stays a quiet book of answers.

## Experience

The app has three screens:

| Screen | What you see |
| --- | --- |
| **Cover** (idle) | A framed cover: the title `Answer Book`, the closed-book illustration, and a filled `Press OK` button. |
| **Opening** | A short page-turn animation while the book "reads" the environment. |
| **Page** (result) | A framed archive page: a page number `No.NNN`, a rarity sigil (`✦ Rare` / `✦ Legendary`), the answer as the hero line, and the answer's category engraved at the foot. |

Press `OK` again on a page to open another. Press `Back` to close the book.

Each answer belongs to one of five categories — **Forward, Observe, Warning,
Explore, Reflection** — and carries a rarity of **Normal**, **Rare**, or
**Legendary** (Legendary pages reveal a special answer).

## How answers are generated

Every page open runs one short, read-only sensing pass and folds it into a
32-bit FNV-1a hash that seeds the answer:

- **BLE** — RSSI sampling across advertising channels 37/38/39
  (skipped if Bluetooth is already active).
- **SubGHz** — RSSI / LQI / pipe activity on common regional bands
  (315, 433.92, 868.35, 915 MHz). **Receive only — the app never transmits.**
- **NFC** — a brief scan for a present tag and its protocol mask.
- **Device state** — RTC time, uptime, battery percent, battery temperature.
- **Hardware RNG** — `furi_hal_random_get()` samples.

These are reduced to a five-axis vector (`entropy`, `activity`, `stability`,
`novelty`, `silence`) that biases a weighted category choice; the hash then
selects the specific answer and the rarity roll. Because the environment is
never identical twice, repeated questions rarely give identical pages.

## Saved records

Each opened page is appended as one JSON line to the app's data log:

```
/ext/apps_data/answer_book/answers.log
```

A record stores the timestamp, the raw BLE/SubGHz/NFC scores, the five-axis
vector, and the chosen `answer` / `category` / `rarity` / `seed` — a collectible
archive for future codex/history features. Saving is silent; the page UI shows
no save status.

## Build & flash

This is a [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) project.

```sh
ufbt              # build -> dist/answer_book.fap
ufbt launch       # build, upload to a connected Flipper, and run
```

If the Homebrew `ufbt` launcher points at a missing Python binary, invoke it
through the module instead:

```sh
PYTHONPATH=/opt/homebrew/lib/python3.11/site-packages python3 -m ufbt
```

The installed app appears under **Apps → Tools → Answer Book**.

## Project layout

| Path | Purpose |
| --- | --- |
| `answer_book.c` | The whole app: oracle engine, view, and rendering. |
| `application.fam` | uFBT app manifest (`appid: answer_book`). |
| `answer_book.png` | 10×10 launcher icon. |
| `images/book_closed.png` | Idle cover illustration (48×30). |
| `images/book_flip_0..6.png` | Page-turn animation frames (48×30). |
| `images/book_turn/` | The same frames as a uFBT `IconAnimation` bundle (reserved for future use). |
| `changelog.md` | Version history (used by the Flipper App Catalog). |
| `screenshots/` | qFlipper screenshots used in the App Catalog listing. |

## Requirements

Runs on a stock **Flipper Zero** — no extra hardware, GPIO modules, or
expansion boards required. Uses only the built-in radios and sensors, all in
receive/sample mode.

## Notes

- The app does **not** transmit on SubGHz; all radio use is receive/sample only.
- BLE sampling is skipped while Bluetooth is active to avoid disrupting it.
- NFC scanning is short-lived and only runs while a page is opening.

## License

[MIT](LICENSE) © redbson
