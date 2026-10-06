# Dungeons & Dolphins 4.20.2 audit status

Audit date: 2026-10-06 UTC. All sixteen manifests use release label `4.20.2` and
numeric `(4, 20)` metadata: eleven FAPs and five non-embedded FALs. Descriptor IDs
and plugin API versions are unchanged. This release is part of the cumulative
Game Menu loading and DND collection source delta.

## Current changes

- Spellbook attaches an immutable loading view before deferred collection scanning
  and sorting. Its existing eight-record page cache and offset indexes remain.
- Every sort invocation validates the current file. Ordered data takes one scan
  without rewriting. A single displaced record uses linear reinsertion; bulk
  disorder uses 24-record runs and bounded merge passes over temporary indexes.
- Name/level edits and additions request reordering before returning to the list or
  leaving the app. Successful reordering invalidates pagination/search offsets.
  Failed saves retain the in-memory edit; failed sort/read operations block editing
  until recovery/reload, with a safe exit when no unsaved edit remains.
- Spellbook consumers recover `.sort.bak` before reading or recreating a collection.
  Other apps use the small recovery helper without linking the full sorter.
- Inventory moves sync both private staged bag files before publishing a checked
  per-profile transaction journal. Recovery completes both files and removes the
  journal last. Shared storage and Journal guard collection access and profile
  lifecycle operations. Pending/recovered transfers discard stale UI selections.
- Eight shared-storage FAP source sets and the Journal FAL link the transaction
  helper. Spellbook alone links the full sorter. The native manifest parser accepts
  all sixteen source sets.

## Current host verification

Run these commands from the firmware root:

```sh
ASAN_OPTIONS=detect_leaks=0 python3 scripts/tests/dnd_inventory_transaction/run.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/tests/dnd_inventory_ui/run.py
ASAN_OPTIONS=detect_leaks=0 python3 scripts/tests/dnd_spellbook_sort/run.py
python3 applications/external/dnd/tests/spellbook_sort/run.py
```

| Gate | Current result |
| --- | --- |
| Actual transaction helper with ASan/UBSan | 607 invariants passed, including reset/failure boundaries, journal truncation/corruption and FAT path aliases |
| Extracted Inventory UI helpers | Both move callers handle all four results; stale-index preflight, failed reload, pending input and profile-zero startup cases passed |
| Actual Spellbook sorter with ASan/UBSan | 444 byte-preservation checks passed; 768 records used five source scans and five merge passes; host workspace 3,000 B |
| Sort fault injection | 121 reset boundaries, 121 mutation failures, 179 read failures, ten File allocation failures and workspace allocation failure passed |
| Actual sorter with POSIX adapter and independent bounded oracle | 145 cases / 130 fault cases passed; 100, 300 and 1,000 records each used five source scans and at most four File handles |
| Full shared-storage translation unit | Recovered indexed spell paging/append, paired item/container movement, corrupt-journal write/delete guards and pre-write aliases passed |
| Extracted Spellbook UI helpers | Cross-page search lifetime, index invalidation, failed save/sort/read, retry/safe exit and deferred startup ordering passed |
| Native manifest parser | All sixteen DND manifests accepted; eleven FAPs and five FALs at release 4.20.2 |

C tests use host Storage/GUI stubs and warnings treated as errors. Reset injection
occurs between completed storage API operations; it does not emulate torn FAT
sectors, SD-controller caching, GUI scheduling or the ARM ABI. Explicit ownership
counters supplement ASan/UBSan because LeakSanitizer is unavailable here.

The 4.20.1 memory, complete-suite compilation and loading/handoff reports retained
in this folder are historical. Their whole-suite results have not been recertified
with the new helpers and UI changes. See the current combined report at
[`documentation/cumulative_loading_validation.json`](../../../documentation/cumulative_loading_validation.json).

## Installation and remaining checks

Build the matching firmware and SD resources for the cumulative Game Menu changes.
Rebuild `game_menu.fal` and CFW Settings for the shared discovery-sort change. Build
and install all eleven matching DND FAPs and the eight FAL destinations listed in
[FAL_INTEGRATION.md](FAL_INTEGRATION.md). Loading API version 2 still belongs at
`/ext/apps_data/dndolphins/plugins/dnd_loading.fal`.

No complete ARM toolchain/build or connected Flipper was available for this delta.
Native links/relocations, actual FAL descriptor objects, flash headroom, startup
timing, GUI scheduling, heap/stack high-water and real reset recovery remain device
acceptance gates. Use [DEVICE_TEST_MATRIX.md](DEVICE_TEST_MATRIX.md). The source
delta contains no newly compiled FAP/FAL binaries or firmware image.
