# tools

Python dev bridge (Phase 0 spike, kept as a debugging tool) and fixture
generators. Run the tests with `uv run pytest`.

Fixture generators write into `protocol/fixtures/`; run them from this
directory, tag dumps first:

```
uv run python -m fixtures.make_bambu_dump
uv run python -m fixtures.make_ntag_dump
uv run python -m fixtures.make_message_fixtures
```

`uv run python -m fixtures.make_key_vectors` writes the crypto known-answer
vectors (SHA-256, HMAC RFC 4231, HKDF RFC 5869, Bambu keys for two UIDs) to
`flipper/tests/vectors/bambu_keys.h`; `tests/test_bambu_keys.py` checks the
same tables and fails when the header is stale.

`uv run python -m fixtures.make_tag_vectors` (after the generators above)
writes the tag dump fixtures as C arrays to `flipper/tests/vectors/tag_dumps.h`
for the C `scan` encoder test; `tests/test_tag_vectors.py` fails when it is
stale.

`uv run python -m fixtures.make_flipper_icons` writes the Flipper app's XBM
icons to `flipper/images/` from the icon table of
`docs/design/source/flipper-ui.js` (`flipper/src/ui_icons.c` includes them).
Edit the table and regenerate; do not edit the `.xbm` files. It also
encodes the Get app screen's QR code from `GET_APP_URL` with `segno`
(version 3, error correction L, mask pinned) into `qr_get_app.xbm` at 2 px
a module and into the table's `qr` line, so the design shows the same
code. `tests/test_flipper_icons.py` fails when a file is stale and reads
the QR bitmap back with `zxing-cpp` (both are dev dependencies); a URL that
no longer fits version 3 fails the generator.

`uv run python -m fixtures.make_app_icons` (or `make icons`) writes the app
icons from the formulas of `docs/design/source/FliBuddy App Icon.dc.html`:
the SVGs in `docs/design/icons/`, the 1024 px PNGs of the iOS and Watch asset
catalogs (`host/Apple/*/Assets.xcassets`) and the Flipper's 10x10 1-bit
`flipper/icons/flibuddy_10px.png`. It rasterises with `rsvg-convert`
(`brew install librsvg`); the PNGs are committed, so building the apps does
not need it. `tests/test_app_icons.py` fails when a file is stale (skipped
without `rsvg-convert`).

`uv run python -m fixtures.make_color_codes` writes Bambu's colour codes and
names from `data/bambu_color_codes.json` (refresh it from Bambu Studio with
`--source`) to the host's `BambuColorCodes.swift` and, packed, to the
Flipper's `flipper/src/bambu_color_table.h` (a 12-byte row each, names stored
once, about 6 KB), which names a tag's colour without the app.
`tests/test_color_codes.py` fails when either is stale and unpacks the C
table back to the rows.

`tests/test_fixtures.py` fails when a committed synthetic fixture differs
from what its generator produces.

## Simulator screenshots

`sim_shot.sh` screenshots the iOS, iPadOS or watchOS app in its demo mode
(`-FliBuddyDemo YES`) on a simulator, after `make apple-project
apple-build`; run it without arguments for its usage. `store_shots.sh [dir]`
takes the App Store set with it. Both are described in "Demo mode and
simulator screenshots" and "App Store packaging" of `host/README.md`.

## Dev bridge

`bridge/` connects the Flipper app to Bambuddy over USB serial, for the
Phase 0 spike and for debugging the protocol without the Mac host:

```
uv run python -m bridge --bambuddy http://<BAMBUDDY_LAN_IP>:8000 --api-key bb_...
uv run python -m bridge --dump-dir ../protocol/fixtures/tags   # no Bambuddy: print and save scans
```

- `--port` defaults to the first `/dev/cu.usbmodemflip_*`; the API key can
  come from `$BAMBUDDY_API_KEY`.
- At start-up the bridge reads `/settings/spoolman` and refuses Spoolman
  mode (the key needs `settings:read`).
- It speaks protocol version 2: it answers `hello`, prints each parsed tag
  and the `lookup` it sends, and performs the `weigh`, `new`, `full`,
  `empty` and `delete` intents (`--core-weight`, default 242 g = Bambu
  reusable spool 215 g + refill core 27 g, and `--catalog-id` set the core
  weight and `core_weight_catalog_id` of new items); `create_on_host` /
  `link_on_host` are only logged. A repeated intent id gets the stored `result`; failed calls are
  not stored, so a retry runs again.
- A scan with `autosave: true` creates an unknown tag that carries a
  material at once (`created: true`); otherwise nothing is created until
  `new` or a `weigh` on the unknown tag, which creates the spool and then
  records the weight (looked up again first, so a retry never creates a
  second one). Tags are looked up with `include_archived=true`: a spool
  marked empty is `found` with `spool.archived`, and a weigh or Mark as new
  restores it first. The bridge cannot push a fresh `lookup` after a host
  action: it has no feed, so New / Link happen in Bambuddy itself.
- `--dump-dir` saves every scan in the `protocol/fixtures/tags` format.
  MIFARE Classic sector trailers are rebuilt from the derived Key A (the
  Flipper does not send them). `protocol/fixtures/tags/bambu_pla_pure_real.json`
  was captured this way in Phase 0 (spool "PLA Pure", UID `7EF1AB04`);
  sectors 3-15 did not read and are zero-filled and listed in `missing`.
  Real dumps are marked `"synthetic": false`, have no generator and are
  checked for shape only by `tests/test_fixtures.py`.

Modules: `bambu_tag.py` (pure tag parser, mirrored by the Swift one; the
tray UUID is block 9, which matched the AMS-reported `tray_uuid` in Phase 0;
Bambu UIDs go to Bambuddy as UID + `00000100`, the AMS form),
`bambuddy.py` (httpx client), `session.py` (framing and message handling),
`__main__.py` (CLI and serial loop).

## Core weight fix

FliBuddy before 1.0 created spools with a 325 g core (empty spool); the
default is now 242 g. `fix_core_weight.py` sets the new core on every spool,
archived ones included, that still has the old one. A weighed spool that is
not archived or empty also gets its remaining back: `weight_used` drops by
the difference, as if the last weighing had used the new core.
`weight_locked` is sent unchanged. It lists the changes and writes nothing
unless given `--apply`:

```
BAMBUDDY_URL=https://bambuddy.example.com BAMBUDDY_API_KEY=... \
    uv run python fix_core_weight.py [--old 325] [--new 242] [--apply]
```

The key needs inventory read and update; it is read from the environment
and never printed.

## Lint and coverage

`uv run ruff check` lints the tools; `uv run pytest --cov` fails under 80 %
line coverage (`[tool.coverage]` in `pyproject.toml`). `swift_coverage.py`
applies the same gate to the Swift package: `make coverage-host` at the
repository root runs `swift test --enable-code-coverage` and passes the
report to it.
