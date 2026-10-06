# Dungeons & Dolphins 4.20.3 FAL integration

This release retains eleven launchable FAPs and moves four approved feature implementations into versioned, non-embedded FALs. A fifth FAL draws one randomly selected DND splash from fifteen choices with an animated SDK hourglass. Build the whole suite together; older FAPs do not implement these contracts.

| FAL | Used by | Lifetime and effect |
|---|---|---|
| `dnd_character_sheet.fal` | Hub, standalone Character Sheet wrapper | Read-only ten-page sheet; borrows the parent's character and dispatcher; unloaded on return |
| `dnd_journal.fal` | Hub, standalone Journal wrapper | Owns Journal UI/persistence; borrows dispatcher and Storage; Hub reloads the canonical character before resuming |
| `dnd_monster_turn.fal` | Initiative, Bestiary | Focused stat block/attack tools; keeps the parent encounter, round and selection resident; no full Bestiary FAP launch from Initiative |
| `dnd_spell_damage.fal` | Combat | Lazy spell damage resolver/table; released outside spell workflows; validated before spending casting resources |
| `dnd_loading.fal` | All DND FAP handoffs and local UI loading bridges | DND-owned splash/hourglass; retained across outgoing FAP teardown and incoming startup |

## Build and installation

Place the full source directory under the matching firmware tree's `applications/external/dnd/`. Use the supplied firmware's toolchain and API. From the firmware root, build all sixteen targets:

```sh
./fbt fap_dndolphins fap_dndcharactersheet fap_dndcombat fap_dndgrants fap_dndinventory fap_dndspellbook fap_dndadventure fap_dndjournal fap_dndinitiative fap_dndbestiary fap_dndbackup fap_dnd_character_sheet fap_dnd_journal fap_dnd_monster_turn fap_dnd_spell_damage fap_dnd_loading
```

FBT's `fap_` target prefix applies to plugins too; their generated files end in `.fal`. Deploy the eleven FAPs to `/ext/apps/Games/`. Install the five FAL files at all eight destinations below. Parent duplication is intentional: the native non-embedded plugin deployment rule uses each declared parent's `apps_data/<parent>/plugins/` directory. Every DND app uses the one loading FAL installed under DNDolphins.

| File | Required destination |
|---|---|
| `dnd_character_sheet.fal` | `/ext/apps_data/dndolphins/plugins/dnd_character_sheet.fal` |
| `dnd_character_sheet.fal` | `/ext/apps_data/dndcharactersheet/plugins/dnd_character_sheet.fal` |
| `dnd_journal.fal` | `/ext/apps_data/dndolphins/plugins/dnd_journal.fal` |
| `dnd_journal.fal` | `/ext/apps_data/dndjournal/plugins/dnd_journal.fal` |
| `dnd_monster_turn.fal` | `/ext/apps_data/dndinitiative/plugins/dnd_monster_turn.fal` |
| `dnd_monster_turn.fal` | `/ext/apps_data/dndbestiary/plugins/dnd_monster_turn.fal` |
| `dnd_spell_damage.fal` | `/ext/apps_data/dndcombat/plugins/dnd_spell_damage.fal` |
| `dnd_loading.fal` | `/ext/apps_data/dndolphins/plugins/dnd_loading.fal` |

The Hub and loading FAL each bundle all fifteen raw bitmaps as file assets. Native preload extracts them to `/ext/apps_assets/dndolphins/loading/` and `/ext/apps_assets/dnd_loading/splashes/`. Both copies permit first use independently of the other app. The `.fapassets` section is nonresident; the supplied unpacker streams files with a 512-byte copy buffer. First extraction/update writes all asset files to SD, while normal image acquisition reads only the selected 1,024-byte file. Do not copy the source PNGs onto the device manually.

The Monster Turn FAL bundles its own monster TXT assets. Native preload unpacks them under `/ext/apps_assets/dnd_monster_turn/`, independently of whether the Bestiary FAP has run. Custom monsters, enabled packs and legacy migration remain under the existing `/ext/apps_data/dndbestiary/` contract. No character or collection schema migration is needed.

All source changes are contained in `applications/external/dnd/`. Build and deploy the matching DND FAPs/FALs against the supplied stock firmware API; no firmware overlay, service modification, SDK patch or firmware flash is required by this release. The obsolete overlay source directory is removed. The loading descriptor is API version 2, so install the new loading FAL together with all eleven FAPs.

## UI and ownership

Hub Character Sheet and Journal now run inside the Hub's dispatcher. The standalone FAPs remain thin wrappers for direct launch. Parent callbacks/timers/editors are quiesced before borrowing the dispatcher; the FAL unregisters its views and clears its callback bindings before return. The parent restores its callbacks afterwards. Explicit `loading_on_return` tells a UI FAL whether private loading view `0xDFFE` is registered; there is no dependency on the supplied firmware's misleading `view_dispatcher_check_id()` header prose. Loading and error views use reserved IDs, separate from feature views.

