# Development

Ham Cram is a C app for the official Flipper Zero firmware, built with
[uFBT](https://pypi.org/project/ufbt/). Python (3.11+, standard library only)
is used for side tooling.

## Build and run

```sh
pip install ufbt
ufbt            # build dist/hamcram.fap against the current Release SDK
ufbt launch     # build, upload to a connected Flipper and start the app
ufbt format     # apply the firmware's clang-format style
ufbt lint       # check formatting, as CI does
```

To try another firmware channel: `ufbt update --channel=rc` (or `dev`), then
`ufbt`. Go back with `ufbt update --channel=release`.

The question pools are shipped as file assets: everything under `files/` is
packed into the `.fap` and unpacked to `apps_assets/hamcram/` on the SD card
when the app is installed or first started.

## Layout

```
application.fam        app manifest (ID, version, category, icon)
hamcram.c / hamcram.h  app shell: allocation, pool list, settings
core/pool.*            reads .hcp pool files from the SD card
core/progress.*        per-question stats and exam history, saved per pool
core/session.*         builds exams, study runs and review lists
views/quiz_view.*      the question screen (wrapping, scrolling, input)
scenes/                one file per screen, using the SceneManager pattern
files/pools/*.hcp      generated pool files (committed; the catalog builds from git)
pools/*.json           the pools as parsed from the NCVEC documents (committed, reviewable)
pools/sources.json     where each pool came from, its dates and checksum
tools/hampool.py       NCVEC .docx -> JSON -> .hcp, plus verification
tools/make_icon.py     writes the 10x10 icon
tests/                 host-side tests (not part of the app build)
catalog/manifest.yml   draft manifest for the Flipper Apps Catalog
```

## Tests

There is no Flipper emulator, so the logic that does not need hardware is
tested on the host. `core/` and `views/quiz_view.c` are compiled unchanged
against a small stand-in for the SDK in `tests/host/shim`:

```sh
python tests/run_host_tests.py
```

This needs a C compiler: `gcc` or `clang` on PATH, or `pip install ziglang`
(the simplest route on Windows). Set `HAMCRAM_SANITIZE=1` to build with
AddressSanitizer and UBSan. The run:

- reads every question of every pool through the C reader and compares each
  field with the pool JSON;
- feeds damaged and truncated pool files to the reader;
- generates thousands of practice exams and checks the one-per-group rule,
  figure filtering and answer shuffling;
- exercises study, review and exam scoring, and save/load of progress,
  including an interrupted save and a pool whose questions changed;
- lays out every question on a simulated screen and walks the cursor all the
  way down and back up, checking that all text is reachable and nothing is
  lost in wrapping.

What the host tests cannot cover is the real device: fonts, the GUI modules
and the SD card. After changing a scene or the question view, run through a
practice exam on a Flipper before releasing.

## Memory

A `.fap` is loaded into RAM, so question text is never compiled in. A pool's
lookup tables (8 bytes per question, about 5 KB for Extra) stay in memory and
question text is read from the SD card one question at a time. See
[pool-format.md](pool-format.md).

## Releasing

1. Bump `fap_version` in `application.fam` and add an entry at the top of
   `docs/changelog.md`.
2. `ufbt lint`, `ufbt`, `python tools/hampool.py verify`,
   `python tests/run_host_tests.py`; test on a device.
3. Commit and push, then follow [catalog/README.md](../catalog/README.md).
