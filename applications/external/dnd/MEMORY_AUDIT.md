# Memory and performance audit — 4.20.2

This release moves four implementations into FALs and adds a fifth loading FAL. Sheet/Journal standalone wrappers and the Combat parent have smaller host executable proxies; integrated Hub workflows retain the parent and can increase peak RAM. Initiative tools avoid unloading/reloading the whole Bestiary FAP. No device heap, latency or ARM size improvement is certified.

## Comparable host code and constants

Both 4.19.1 and 4.20.2 were compiled with GCC 13.3.0, `-Os -fPIC`, entry-rooted section GC and the same current host shim. Totals sum `.text`, `.rodata` and `.data.rel.ro`; FAP totals include that common shim, while FAL totals contain their own source and a placeholder for generated icons. These are x86_64 ownership proxies, not native FAP/FAL sizes. The historical 4.19.1 text+rodata-only figure used different support objects and is not a directly comparable baseline.

| FAP | 4.19.1 bytes | 4.20.2 bytes | Change |
|---|---:|---:|---:|
| `dndolphins` | 133,394 | 136,658 | +3,264 |
| `dndcharactersheet` | 34,266 | 29,898 | -4,368 |
| `dndcombat` | 118,266 | 111,162 | -7,104 |
| `dndgrants` | 115,626 | 116,858 | +1,232 |
| `dndinventory` | 90,282 | 92,602 | +2,320 |
| `dndspellbook` | 72,722 | 73,970 | +1,248 |
| `dndadventure` | 68,506 | 69,706 | +1,200 |
| `dndjournal` | 37,770 | 17,994 | -19,776 |
| `dndinitiative` | 41,778 | 43,922 | +2,144 |
| `dndbestiary` | 79,882 | 80,826 | +944 |
| `dndbackup` | 52,426 | 53,642 | +1,216 |

The new FAL proxies are separate resident modules when mapped:

| FAL | Host code/constants bytes |
|---|---:|
| `dnd_character_sheet` | 7,339 |
| `dnd_journal` | 23,200 |
| `dnd_monster_turn` | 16,181 |
| `dnd_spell_damage` | 9,293 |
| `dnd_loading` | 2,070 |

Sheet drops 4,368 B, standalone Journal drops 19,776 B and Combat drops 7,104 B in this comparison. Hub adds 3,264 B for integrated dispatch/reload/loading behavior. Every FAP also includes the DND handoff bridge and public Loader barrier. Initiative/Bestiary now include plugin/error/loading bridges; their parent size alone does not include the tools FAL. These results support code ownership decisions, not a claim that every converted workflow loads faster or needs less peak heap.

## Current 32-bit layout proxy

`tests/host/layout32.py` uses a freestanding 32-bit pointer ABI; values exclude native View/dispatcher/timer/ELF allocator objects and dynamic collection storage.

| State/record | Bytes |
|---|---:|
| `DndDolphinsApp` | 3,416 |
| `DndCharacter` | 3,148 |
| `DndCatalogRuntime` | 48 |
| `DndCollectionCacheRuntime` | 64 |
| `DndRollRuntime` | 76 |
| `DndGrantReviewRuntime` | 24 |
| `DndCombatRuntime` | 256 |
| `DndProfileState` | 308 |
| `DndInventoryCollectionApp` | 1,716 |
| `DndSpellbookCollectionApp` | 1,808 |
| `BestiaryApp` | 1,536 |
| `InitiativeApp` | 5,368 |
| `JournalApp` | 1,396 |
| `DndCharacterSheetApp` | 20 |
| `DndMonsterTurnApp` | 208 |
| `DndMonsterDetail` | 1,544 |
| `DndLoading` | 8 |
| `DndLoadingHandoff` | 32 |
| `DndLoadingTransfer` | 148 |
| `DndPlugin` | 8 |
| `DndPluginLoading` | 20 |

