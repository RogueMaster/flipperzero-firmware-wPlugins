# Content packs

Plain-text content for the pack-driven games, one directory per game (`packs/trivia/`,
`packs/wyr/`, `packs/scramble/`, `packs/draw/`, `packs/spectrum/`, `packs/kmk/`).

The format is the same for every game: `Key: value` lines, with a line of `---` or a
blank line between blocks. A `Pack:` key names the pack; without one the filename is
used. The Flipper streams these blocks without interpreting them — each game's meaning
for the keys lives in the ESP firmware.

## Format

The block grammar is generic — the same for every game:

- The first non-empty line of a block **may** be `Pack: <name>` to name the pack.
  If absent, the file name is used.
- Every other line is `Key: value`. Which keys matter, and what they mean, is up to
  the game — the Flipper ships them verbatim without parsing them.
- Blocks are separated by a line containing only `---` (three hyphens) and/or one
  or more blank lines.
- Every line is trimmed of surrounding whitespace before parsing.

### Trivia's keys

Its block is one `Q:` line, exactly four option lines `A:` / `B:` / `C:` / `D:`, and
one `Answer:` line whose value is a single letter `A`-`D` matching a real option:

```
Pack: General Knowledge
Q: What is the capital of France?
A: Paris
B: London
C: Berlin
D: Madrid
Answer: A
---
Q: How many continents are there?
A: 5
B: 6
C: 7
D: 8
Answer: C
---
```

Keep it short so it renders on phones and fits the UART payload budget: question
<= ~110 characters, each option <= ~38 characters. No em-dashes in the content —
use commas or periods (the `---` separator is plain hyphens and is fine).

### Would You Rather's keys

Its block is two lines, `A:` and `B:`, the two options of the poll:

```
Pack: Everyday
A: Be able to fly
B: Be invisible
---
A: Read minds
B: See the future
---
```

Packs are votable in the lobby like trivia topics; keep the pack name short (it
shows in the vote strip on a phone) and each option a short phrase.

### Word Scramble's keys

Its block is one `Word:` line — a single plain word that gets shuffled for players
to unscramble:

```
Pack: Classic
Word: planet
---
Word: guitar
---
```

Packs are votable in the lobby, same as trivia. Keep words lowercase, single
words, and under 20 characters (the engine's word buffer is 24 bytes including
the NUL terminator).

### Draw & Guess's keys

Its block is one `Word:` line — the word one player draws and the rest guess:

```
Pack: Classic
Word: apple
---
Word: house
---
```

Same 20-character guidance as scramble. Unlike the other games, draw has no vote
strip yet — the first pack streamed is the one played.

## Languages

English packs live at the game root (`packs/<game>/*.txt`) and are the default. A
translated set goes in a subdirectory named for the host's language code — the same code
the phone UI uses (e.g. `pt-br`):

```
packs/trivia/geral.txt         # English (default)
packs/trivia/pt-br/geral.txt   # Brazilian Portuguese
```

When the host picks a language in Settings, the Flipper streams that language's
subdirectory for each game, falling back to the English packs **per game** where a
language has none (so a partly translated language still plays). Content is UTF-8, so
accented words are fine — Word Scramble shuffles whole characters (not bytes) and Draw
counts blanks per character. Adding a language is a set of pack files here plus a row in
the Flipper's Settings language picker (and, for the phone UI itself, a catalog in
`web/core/i18n.js`).

## How packs get onto the SD card

Packs live here and are pushed to the Flipper by the deploy script, which copies
every `packs/<game>/*.txt` file to:

```
/ext/apps_data/hotspot_arcade/packs/<game>/<name>.txt
```

Run it with the Flipper connected over USB:

```sh
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```

The Flipper reads packs from that directory tree at runtime. The deploy script only
adds files; packs removed from this folder are not deleted from the SD card
automatically.
