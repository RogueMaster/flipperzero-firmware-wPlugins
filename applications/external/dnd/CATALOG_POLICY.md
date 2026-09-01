# Catalog policy

Bundled catalogs are read-only app assets; user-owned/custom data lives in app data.

- Character option catalogs provide compact names/metadata suitable for offline selection.
- Owned spells/items are copied into character sidecars and then treated as character state.
- Bestiary bundled monsters, user custom monsters and installed monster packs remain separate sources.
- Default custom Dolphin/Capybara assets are seed material only. They are copied only for a fresh custom-monster state and are never merged over user data.
- Campaigns remain declarative scene/choice files. Ghost Protocol is fictional defensive-security content and does not include real-world exploit instructions.
- Text pack formats stay editable; do not add checksum rejection.
- Do not duplicate the same owned catalog in multiple FAP asset trees unless runtime ownership requires it.

## Item catalog metadata

Bundled Item rows use `Name|Category|Rarity|Source`. Category and Rarity drive the streamed Inventory Name-catalog filters/presentation; Source records provenance. The Item catalog and owned `PocketItem` records currently do **not** contain a GP cost/value field. Do not encode prices into Source, Detail, weight or another unrelated field merely to preserve a reference value.

The bundled Scroll category contains exactly one generic Spell Scroll row for Cantrip and for each spell level 1 through 9. Scroll rarity follows the standard level progression: Common for Cantrip/Level 1, Uncommon for Levels 2–3, Rare for Levels 4–5, Very Rare for Levels 6–8, and Legendary for Level 9. Per-spell Scroll rows are not bundled.

## Spell catalog metadata

Bundled spell rows use `Spell|Level|Class, Class|School|Ritual|Source`. School, Ritual and Source are optional when parsing user/custom catalogs for compatibility, but bundled rows should populate all six fields so Add Spell filtering remains complete. Level/Class govern eligibility; School/Ritual/Source are copied into the owned spell and are also filter keys.

Spell filtering is performed while streaming the source catalog, before the bounded catalog page is selected. This keeps filtered pages dense without loading the whole spell catalog into memory.


## Bounded catalog paging

Inventory and Spellbook Name catalogs remain streamed from read-only assets. Each standalone collection FAP may retain a bounded filtered-page seek map (64 32-bit offsets maximum) and use a small buffered reader so repeated page navigation does not restart at byte zero or perform one storage read per character. Inventory may roll that 64-page window forward for catalogs with more than 64 filtered pages, preserving the same offset-array size while keeping sequential paging seekable. The seek map is invalidated whenever filter state changes. It is an acceleration hint only: catalog contents remain authoritative on storage and the full catalog is never materialized in heap.

## Feat progression filter

Progression/grant-driven feat selection defaults to **Allowed**; Hold OK toggles **Allowed / All**. Allowed includes only bundled rows whose represented prerequisites can be checked by the app: level gates, Grappler STR/DEX, Fighting Style Feature, Spellcasting Feature, Epic Boon level and already-owned non-repeatable feats. Repeatable bundled feats remain eligible. Unknown/custom rows are hidden in Allowed and available through All. Manual Features & Perks catalog editing is unrestricted.
