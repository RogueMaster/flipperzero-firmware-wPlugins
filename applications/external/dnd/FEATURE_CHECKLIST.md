# Dungeons & Dolphins feature checklist

- [x] Missing active-character IDs advance to the next available character and wrap safely.
- [x] Failed/unreadable profile switches restore the previously saved character instead of leaving default `New Hero` data active.
- [x] Journal/Adventure/Initiative direct launches use persisted active character when available, then character 0.
- [x] Bestiary launch from DNDolphins is character-independent.
| Area | Current implementation |
|---|---|
| Character files | Multiple editable schema-5 core text saves directly under `/ext/apps_data/dndolphins/` |
| Character picker | Storage-backed, ID ordered, fixed eight-record metadata cache; character count does not linearly grow picker heap |
| Character shadows | Same-stem `.shd` per saved character/level; active-level state refreshes in place, older shadows remain, and `.shd` files are never read/scanned or used to reserve IDs |
| Character autosave | Coalesced normal saves plus synchronous save before important handoffs/exit; character backup/recovery behavior remains DNDolphins-owned |
| Multiclass/build | Up to four classes, subclasses, levels, Hit Dice and structured grants |
| Ability/rules tracking | Abilities, saves, all 18 skills, proficiency/expertise, passive scores and misc modifiers |
| Vitals | HP/temp HP, AC, speed, initiative, exhaustion, death saves, inspiration, XP/milestone leveling and minimum-XP floor on level increase |
| Spellcasting | Known/prepared/always-prepared/ritual/free cast, multiclass slots, Pact Magic, Mystic Arcanum and spell points |
| Owned spell storage | Per-character `ch_{id}_spellbook.txt`; independent escaped line records; up-to-eight-record cache with 1 -> 2 -> 4 -> 8 growth, page-save/page-free behavior and full release outside spell workflows |
| Spell combat | Structured spell attack/damage/result mappings with supported cast-level scaling plus guarded Notes `XdY` fallback; combat eligibility/index discovery streams the owned spellbook sidecar directly before hydrating a selected page |
| Combat attack mode | Shared Normal/Advantage/Disadvantage mode for weapon and spell attack d20s; weapon discovery streams the owned item sidecar into a compact logical-index map |
| Combat draw memory | Visible Combat rows are formatted on demand instead of allocating all 22 row strings simultaneously |
| Inventory/equipment | Quantities, containers, weight, equipped/attuned state, charges, ammo, weapons, armor/shields and currency |
| Owned item storage | Per-character `ch_{id}_items.txt`; independent escaped line records; up-to-eight-record cache with 1 -> 2 -> 4 -> 8 growth, page-save/page-free behavior and full release outside item workflows |
| Spell/item catalogs | Master catalogs are discovery/add sources only; owned records are self-contained and normal character load does not open the master catalogs |
| Dice | d4/d6/d8/d10/d12/d20/d100, advantage/disadvantage, modifiers, Guidance and multi-die detail |
| DNDAdventure | Standalone campaign runtime and sole owner of all campaign progress/state |
| Campaign packs | Adventure-only inbox validation/install, registry and enable/disable with canonical direct writes |
| Adventure milestones | Adventure saves its guard first, then writes a one-way milestone entry to DNDJournal |
| DNDJournal | Standalone timestamped per-character entries with fixed eight-entry metadata cache, filename-first newest-page selection and body-on-open |
| Journal scale | No 24-full-entry list cap; list RAM remains bounded as file count grows |
| Journal→Adventure | Milestone entry can launch Adventure; Journal never creates/modifies Adventure progress |
| DNDInitiative | Standalone Party Roster/combat, Roll for All, manual participant edit and previous-turn Back behavior |
| Bestiary | Streamed monster browser/detail, filters, custom monsters, favorites, recents and diagnostics; 15-summary browse window with bounded sparse/recent lookup hints instead of full-file offset/hash heap tables |
| Encounters | Generate/save/manage encounters and transfer monsters to DNDInitiative |
| Monster packs | Bestiary-only pack management; campaign pack code is absent from Bestiary |
| Cross-FAP handoff | Outgoing app quiesces callbacks, releases owned heap/views/services/static caches, then calls the shared handoff module; only `dnd_handoff.c` imports Loader and starts the destination FAP |
| Firmware `qsort` | Not required |
| Companion stack reservations | DNDolphins 6 KB, DNDAdventure 4 KB, DNDJournal 4 KB, DNDInitiative 4 KB and DNDBestiary 6 KB based on the current source-derived peak audit |
| Verify Profile memory | Temporary 3,976-byte character verification state is checked transient heap, not stack; character save format is unchanged |
| Bestiary state-reader memory | Encounter scans use one checked transient 1,288-byte heap reader/line workspace; large nested reader frames are removed |
| Runtime memory probes | Not included; memory review is source-based |
| Embedded spell/item migration | Intentionally absent; old embedded `Spell*` / `Item*` fields are ignored and only current sidecars define owned records |

## Storage resilience

- Best-effort character loading applies recognized fields by name regardless of declared file version or ordering.
- Legacy `/ext/apps_data/dungeons_and_dolphins/profiles/ch*.txt` character files relocate unchanged into the current character root when no current primary with that character ID exists.
- Journal and Initiative use named-field loaders; their old compact-row formats are intentionally not parsed.
- `.shd` character shadows are write-only history and never participate in character discovery/recovery.
- Character persistence has no global heap-consistency save veto; optional dynamic collections are handled independently and filename metadata protects identity during partial loads.

- Core character loading deliberately clears spell/item pointers and does not hydrate their sidecars.
- Spellbook/item dirty fingerprints are independent from the core character fingerprint, so opening/releasing a sidecar cannot create a character `.shd` update.
- Crossing a spell/item cache boundary saves a changed outgoing eight-record page before freeing it; leaving the workflow frees the current page. Adventure item rewards scan/update the item sidecar through the same bounded eight-record window.
- Sidecar records have no serialized count and no authoritative line-number identity.