Common Hub state remains 3,416 B and Inventory remains 1,716 B. Destination names reuse otherwise idle Catalog storage, adding no fixed Inventory allocation. Combat runtime grows from 248 to 256 B for the lazy module handle. Bestiary state falls from 1,680 to 1,536 B after removing its four inline attack records. The Sheet FAL state is 20 B and borrows a character; its standalone wrapper still owns a separate 3,148 B character allocation. Journal remains a 1,396 B feature state, plus optional entry/search/editor storage. Initiative owns a 1,544 B detail only while a name-resolved tools session is active; Bestiary passes its existing detail by immutable reference.

## Stack reservations and live individual host frames

| Target | Reserved stack bytes | Largest live project host frame bytes |
|---|---:|---:|
| `dndolphins` | 6,144 | 2,192 |
| `dndcharactersheet` | 4,096 | 1,168 |
| `dndcombat` | 6,144 | 1,744 |
| `dndgrants` | 6,144 | 1,744 |
| `dndinventory` | 4,096 | 1,632 |
| `dndspellbook` | 4,096 | 1,168 |
| `dndadventure` | 4,096 | 1,264 |
| `dndjournal` | 4,096 | 272 |
| `dndinitiative` | 6,144 | 1,584 |
| `dndbestiary` | 6,144 | 2,320 |
| `dndbackup` | 4,096 | 1,184 |
| `dnd_character_sheet` | 0 | 304 |
| `dnd_journal` | 0 | 1,280 |
| `dnd_monster_turn` | 0 | 1,600 |
| `dnd_spell_damage` | 0 | 80 |
| `dnd_loading` | 0 | 32 |

These maxima include only project functions whose symbols survive the entry-rooted link; discarded shared storage functions are excluded. Individual frames do not measure cumulative call chains or inlined stack on ARM. A zero FAL reservation means it runs on the caller/GUI thread; it does not mean zero stack use. Initiative is raised from 4 to 6 KiB because its live session now calls the monster query/parser and plugin loader without handing execution to a separately stacked Bestiary FAP. The firmware loader and GUI keep their supplied 2 KiB reservations; both require native high-water measurement. Other FAP reservations are retained.

## Coexistence and device gates

- The Hub remains mapped while integrated Sheet or Journal code runs. Journal can coexist with a large Hub executable and working character; direct standalone wrapper launch is the lower-residency route. Measure peak heap on the intended firmware before treating integration as an OOM improvement.
- Combat holds its damage module through active spell list/cast/result screens and frees it outside those screens or at teardown. Table removal improves non-spell residency; casting still needs the table plus ELF overhead.
- Bestiary tools share parsing/UI with Initiative but can temporarily duplicate monster backend code and assets alongside Bestiary. Borrowing a detail avoids duplicating that record.
- A DND record retains the loading FAL across outgoing teardown/incoming startup. Its 32-byte handoff context and 148-byte transfer proxies exclude native mutex, pubsub, animation, record and ELF allocations. The incoming app normally unsubscribes, drains the timer and frees the module at readiness. Failure/timeout restores drawing but retains one inactive module/context until the next DND readiness/handoff; its timer then performs no drawing. A separately mapped local loading instance can briefly coexist during standalone feature startup. Public Loader barriers separate startup map/free operations without changing SDK state. Host icon placeholders do not measure native compressed assets.
- Measure first/repeat load time, contiguous heap, stack high-water, SD asset extraction, all missing/invalid/OOM paths, Journal reload failure and held-input handoffs on hardware. A source-valid FAL can still fail an ARM relocation or exceed device resources.

Reproduce current sizes with `python3 tests/host/measure_host.py`. For the retained baseline comparison use that same script with `--source-root /path/to/4.19.1 --output-root /absolute/comparison/path`. Full evidence is in `tests/host/host_fap_fal_sizes.json`, `baseline_4191/host_fap_fal_sizes.json`, `stack_frames_host.json` and `layout32.json`. The prior audit is preserved as [MEMORY_AUDIT_4.19.1.md](MEMORY_AUDIT_4.19.1.md).
