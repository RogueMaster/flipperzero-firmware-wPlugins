# Hanzi Cards

Mandarin flashcards for the [Flipper Zero](https://flipperzero.one): simplified or traditional characters, pinyin with tone marks, and the tones played on the speaker.

![The back of the card for 公共汽车](screenshot.png)

![服務員 in traditional characters](screenshot_traditional.png)

## What it does

- **Decks:** HSK 1 (150 words) and HSK 2 (156 words), each in a rough learning order with its own progress.
- **Characters:** simplified or traditional, switchable at any time.
- **Cards:** the front shows the characters. Flip it to see the pinyin and the English.
- **Tone sound:** on a flip, the speaker glides through each syllable's tone contour (high and flat, rising, dipping, falling), so you hear the shape of the word.
- **Spaced repetition:** cards you miss come back often and cards you know come back rarely. New cards are only introduced while fewer than six are still being learned.
- **Saving:** progress is saved to the SD card.

## Controls

| Button | Action |
| --- | --- |
| OK | Flip the card; once flipped, play the tones again |
| Left | Again: you missed it |
| Right | Good: you knew it |
| Back | Flip the card back over |
| Hold OK | Settings |
| Hold Back | Save and quit |

## Settings

| Setting | Values |
| --- | --- |
| Deck | HSK 1 or HSK 2 |
| Characters | Simplified or Traditional |
| Front | Hanzi or English |
| Tone sound | On or Off |
| Reset progress | Press twice to clear the current deck's progress |

## Adding words

Decks are built from the tab-separated lists in `decks/` (simplified, traditional, numbered pinyin, English). Edit a list and rebuild its deck file:

```bash
python tools/build_deck.py decks/hsk1.tsv assets/hsk1.deck
```

The script needs [Pillow](https://python-pillow.org) and the fonts `NotoSansCJKsc-Regular.otf` and `NotoSansCJKtc-Regular.otf` from [noto-cjk](https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF) in a `fonts/` folder (or pass `--font` and `--trad-font`). A new deck also needs an entry in the `decks` table in `hanzi_cards.c`.

## Build and install

You need [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), the Flipper app build tool:

```bash
python3 -m pip install --upgrade ufbt
```

With the Flipper connected over USB (and qFlipper closed), build, install and start the app:

```bash
ufbt launch
```

It installs to `Apps → Tools → Hanzi Cards`.

## License

[MIT](LICENSE). The character bitmaps in `assets/` are rendered from Noto Sans CJK, which is licensed under the [SIL Open Font License](https://openfontlicense.org).
