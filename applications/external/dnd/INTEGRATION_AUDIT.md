# Dungeons & Dolphins 4.19.1 integration audit

Audit date: 2026-10-04 (America/New_York)

The uploaded latest archive required corrections before its documented refactor could be accepted. The corrected source passes current-mode strict host compilation, all eleven manifest links and entry-point lifecycle tests, and fourteen ASan/UBSan regression executables. This is source and host validation; a fresh RogueMaster ARM build and physical-device acceptance remain outstanding.

## Input and comparison

- Latest input: `Dungeons_and_Dolphins(3).zip`; SHA-256 `4423d1378e5004ef76a3bf1d33b6d8108044ab3260032583c0f8b46eaffccf27`.
- Comparison input: `Dungeons_and_Dolphins(1).zip`.
- The latest input adds eight refactor files. Of 123 changed existing non-Git files, 115 differ only in line endings; eight contain substantive changes. Catalogs, saves and feature data are retained.
- The release remains `4.19.1`; the firmware binary metadata correctly uses numeric `(4, 19)` for all eleven FAPs.

## Corrections

| Finding | Corrected behavior | Evidence |
|---|---|---|
| Duplicate TextInput/NumberInput release declarations | Each helper has one forward declaration; Hub, Combat and Grants compile with `-Wredundant-decls -Werror` | Every active manifest source set, at both `-O1` and `-Os` |
| Ordinary keypress dereferenced a missing Roll runtime | Shared input routing checks runtime availability; Home and Grants remain usable without permanently allocating dice state | `test_lifetimes.c`: Home navigation and Grants review/diagnostics input |
| Profile create/switch/delete assumed a resident collection cache | These paths discard optional cache state safely after clearing its collection owners | New profile, switch, failed-target recovery, replacement deletion and final deletion regression |
| First-use or row-kind switch freed the cache before allocating a page | Language/Proficiency pages reacquire the descriptor after release; failed allocations leave retryable state | Fresh lists, switching differently sized rows, first/second allocation failure and retry |
| Catalog selection read return state after screen teardown | Selection snapshots the return position before leaving Catalog; Language/Proficiency cache invalidation accepts an absent cache | Real Catalog OK, Short Back and Hold Back input |
| Favorite Spells entered without collecting its index | Entry builds the bounded Favorites index and keeps its spell page owned through selection/casting/return | Favorite cast and Back round trip |
| Combat casting-stat navigation reset attack mode | Roll state remains alive across the Combat Magic screen, preserving the current session mode | Combat screen and Magic ownership regression |
| Get Elevated toggle assumed an Item cache existed | Optional Item offsets are invalidated only when that cache exists | Settings toggle from an unused cache state |
| Hub Feat selection could enter a Grants-only scene | The Feature is saved, then dependent review is handed to DNDGrants; Hub contains no reachable grant staging/review roots | Paged Skilled selection, stored Feature and consumer grant check and entry-rooted symbol exclusion |
| Old implementation copies and validation survived the refactor | Retired the `.inc` and three wrappers; compile objects are keyed by source plus actual FAP mode; layout/audit probes read the current core | No legacy implementation remains; all eleven actual-mode builds |

## Feature retention

| Area | Current checked coverage |
|---|---|
| Hub and private graphics | Fifteen Graphical Home assets, Text/Graphical navigation, 2-second splash and argument bypass, Character Sheet/companion launch contracts |
| Combat | All 26 menu actions; Unarmed Strike/Grapple/Shove, legacy ten-slot migration, 300 weapons/spells, casting labels, Utility/Favorite/Ritual routes and Initiative source return contract |
| Grants | SRD/All switching, fixed grant metadata, resumable byte cursor, reviewed choices and Debug-only diagnostics |
| Spellbook | 320 records, bounded sorting/search/filtering, flags and source tags, Magic management and failed-publication rollback |
| Inventory | 320 records, Main/Group/named bags, paired Bag Mover transaction, Homebrew/420 gates, starting equipment and rewards above index 255 |
| Backup and shared storage | SHD v3 bags and older bundle compatibility, exact selected restore, profile duplication/archive, read/write/rename rollback and path-collision preservation |
| Bestiary | Homebrew filtering, Debug projection, Custom Encounter add/repeat/save flow and source/turn-tool handoff contracts |
| Adventure and Journal | Reward preview and milestones, campaign install/retry, progress rollback, note cursor/search and transactional Inventory handoff |
| Shared ownership | Eleven standalone entry points; balanced exercised allocations; draw callbacks checked for heap/storage work; core UI draws also measured for transitive read/allocation activity |

## Reproduce

From the source root:

```sh
python3 tests/host/run_tests.py
python3 tests/host/layout32.py
python3 tests/host/measure_host.py
```

`tests/host/validation_output.txt`, `layout32.json`, `stack_frames_host.json`, and `dndolphins_size_proxy.json` record this run. `validated_sources.sha256` records the checked source/asset inputs.

The regenerated common 32-bit app layout is **3,416 bytes**, with optional Catalog **48**, collection cache **64**, Roll **76**, Grant Review **24**, Combat **248**, and profile browser **308** bytes. These fixes preserve the 26.9% reduction from the previous 4,676-byte common layout.

The same-compiler entry-rooted host executable measures **122,588 bytes text+rodata**, versus **125,124** for the uploaded core, a reduction of **2,536 bytes**. Both use gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0, identical non-core objects/shims and `-Os`. This is a host proxy, not firmware RAM or DFU size.

## Target and device limits

No ARM compiler, matching RogueMaster SDK, uFBT, clang-format executable or physical Flipper is available in this environment. Current target compilation, Loader/FAL implementation compatibility, on-device draw/input acceptance, cumulative stack high-water, free-heap/OOM behavior and actual power-loss durability therefore remain device/build gates. Historical `tests/sdk` evidence does not certify this source revision. Existing stack reservations remain unchanged.

## Applying the files

Place the corrected project contents in `applications/external/dnd/` and build all eleven DND FAPs together with the matching firmware. When applying the delta, delete these retired source files relative to the firmware root:

- `applications/external/dnd/dnd_app_shared.inc`
- `applications/external/dnd/dndolphins.c`
- `applications/external/dnd/dndcombat.c`
- `applications/external/dnd/dndgrants.c`

The full archive already omits those four files. No on-device user data or catalog filename migration is required.
