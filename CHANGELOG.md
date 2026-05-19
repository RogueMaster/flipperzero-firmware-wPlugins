# Changelog

All notable changes to **I2C Tools CLI** are documented here. Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow MAJOR.MINOR.

## Version 1.0 — 2026-05-14

First release prepared for the Flipper Application Catalog.

**Added** (fork-specific, on top of upstream NaejEL/flipperzero-i2ctools)
- **N-byte read** in the sender view (1..32 bytes) — Long Left / Long Right adjust *Len* when the sender is in READ mode.
- **Write mode** toggle on the sender view — Long OK switches the sender between READ and WRITE; in WRITE mode, Long Left / Long Right adjust the byte value to send.
- **HEX / ASCII** display toggle on read results — Long Back flips the format; once a result is shown, Long Up / Long Down page through the buffer.
- **Serial CLI** registered as the *i2c* command (parallel-safe), with subcommands **scan**, **probe**, **read**, **write**, **help**.
  - *i2c read* accepts an optional **hex** (default) or **ascii** format suffix.
  - All arguments parsed as hex with optional **0x** prefix.
- New 10×10 app icon featuring **"I2C"** above a **">_"** prompt to convey the dual GUI + CLI nature.


**Changed**
- *stack_size* raised from 2 KiB to 4 KiB (the CLI callback uses a 256-byte buffer on its stack).
- App description and metadata updated for catalog submission.

**Fixed**
- Calls to the renamed *CliRegistry* API (current SDK) instead of the legacy *Cli* API.

**Upstream attribution** — original GUI, scanner, sender, sniffer and infos views: © 2023 NaejEL — see [LICENSE](LICENSE).
