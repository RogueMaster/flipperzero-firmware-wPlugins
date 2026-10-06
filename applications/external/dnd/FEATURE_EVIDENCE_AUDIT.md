# Feature Evidence Audit

Checkpoint: 4.20.3 source/host audit, including retained 4.19.1 corrections
Date: 2026-10-06 UTC

This matrix records current-source evidence for the retained feature history from 4.19 onward. Historical changelog bullets are not treated as proof by themselves; each row points to present implementation and/or regression coverage.

| Feature claim | Current evidence | Validation status |
|---|---|---|
| Scalable Spellbook collections beyond former small ceilings | `dndspellbook_collection.c` uses 16-bit logical indexes/page positions and bounded page windows; host Spellbook regression exercises 320 records | Verified |
| Scalable Inventory collections | `dnd_storage.c`/Inventory paths use streamed visitors and 16-bit counts/indexes; host Inventory regression exercises 320 records and bags | Verified |
| Scalable Feature/Perk collection | `dndolphins_progression_store.c` uses streamed/indexed Feature sidecars; host regression exercises 320 Features | Verified |
| Character Classes default Spellbook filter | `dndspellbook_collection_filter_class_name()` returns `Character Classes` for filter 0 and filter adjustment retains it as default | Verified |
| Any Class and bundled class filters | Spellbook class-mask/filter code in `dndspellbook_collection.c` | Verified |
| Allowed vs All Spells browsing | Spellbook filter UI and eligibility logic in `dndspellbook_collection.c` (`Allowed` / `All Spells`) | Verified |
| Known/knowable/free-granted totals | Spellbook status summary in `dndspellbook_collection.c` | Verified |
| Shared Settings | `dnd_settings.c`; 3.6.8 audit adds transactional temp/backup publication and malformed-line tests | Verified |
| Scalable Languages and Proficiencies | `dnd_character_collections.c` uses streamed 16-bit window/count APIs; host tests exercise 300 records | Verified |
| SHD bundle backup/restore | `dndbackup.c` + `dnd_backup_storage.c` | Verified |
| Exact selected SHD core restore | `dnd_backup_storage_restore_bundle()` and host storage regression | Verified |
| Restore rollback on staging failure | Backup storage transaction logic and failure-injection tests | Verified |
| DNDCombat split FAP | `application.fam` contains standalone `dndcombat` manifest and host link/entrypoint validation | Verified |
| DNDGrants split FAP | `application.fam` contains standalone `dndgrants` manifest and host link/entrypoint validation | Verified |
| DNDBackup & Restore standalone FAP | `application.fam` contains `dndbackup`; UI owns Create SHD Backup/Browse SHD Restore | Verified |
| 11 current FAP manifests | `application.fam` contains 11 app entries; host harness links all 11 | Verified |
| Inventory first-create failure cleanup | `dnd_storage.c`; failure-injection coverage for Main and named bags | Verified |
| Feature first-create failure cleanup | `dndolphins_progression_store.c`; failure-injection coverage | Verified |
| Transactional shared settings publication | `dnd_settings.c`, `dndbackup.c`, `dndbestiary_state.c`; host failure injection | Verified |
| Retry-safe Adventure campaign-pack install | `dndadventure_campaign_packs.c`; failure-injection retry regression | Verified |
| Journal → Inventory handoff is failure-safe | `dndjournal_fal.c` copies/publishes the Inventory sidecar transactionally; host failure injection preserves the prior file and validates retry | Verified |
| Backup/progression copy reads reject I/O failure | `dnd_backup_storage.c` and `dndolphins_progression_store.c` check input file error state before commit; host read-failure injection covers export/copy rollback | Verified |
| Randomized loading artwork with one resident image | `dnd_splash_image.*`, `test_splash_images.c`, `tools/build_splash_assets.py` and `splash_sources/ASSETS.json`: fifteen choices, exact 1-bit encoding, one shared bitmap across mapped/unmapped FAL lifetimes and missing/corrupt/OOM fallback | Verified by source/host gates; rendered timing and native heap remain device gates |
| Draw callbacks avoid direct storage/heap mutation | Host audit currently checks 118 direct static draw helpers | Verified by static host gate; device behavior still requires device validation |

## Checkpoint 8 collision-ownership evidence

- `dnd_storage_create_items_from_assets()` now records whether the first Inventory sidecar was actually opened before cleanup; a failed open caused by a directory/non-file collision leaves that path untouched. Host regression: `tests/host/test_storage.c`.
- `dndadventure_write_milestone_journal()` applies the same ownership rule to generated Journal entries; failed milestone creation cannot delete a path it did not open, and Adventure choice/progress rollback is verified in `tests/host/test_adventure_flow.c`.

## Current shared-core integration evidence

The 2026-10-04 run validates `dnd_app_core.c` under each actual Hub/Combat/Grants build mode; it does not use the retired wrapper/`.inc` implementation. `tests/host/test_lifetimes.c` reproduces real input, first-use, profile, Catalog, Favorites, Feat handoff, Settings and editor transitions. It also injects optional allocation failures and measures draw calls for storage/allocation work. The common layout is regenerated at 3,416 B. See [INTEGRATION_AUDIT.md](INTEGRATION_AUDIT.md) for individual corrections and current target/device limits.

## 4.20.3 evidence

The current retained `tests/host/validation_output.txt` passes eleven actual FAP entry lifecycles, five real dlopen/dlclose FALs and twenty ASan/UBSan executables. Plugin tests cover descriptor/load/size/version failures, allocation cleanup, callback-safe unload, Journal character refresh and failed-refresh save protection, Monster first-use custom data and exact parent return, and Combat resource preservation when its resolver is missing. Inventory tests cover Hold OK, cancel/rollback, immediate per-item Container moves and draw counters for the changed mover/detail paths. Handoff tests run the DND implementation and real loading FAL through a zero-view gap, destination readiness, native event models, timeout, bounded-cache recovery and simultaneous animation/deletion. Two independently mapped loading modules exercise local/handoff overlap while sharing a single bitmap. `test_splash_images.c` covers all fifteen artwork choices, rejection sampling, exact file reads, last-owner cleanup, survival after allocating-module unload, allocation faults and original/text fallback; captures show no reads or allocations during draws. The obsolete firmware-overlay test inputs are removed.

The source import/manifest contract audit matches the supplied API 88.7 and verifies the stock direct-draw, Loader RPC, pubsub and timer contracts; it requires no firmware changes. Normalized host imports are not an ARM relocation certificate. Static draw-helper checks do not establish that every indirect draw path is I/O-free. Current ARM/device gates remain unverified. See `tests/host/VALIDATION.md`, `FAL_INTEGRATION.md` and `DEVICE_TEST_MATRIX.md`.