Each descriptor has a unique ID and a size-prefixed API. The four feature descriptors use API version 1; loading uses version 2. Load failure, wrong ID/version/size, missing function or UI allocation failure cleans up partial ownership and gives the parent an error/return path. Borrowed data is never freed by the FAL. Views, editor callbacks, timers and animation callbacks must finish using module code before the ELF is unmapped. Host tests inspect live callback addresses before every real shared-module unload.

Journal can update the canonical character through milestones and append Inventory records. The Hub discards its old collection caches and reloads that exact profile before resuming. A failed reload marks it unavailable/read-only, preventing stale/default data from being saved. Adventure continuation remains a normal FAP handoff after the Journal FAL has returned and unloaded.

Combat loads and validates the resolver before classification/casting needs it. Missing or incompatible damage code cannot consume spell slots, Pact slots, free casts or points. Favorite and Ritual indexing can remain independent of damage classification; actual casting requires the FAL.

## Loading behavior

All eleven FAPs prepare the shared loading FAL before queuing a destination and before detaching their GUI. `dnd_app_handoff.c` retains the mapped module, service references and a copied destination path in the DND-owned record `dnd_loading_handoff_v2`. All surviving animation/event callbacks belong to that FAL; the selected bitmap is immutable shared heap data. Neither surviving callbacks nor bitmap data point into the outgoing FAP.

The FAL uses the stock public `gui_direct_draw_acquire()` API. In the supplied firmware it suppresses ordinary GUI drawing but leaves the GUI mutex unlocked, allowing the next app to attach its normal views while the splash/hourglass stays visible. After activating its view, the incoming DND app calls `dnd_handoff_ready()` with its own FAP path. Only a matching destination dismisses an active handoff; stale parent readiness cannot do so. Cleanup runs from the app thread: unsubscribe from Loader events, stop/free and drain the animation, release direct drawing, then unmap the FAL and destroy the retained record.

The Hub attaches its own splash early and activates Home before removing it. Normal launch has the existing two-second minimum; return/deep-link launches use the same loading artwork without that minimum. Local Sheet/Journal/Monster UI loads and returns borrow the loading FAL on the same dispatcher. The hourglass uses SDK animation. Startup, local loading and handoff instances share one selected bitmap while their lifetimes overlap; only the last owner releases it. The original and fourteen supplied PNGs are preserved without editing. See [SPLASH_LOADING.md](SPLASH_LOADING.md).

Loader load failure, an empty launch queue, incoming app exit before readiness and a ten-second animation timeout restore normal drawing. These callbacks retain one bounded inactive module/context until the next DND app reaches readiness or prepares another handoff; they never unmap their own executing code. Its timer callbacks then do no drawing. Normal readiness frees the handoff context/module immediately; another active loading owner can retain the same bitmap until its own cleanup. Missing/incompatible loading FAL or allocation failure leaves the ordinary stock loader behavior available; local bridges use a small text fallback.

Public synchronous `loader_is_locked()` requests precede DND module map/free operations and readiness cleanup. They wait for Loader startup/unload work to finish before touching the supplied SDK's loaded-module list. This separates those DND startup operations without changing SDK synchronization. Direct drawing preserves the stock handling of held-key releases; held-input routing still needs device acceptance. All FAPs retain `UnloadAssetPacks`, and the loading animation owns the built-in SDK hourglass throughout its lifetime.

Initial loading before DND code begins remains the stock firmware loading screen. After entry, the Hub's splash covers its startup; between DND FAPs the retained FAL covers the interval with no app view attached. Native direct-draw acquire/release reset and commit the canvas, so host checks cannot establish an absolute zero-blank-frame timing guarantee. Record startup and handoffs on hardware to verify the visible result.

## Validation and performance limits

Twenty ASan/UBSan host regressions, sixteen strict source sets, eleven entry-point lifecycles and five real shared-module links pass. Native manifest parsing, public API source contracts and normalized import names match the supplied API 88.7. This is source/host evidence; no current ARM relocation, flashed-device timing, heap or cumulative stack high-water result is claimed. See [tests/host/VALIDATION.md](tests/host/VALIDATION.md) and [MEMORY_AUDIT.md](MEMORY_AUDIT.md).

Thin Sheet/Journal wrappers and the lazy Combat table reduce those parent executable proxies. Running a FAL keeps its parent resident: integrated Hub UI and Bestiary tools can increase peak coexistence. The handoff also uses heap while the next app loads, and two independently mapped loading FAL images can briefly overlap when a standalone feature starts its local loading view. These choices prioritize continuity and avoiding a full Bestiary reload; they do not establish a universal RAM or latency improvement. Device acceptance remains in [DEVICE_TEST_MATRIX.md](DEVICE_TEST_MATRIX.md).

Loading cleanup keeps each animation callback and borrowed view/context alive until `icon_animation_free()` completes. In the supplied firmware, that calls timer delete plus command-queue flush. Stopping alone is not a callback completion barrier. Do not clear the callback/context or release its view before this free returns. Handoff cleanup also unsubscribes before deleting its timer and freeing its mutex/context.
