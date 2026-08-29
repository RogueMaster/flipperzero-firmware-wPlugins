<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1>

# DNDolphins

DNDolphins is an offline 5E-compatible toolkit for Flipper Zero. The project is split into five independent FAPs so each workflow owns only the data and memory it needs.

## Applications

| App | App ID | Purpose | Writable root |
|---|---|---|---|
| DNDolphins | `dndolphins` | Characters, spells, inventory, dice and character combat | `/ext/apps_data/dndolphins/` |
| DNDAdventure | `dndadventure` | Campaigns, campaign packs, scenes, progression, rewards and milestones | `/ext/apps_data/dndadventure/` |
| DNDJournal | `dndjournal` | Per-character journal and Adventure milestone history | `/ext/apps_data/dndjournal/` |
| DNDInitiative | `dndinitiative` | Party roster, initiative, rounds, HP/AC and conditions | `/ext/apps_data/dndinitiative/` |
| DNDBestiary | `dndbestiary` | Monsters, custom monsters, monster packs and encounters | `/ext/apps_data/dndbestiary/` |

DNDolphins launches Journal, Adventure, Initiative and Bestiary only by explicit absolute FAP path. The launch contract uses `/ext/apps/Games/dndjournal.fap`, `/ext/apps/Games/dndadventure.fap`, `/ext/apps/Games/dndinitiative.fap`, `/ext/apps/Games/dndbestiary.fap` and `/ext/apps/Games/dndolphins.fap`; app IDs are manifest identifiers and are not used as Loader launch targets. Bestiary is character-independent for launching, but receives the loaded character ID when one exists so Add to Initiative targets that character. Journal, Adventure and Initiative also receive the loaded character ID when DNDolphins has one; when they are opened without a handoff argument they read `custom_active_profile.txt` and fall back to character 0 if no active reference is available. Every handoff tears down the outgoing FAP's owned views, services, callbacks, heap state and caches before Loader starts the destination.
The Loader API itself is isolated to `dnd_handoff.c`; app modules do not include `<loader/loader.h>` directly.

Companion apps fully tear down before another FAP is launched. DNDolphins does not keep Adventure, Journal, Initiative or Bestiary runtime state resident while those apps are open.

## DNDolphins

- The current writer emits schema-5 character text saves named `ch_{id}_{characterName}_{characterLevel}.txt` directly under `/ext/apps_data/dndolphins/`. Loading is best-effort by recognized field name: file version/order, unknown fields and malformed unrelated fields do not reject the rest of a readable character.
- Character save/load has no global heap-consistency predicate. Optional dynamic collections are handled independently, so a missing unrelated allocation cannot veto otherwise valid character fields.
- The character picker is storage-backed. It keeps only an eight-record metadata window in RAM and scans the directory for the next/previous window as navigation moves. Character-list heap use therefore stays bounded as the number of characters grows.
- Same-stem `.shd` character shadows preserve saved state by character level/name and are refreshed only for actual character-state updates. They are write-only sidecars: DNDolphins never scans, opens, parses, recovers from or reserves IDs from `.shd` files. Shadow maintenance is best-effort and can never make a successful primary `.txt` save fail; delete/archive operations do not remove historical shadows.
- `custom_active_profile.txt` is stored at the character root and is loaded best-effort by its `Active=` field. If its selected character no longer exists, DNDolphins advances to the next available primary character ID and wraps when needed. A failed/unreadable selection is never allowed to publish default `New Hero` data over an existing save. `exports/` and `archive/` are dedicated subdirectories.
- On startup, character files matching `ch*.txt` are relocated from `/ext/apps_data/dungeons_and_dolphins/profiles/` into `/ext/apps_data/dndolphins/` when no current primary character with that character ID is already present. Files are moved unchanged and then interpreted by the tolerant character loader; no other old-app data is migrated.
- Multiclass levels, subclasses, abilities, saves, all 18 skills, proficiency/expertise, passive scores, HP, AC, speed, initiative, exhaustion, death saves, inspiration and XP/milestone leveling are tracked.
- When total character level rises, XP is raised to the minimum XP required for that level if the stored value is lower. Higher XP is preserved and lowering level never lowers XP.
- Species, background, alignment, languages, feats, tools, armor/weapon training, senses and proficiencies are editable.
- Spellcasting supports known/prepared/always-prepared/ritual/free-cast states, multiclass slots, Pact Magic, Mystic Arcanum, spell points, Spell Attack and Spell Save DC.
- Owned spells and items are stored outside the core character file in `ch_{characterId}_spellbook.txt` and `ch_{characterId}_items.txt`. The normal character load leaves both collections unallocated; relevant Magic/Combat/Inventory/resource screens use a bounded **up-to-eight-record sidecar cache** that grows 1 -> 2 -> 4 -> 8 with the actual records in the current page. Crossing a cache boundary saves the outgoing page if changed, frees it, and loads the next/previous page; leaving the workflow frees the collection entirely.
- Combat weapon/spell discovery is sidecar-authoritative: opening Weapon Attacks or Spell Attacks streams the owned sidecar and rebuilds a compact logical-index map, so a stale or empty eight-record page cannot hide owned combat records. The selected/visible record still hydrates through the normal bounded page cache.
- Master `spells.txt` and `items.txt` catalogs are opened only for discovery/add/change-from-catalog workflows. An already-owned record is self-contained in the character sidecar and does not depend on its master catalog. Sidecar records are independent escaped lines rather than numbered `Spell0...` / `Item0...` variables; counts and cache/page positions are derived at runtime and are never serialized as identity.
- Pre-sidecar embedded `Spell*` and `Item*` character fields are intentionally ignored. There is no migration or fallback reconstruction path for those old fields.
- Combat has a shared `Normal` / `Advantage` / `Disadvantage` attack mode used by weapon and spell attack d20 rolls. Damage rolls, saving-throw spells and automatic effects are unchanged.
- Combat rows are formatted one visible row at a time instead of allocating the full Combat menu string table on the stack.
- Spell combat keeps structured spell mappings for cast-level scaling, multi-attack behavior, secondary dice and special resolution. A guarded Notes `XdY` parser remains a fallback for simple unmapped spell damage rather than replacing structured spell logic.
- Inventory includes equipment, containers, attunement, charges, ammunition, armor/shields, weapon attack data, currencies and character resources.
- Dice include d4, d6, d8, d10, d12, d20 and d100 with advantage/disadvantage, modifiers, multiple dice, Guidance and individual result review.

