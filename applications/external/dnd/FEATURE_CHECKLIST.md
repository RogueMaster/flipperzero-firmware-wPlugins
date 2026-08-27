# Request checklist

| Requested capability | Version 3.0.2 status |
|---|---|
| Flipper FAP d20 icon | `icon.png`, 10x10 1-bit, assigned by `fap_icon` and FBT-validated |
| Buttons and screen only | Implemented |
| Multiple independent characters | No fixed application limit; dynamically indexed from SD |
| Separate readable `.txt` per character | `ch_{x}_{characterName}_{characterLvl}.txt` |
| Autosave every character mutation | Implemented with checksum/backup recovery |
| Multiclass levels and class-linked perks | Up to four classes; features store source class and gained level |
| Character stats and 2024 proficiency formula | Implemented |
| All six saving throws | Explicitly displayed/editable in Abilities & Saves |
| All 18 standard skills grouped by ability | Implemented |
| AC, initiative, passive scores, Spell Attack, Spell Save DC | Implemented and visible |
| Known/Prepared/Always Prepared/Ritual/free-cast spells | Implemented per spell |
| Class-and-level spell catalog default | Implemented for annotated records; hold OK for All; streamed pages retain ten records and sort by level then name |
| Spell slots and Wizard Arcane Recovery | Implemented with Short/Long Rest rules |
| Feature recharge cadence | Manual, turn, encounter, dawn, Short/Long, or Long |
| Items, equipment, weapons, attack/damage rolls | Implemented; standard armor and weapon selections populate supported combat and AC fields |
| Multi-die individual results plus sum | Implemented with paging; Guidance mode adds a visible d4 to d20 rolls |
| Animated dice sequence | Implemented with original code-drawn frames |
| CP/SP/EP/GP/PP | Implemented |
| Notes, adventure notes, item notes, milestones | Implemented; milestones can level a selected class once |
| Party initiative presets and turn tracking | Per character with preset HP/AC, round/current turn, negative HP, participant field editor, armed removal, conditions, history, and undo |
| SD catalogs and custom long-OK text | Implemented for names; Background uses short catalog/long custom |
| Background catalog in its own SD file | `catalogs/backgrounds.txt` included |
| Core and add-on option names | Names-only catalogs include packaged and optional add-on selections |
| Structured rules-aware grants | Species, background, feat, class feature, subclass feature, spell, and item grants have review/apply/skip state |
| Core class-feature assignment | Names in `abilities.txt`; assigned feature stores class, level, notes, uses, formulas, and recharge |
| Per-class spellcasting | Shared multiclass slots, casting modes/ability, limits, spellbook, Pact slots, Arcanum, and spell points |
| Inventory resources | Containers, carried/equipped weight, capacity, armor/shield AC, attunement, ammo groups, and charges |
| Catalog diagnostics | Stable-ID validation, duplicate detection, required-field checks, and packaged-asset fallback |
| Fantasy quest / choices / skill checks / achievements | Data-driven Adventure mode with branches, rewards, flags, achievements, sprite tags, and checkpoints |
| Old save backward compatibility | Schema 2 is the compatibility baseline; unknown versions are preserved without overwrite and future schema changes must retain or migrate schema 2 |
| Stable save schema and migration | Explicit schema dispatch, checksum-first validation, non-destructive asset-to-data relocation, and no blank replacement after a failed load |
| Profile portability | Rename, chunked duplicate/export/archive, validated import, and checksum diagnostics |
| Recoverable prior generation | One previous successful save retained per profile with explicit restore action |
| Translation and accessibility | Runtime translation removed for speed; documented high-contrast display and control conventions remain |
| Separate bestiary application | Dolphin Bestiary is a separate FAP declared in the same manifest with an exclusive source list and asset namespace |
| Monster discovery and encounter roles | 340 bundled records, 20-record windows, combinable source/environment/name/challenge/type filters, role weighting, browser and generated-encounter OK-opened details, full-screen scrollable stat-field readers, preserved return selection, and buffered diagnostics |
| Custom monster lifecycle | Stable-ID edit, visible confirmed custom-only delete, and atomic rewrite/rollback in a separate custom index/stat-block layer that cannot replace packaged tables |
| Campaign pack manager | On-device selection, per-profile/per-campaign progress, manifest diagnostics, schema, and starter template |
| Complete structured editors | Full attack-template and grant editors with autosave |
| Device resilience | Allocation stress runner, SD read-only fallback, unsaved warning, retry control, and published hardware matrix |
| Direct-launch memory safety | Empty profiles allocate only their used spell, feature, item, journal, and grant records; all five groups grow on demand with checked limits; spell pages retain ten records |
| Asset namespace | Packaged catalogs, campaigns, and monster tables use `APP_ASSETS_PATH`; writable profiles, custom campaigns/progress, and custom monsters use persistent `APP_DATA_PATH` with copy-verify-cleanup relocation |
| Cross-FAP navigation | Each application ends with a menu entry that queues the other FAP and exits cleanly |

The expansion catalogs intentionally contain option names and original metadata rather than proprietary descriptions or mechanics. Custom text and notes support owned books, homebrew, errata, and table rulings.
