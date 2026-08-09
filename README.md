# Internet Time Screen

External Flipper Zero app (`.fap`) that shows Swatch Internet Time (`@000`–`@999`)
alongside local time. Stock-firmware compatible; no network or firmware fork.

## Requirements

- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) on the official
  **release** channel (local builds use SDK **1.4.3** / API **87.1**)
- Flipper Zero with microSD card, USB data cable
- C11 host compiler (`cc`) for unit tests

## Build

```bash
ufbt lint
ufbt
```

The FAP lands in `dist/`.

## Host tests

```bash
./tests/run_host_tests.sh
```

(Available after Stage 1 time-core modules land.)

## Launch on device

```bash
ufbt launch
```

OK opens settings; Back exits. Configure the current UTC offset (15-minute
steps) so Biel Mean Time can be derived from the Flipper RTC.

## Note on firmware

Development acceptance may run on API-compatible custom firmware (e.g.
RogueMaster) when API major/minor matches the release SDK. Catalog-oriented
builds still target the official release SDK.
