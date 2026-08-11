# Fake Chip Detector

The app itself. Everything about what it does, how to wire a module to it and how to install it
lives in the repository README, one directory up: **[../README.md](../README.md)**.

In this directory:

- **[SUPPORTED_CHIPS.md](SUPPORTED_CHIPS.md)** — every chip in the database, with the register
  and expected value used to identify each one. Generated from `chip_db.c`; do not hand-edit.
- **[LIVE_TESTS.md](LIVE_TESTS.md)** — how a live test works, how to run one from the SD card,
  and how to write your own.
- **[docs/changelog.md](docs/changelog.md)** — what shipped.

Build with `ufbt` from this directory — it is the one that holds `application.fam`.
