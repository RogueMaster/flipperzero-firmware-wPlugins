# Catalog and content policy

- Bundled catalogs may contain public option names, original classification metadata and freely licensed/open reference data.
- Do not bundle proprietary descriptive rules text, copied adventure prose or artwork without redistribution rights.
- Stable IDs should be lowercase, filename-safe where required and durable across spelling corrections.
- DNDolphins owns character catalogs/metadata; DNDAdventure owns campaigns/campaign packs; DNDBestiary owns monster data/monster packs.
- Direct custom records and installed packs must stay in the owning app's data namespace rather than modifying packaged assets.
- Character `.shd` history is runtime user data, not catalog content.
- Structured spell-combat mappings should remain machine-readable rules data where cast-level scaling, multiple attacks, secondary dice or special resolution are required. Free-form spell Notes are a fallback for simple dice expressions, not the canonical source for complex combat behavior.
- Unknown/incomplete mechanics should remain player-entered, custom or diagnostics-gated instead of being guessed.
- Campaign behavior remains declarative and bounded; executable scripting is outside the current design.

See `ATTRIBUTION.md`, `CAMPAIGN_PACK_SCHEMA.md` and `MONSTER_PACK_SCHEMA.md` for ownership and format details.

## Owned spell/item records

Master `spells.txt` and `items.txt` catalogs are discovery/add sources, not runtime backing stores for records already assigned to a character. After selection, the complete owned record is persisted in `ch_{characterId}_spellbook.txt` or `ch_{characterId}_items.txt`. Normal character loading does not open either master catalog. Owned sidecar records remain usable if the master catalog changes or is reordered because they carry their own stored fields. Runtime owned-record access is bounded to **eight spell/item records at a time**; cache starts/counts and line positions are transient lookup state, not persisted identity. The catalog is consulted again only when the user explicitly opens an add/change-from-catalog workflow. The master spell/item catalog files themselves are already read incrementally through a small reader/line buffer rather than loaded wholesale; only the current selection metadata page is retained (10 entries for spells and 24 for items in the current UI).

Combat discovery is also sidecar-authoritative. Entering Combat > Weapon Attacks or Combat > Spell Attacks streams the character-owned sidecar one valid record at a time and rebuilds a compact logical-index map. The master catalogs are never consulted for Combat, and the existing eight-record UI page cache is not used as the source of truth for which owned records exist.
