# Trivia packs

Plain-text question packs for the Hotspot Arcade trivia game. The Flipper parses
this format directly on-device, so it is deliberately trivial: no JSON, no
escaping, one question per block.

## Format

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

Rules:

- The first non-empty line **may** be `Pack: <name>` to name the pack. If absent,
  the file name is used.
- Each question block is:
  - one `Q:` line (the question),
  - exactly four option lines `A:` / `B:` / `C:` / `D:`,
  - one `Answer:` line whose value is a single letter `A`, `B`, `C`, or `D`.
- Blocks are separated by a line containing only `---` (three hyphens) and/or one
  or more blank lines.
- Every line is trimmed of surrounding whitespace before parsing.
- Keep it short so it renders on phones and fits the UART payload budget:
  question <= ~110 characters, each option <= ~38 characters.
- No em-dashes in the content. Use commas or periods. The `---` block separator is
  three plain hyphens on their own line and is fine.

The `Answer:` letter must map to a real option, and each block must have exactly
four options.

## How packs get onto the SD card

Packs live here in `trivia-packs/` and are pushed to the Flipper by the deploy
script, which copies every `*.txt` file to:

```
/ext/apps_data/hotspot_arcade/trivia/<name>.txt
```

Run it with the Flipper connected over USB:

```sh
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```

The Flipper reads the packs from that directory at runtime. The deploy script only
adds files; packs removed from this folder are not deleted from the SD card
automatically.
