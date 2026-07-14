# CFS Tag Writer

External Flipper Zero app (FAP, C) that reads, writes, clones, and erases
Creality CFS filament-spool NFC tags — MIFARE Classic 1K with an AES-128
encrypted payload.

## Tech Stack
- C, against the Flipper firmware SDK via uFBT (installed in `.venv`)
- UI: firmware `scene_manager` + `view_dispatcher` (submenu, widget, popup,
  byte/text/number input)
- mbedtls AES-128 for the payload cipher, linked through `fap_libs` in
  `application.fam`

## Development Commands
- Build the `.fap`: `source .venv/bin/activate && ufbt`
- Build, install, and run on a connected Flipper: `ufbt launch`
- No host-side test suite. Verify on hardware with `ufbt launch` plus the in-app
  Settings > Self-Test scene (crypto + data round-trip). RPC screen-capture
  tooling exists but is maintainer-local, not in the repo.

## Structure
All app source is under `src/`. Includes are root-relative (e.g.
`#include "src/data/cfs_data.h"`) because uFBT puts the project root on the
include path for external apps — this is not an `application.fam` setting.
- `src/core` — shared `CfsApp` state struct (`cfs_app_i.h`), scene-manager glue
  (`cfs_scene.c/h`), app entry point (`cfs_tag_writer.c`)
- `src/data` — tag field encode/decode + the filament catalog
- `src/crypto` — AES key derivation + cipher
- `src/worker` — async NFC thread
- `src/storage` — `.nfc` file save/load
- `src/settings` — persisted app settings
- `src/scenes` — one file per screen in cosmetic workflow subfolders
  (`menu/ read/ write/ erase/ saved/ settings/ help/`). Register each scene in
  `src/scenes/scene_config.h` (X-macro list at `src/scenes/` root);
  `src/core/cfs_scene.{c,h}` generate the enum + handler tables from it, so a
  new scene needs a line there, not just its `.c` file.
- `.catalog/` — separate description + qFlipper screenshots for the Flipper Apps
  Catalog (see Gotchas). The `manifest.yml` lives in a separate
  `flipper-application-catalog` fork, not here.

## Critical Conventions
- `CfsApp` (`cfs_app_i.h`) is the single mutable state struct threaded through
  every scene. Its write-flow staging fields (`pending`, `write_exact`,
  `dual_tag`, `has_source_serial`, `pending_weight_index`) are never reset
  automatically — every entry point into the write wizard must set all of them,
  or a missed assignment silently reuses stale state (this has caused real bugs).
- Filament identity is catalog-driven: `cfs_data.c`'s `cfs_catalog[]` (keyed by
  the 5-char id) is the source of truth for brand/name/material on read and
  write. The older `CfsFilamentType` enum is a read-side legacy fallback only —
  don't route new write logic through it.
- The AES keys / MIFARE access bits in `cfs_crypto.c` are copied verbatim from
  public Creality-ecosystem reference material, not a secret this app protects.

## Known Issues / Gotchas
- `application.fam` restricts `sources` to `"*.c"` (not the default glob) so
  stray files like `*.code-workspace` aren't fed to the linker.
- The tag's date and batch fields are opaque/fixed by Creality firmware (see
  `cfs_data.h`) — not a real calendar date; don't try to "fix" their encoding.
- The CFS tag-format spec (byte offsets/keys) is third-party reference material,
  not this project's work — it is not in the tree or git history, so don't
  expect it or suggest re-adding it.
- `.catalog/` is a deliberate separate copy of the description/screenshots: the
  catalog's `description` field renders only limited markdown (no images, tables,
  or code) and its screenshots must be qFlipper captures, not the upscaled RPC
  captures used in `README.md`/`GUIDE.md`.