## DNDAdventure

- Sole owner of campaign identity, active campaign, scenes, checkpoints, quest flags, achievements and campaign-pack state.
- Bundled campaign data is in DNDAdventure assets; writable campaign data exists only under `/ext/apps_data/dndadventure/`.
- Campaign state is never serialized into `PocketCharacter` or DNDolphins character files.
- DNDAdventure receives a character ID reference and may read current character data for skill modifiers or write legitimate character rewards such as inventory items.
- Custom campaign discovery, validation, installation, registry and enable/disable code is compiled only into DNDAdventure.
- Active campaign, campaign progress, campaign registry and enabled campaign index each use one canonical writable destination. Adventure does not maintain alternate `.tmp`/`.bak` generations for those files.
- Adventure milestone output is one-way into DNDJournal. The related guard is saved before the Journal entry is emitted so replaying an already-granted branch does not intentionally duplicate the milestone.

## DNDJournal

- Entries live under `/ext/apps_data/dndjournal/ch_{characterId}/` as timestamped text files. Entry metadata/body uses named fields loaded independently in any order; the old compact metadata row is intentionally not loaded.
- The list is storage-backed and newest-first. Page selection scans timestamp filenames first, then opens only the selected eight files to hydrate metadata; the 192-byte body is loaded only when that entry is opened.
- There is no small fixed journal-entry count cap caused by an in-memory full-entry array. Large journals trade additional directory-scan time for bounded RAM use.
- Journal parsing is streaming and does not require firmware `qsort` or a large encoded-line buffer.
- Create, edit, complete and delete entries without embedding them into character saves.
- Milestone entries can launch DNDAdventure with the same character ID. Journal never creates or changes Adventure progress.

## DNDInitiative

- Separate per-character Party Roster and active combat state. Initiative saves use independent named fields and load them by field name in any order; malformed/unknown fields are skipped without rejecting neighboring data.
- Start New Combat includes `Roll for All`; hold OK permits manual participant editing/initiative entry.
- Short Back during combat moves to the previous turn; hold Back returns to the Initiative menu.
- Supports negative HP, AC, current/max HP, initiative modifiers, conditions, rounds and turn position.
- Bestiary handoffs append monsters to the selected character's Initiative store after Bestiary teardown.

## DNDBestiary

- Bundled monster reference with streamed browsing and detailed stat blocks.
- Search/filter by name, challenge, type, source, environment and role.
- Custom monster create/edit/delete, favorites, recents, filters, diagnostics and saved encounters.
- Generated and saved encounters can be transferred to DNDInitiative.
- Monster-pack management remains Bestiary-only; campaign-pack code is not linked into this FAP.
- Monster browsing uses a **15-summary working window**. Bundled/custom/enabled index acceleration uses sparse 32-record checkpoints plus small recent-ID/detail hints; full-file lookup tables are not retained in heap and streamed fallback remains authoritative.
- Encounter-state scans heap-own their 512-byte reader / 768-byte line workspace so nested Bestiary actions do not stack those large buffers.

## Memory direction

The project favors bounded caches and storage streaming over list-sized heap allocations. Current manifest stack reservations are:

- DNDolphins: 6 KB
- DNDAdventure: 4 KB
- DNDJournal: 4 KB
- DNDInitiative: 4 KB
- DNDBestiary: 6 KB

See `MEMORY_AUDIT.md` for the refreshed **3.2.15** Cortex-M4 ABI sizes, Verify Profile stack fix, eight-record spell/item paging peaks, Bestiary sparse-cache/15-summary working-set analysis, and recalculated source-derived stack peaks. Runtime heap or stack instrumentation is intentionally not included.

## Build

From a compatible RogueMaster firmware tree:

```text
./fbt fap_dndolphins fap_dndadventure fap_dndjournal fap_dndinitiative fap_dndbestiary
```

The project uses existing RogueMaster/Flipper firmware APIs and does not require enabling firmware-disabled `qsort`.

## Documentation

- `SAVE_SCHEMA.md` — character and companion storage ownership.
- `CAMPAIGN_PACK_SCHEMA.md` — DNDAdventure campaign format and pack inbox.
- `MONSTER_PACK_SCHEMA.md` — DNDBestiary monster format and packs.
- `MEMORY_AUDIT.md` — source-based stack/heap review.
- `FEATURE_CHECKLIST.md` — implemented feature summary.
- `DEVICE_TEST_MATRIX.md` — physical-device qualification cases.
- `ROADMAP.md` — current release baseline and future direction.
- `ATTRIBUTION.md` — rules and reference attribution.
