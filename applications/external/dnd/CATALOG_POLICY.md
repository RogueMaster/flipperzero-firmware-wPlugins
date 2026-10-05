# Catalog policy

Bundled catalogs are read-only app assets; user-owned/custom data lives in app data.

- Character option catalogs provide compact names/metadata suitable for offline selection.
- Owned spells/items are copied into character sidecars and then treated as character state.
- Bestiary bundled monsters, user custom monsters and installed monster packs remain separate sources.
- Default custom Dolphin/Capybara assets are seed material only. They are copied only for a fresh custom-monster state and are never merged over user data.
- Campaigns remain declarative scene/choice files. Ghost Protocol is fictional defensive-security content and does not include real-world exploit instructions.
- Text pack formats stay editable; do not add checksum rejection.
- Do not duplicate the same owned catalog in multiple FAP asset trees unless runtime ownership requires it.

## SRD catalog and Homebrew visibility

The bundled rules catalogs are validated against the project's SRD data set. **Homebrew: Yes / No** is a separate visibility control and defaults to **Yes**.

- Character, Spellbook and Inventory browse catalogs are streamed from read-only assets rather than materialized in heap.
- Homebrew/DNDolphins Item rows remain physically available to the normal Item catalog so **Homebrew: Yes** can reveal them without changing the rules catalog.
- **Homebrew: No** rejects project Homebrew/DNDolphins rows while browsing. It never deletes, rewrites or hides already-owned character data.
- The normal Item asset contains **615 rows**: **579 SRD rows + 36 project Homebrew/DNDolphins rows**. Homebrew No yields the 579-row SRD view.
- The normal Spell catalog contains **355 SRD spells**.
- Character option fallbacks are SRD-only and are used only when the corresponding asset cannot be opened.

## Grant metadata

`character_assets/metadata/options.txt` is the SRD grant/progression view with **1,575 metadata rows / 616 grant-bearing rows**. Feat/feature dependency rows required by valid SRD choices remain present even when the dependency is not itself a top-level picker entry.

The small built-in Class/Subclass/Species/Background/Feat arrays in `dnd_app_core.c` are emergency SRD picker fallbacks only when an asset is missing. They are not appended to a valid catalog. Class Hit Die and spellcasting defaults are separate runtime rule mappings keyed by class name.

Release compliance does not rely on source labels alone. Fixed SRD grant payloads are cross-referenced against the SRD spell, feat, language, skill, save, proficiency, size, resistance, sense and speed vocabularies; **475 fixed SRD payloads** currently pass this gate. Compiled picker fallbacks must exactly match their SRD catalog files.

## Starting equipment and packed support assets

Starting-equipment tables are treated as grant data and are included in the SRD compliance audit. The bundled SRD tables currently contain **72 class-equipment rows** and **16 background-equipment rows**. The normal d100 trinket table follows SRD 5.2.1 and contains all 100 results.

The release gate enumerates all **44 files** under the five packed asset roots (Adventure, Character, Inventory, Bestiary and Spellbook). A packed file cannot be added or removed without updating its classification. Project Adventure campaign files and default Dolphin/Capybara custom-monster seeds are explicitly classified as Dungeons & Dolphins project content.

## Item catalog metadata

Bundled Item rows use `Name|Category|Rarity|Source`. Category and Rarity drive Inventory filters/presentation; Source records provenance. Owned `DndItem` records do not currently contain a GP cost/value field. Do not encode prices into Source, Detail, weight or another unrelated field.

The bundled Scroll category contains one generic Spell Scroll row for Cantrip and for each spell level 1 through 9. Scroll rarity follows the standard level progression. Per-spell Scroll rows are not bundled.

## Spell catalog metadata

Bundled spell rows use `Spell|Level|Class, Class|School|Ritual|Source`. School, Ritual and Source are optional when parsing user/custom catalogs for compatibility, but bundled rows should populate all fields so filtering and owned-spell metadata remain complete.

Spell filtering occurs while streaming the catalog before the bounded page is selected. The SRD Source selector is limited to the SRD namespace.

## Bounded catalog paging

Inventory and Spellbook catalogs remain streamed from read-only assets. Each collection FAP may retain a bounded filtered-page seek map (64 32-bit offsets maximum) and use a small buffered reader so repeated navigation does not restart at byte zero or perform one storage read per character. Inventory may roll that 64-page window forward for long filtered catalogs. The seek map is invalidated whenever filter state changes and is an acceleration hint only; catalog contents remain authoritative on storage.

## Feat progression filter

Progression/grant-driven feat selection defaults to **Allowed**; Hold OK toggles **Allowed / All**. Allowed includes only bundled rows whose represented prerequisites can be checked by the app. Unknown/custom rows remain available through All. Manual Features & Perks catalog editing is unrestricted.

## Language and proficiency catalogs

DNDolphins assets include `catalogs/languages.txt` and `catalogs/proficiencies.txt`. Proficiency rows use `Type|Name|Comma-separated class names`; supported types are Armor, Weapon and Tool. Asset reads use bounded buffers and 24-entry catalog pages without a small total-row cap. Owned selections use separate eight-record sidecar windows.

## Homebrew Item policy

Inventory has exactly 18 `420` rows sourced as `Homebrew` plus 18 `DNDolphins` project rows. **Homebrew: No** rejects those rows before catalog-category filtering. **Get Elevated** is a separate randomized-starting-bundle switch and never gates catalog visibility. Owned inventories are not catalog-filtered by either setting.
