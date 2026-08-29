# Dungeons & Dolphins roadmap

Current baseline: **3.2.15**.

The current direction is a five-FAP suite with strict ownership boundaries, storage-backed bounded lists, tolerant field-name loading, explicit full-path handoffs, and teardown-before-launch behavior.

## Persistence rule for all future work

The existing save structures are considered **frozen by default**.

- Do **not** change a save structure, save version, field layout, or field grouping merely for write safety, atomicity, validation, parser convenience, cleanup, or stricter consistency checking.
- Keep core character fields as independent named fields. The 3.2.14 spellbook/items sidecars are the deliberate exception: each owned record is one escaped line so list edits do not create numbered serialized variables.
- Keep best-effort field-name loading: recognized fields load independently, unknown fields are ignored, and one malformed field should not invalidate unrelated usable data.
- Prefer deriving information at runtime when it can be derived reliably from existing persisted state.
- Add persisted information only when a new feature genuinely introduces state that cannot be reconstructed from existing data.
- When a new persisted field is necessary, make it independent and optional whenever possible so older files remain usable without conversion.
- Keep ownership local: character state belongs to DNDolphins, campaign state to DNDAdventure, Journal state to DNDJournal, Initiative state to DNDInitiative, and Bestiary state to DNDBestiary.
- Do not introduce save-format migrations solely because the code changed internally. In particular, do not add migration/fallback code for the pre-3.2.14 embedded `Spell*` / `Item*` fields.

Baseline behavior to preserve includes explicit `/ext/apps/Games/*.fap` launches, direct-launch active-character fallback for Journal/Adventure/Initiative, next-available active-character recovery in DNDolphins, same-stem write-only character `.shd` history, XP floors on level increase, shared Combat attack mode, **eight-record spell/item sidecar paging**, storage-backed character picker paging, storage-backed Journal metadata paging/body-on-open, structured spell-combat rules, and complete outgoing-app teardown before handoff.

## 3.2.15 — Memory and runtime hardening

Implemented in the 3.2.15 baseline:

1. **Removed the DNDolphins Verify Profile stack spike.**
   - The temporary 3,976-byte `PocketSaveData` now uses checked transient heap storage.
   - Verification still uses the same best-effort core parser and does not alter character save structure.
   - The Verify Profile input path is now about **1,472 B** project-visible and no longer defines the DNDolphins peak.
2. **Reduced DNDBestiary nested encounter-state reader stack use.**
   - Encounter scans heap-own a 1,288-byte workspace containing the 512-byte reader and 768-byte line buffer.
   - Workspaces are released between scans; Bestiary state formats are unchanged.
3. **Reduced Bestiary retained lookup/cache heap.**
   - Removed the geometric full index-offset/hash and statblock-offset arrays.
   - Replaced them with 16 sparse 32-record checkpoints plus eight recent index and eight recent statblock hints per bundled/custom/enabled source path.
   - Streamed fallback remains authoritative; the complete fixed lookup cache is **616 B static RAM** with no persistent lookup-cache heap.
4. **Reduced Bestiary browse working set.**
   - The browse window is now **15 summaries / 2,580 B**, down from 35 summaries / 6,020 B.
   - With the packaged 340-monster catalog, retained project RAM is reduced by about **6,996 B** before detail lookup and **11,092 B** versus the old state after bundled statblock-cache construction.
5. **Recalculated source-visible stack peaks.**
   - DNDolphins: **3,000 B** / 6 KB reserved.
   - DNDAdventure: **2,768 B** / 4 KB reserved.
   - DNDJournal: **2,664 B** / 4 KB reserved.
   - DNDInitiative: **1,352 B** / 4 KB reserved.
   - DNDBestiary: **4,368 B** / 6 KB reserved.

Remaining hardware/runtime hardening:

- Stress 100/250/500+ primary character files and 100/500/1,000+ Journal entries.
- Repeatedly cycle DNDolphins → Bestiary → Initiative → DNDolphins and DNDolphins → Journal/Adventure → DNDolphins.
- Exercise Verify Profile under constrained heap and Bestiary save/rename/duplicate/archive/delete with maximum-size encounters.
- Hardware-stress the new DNDolphins 6 KB reservation and current companion reservations. If Bestiary still needs stack work, target the large encounter **writer** buffers next; the nested state-reader path is no longer the peak.

## 3.2.16 — Character, Journal, and navigation polish

1. Improve character-picker navigation without rebuilding a full in-memory profile array:
   - clearer page/position feedback,
   - optional streamed name search,
   - faster next/previous scans only if hardware testing shows a real latency problem.
2. Add Journal filtering using fields already stored today:
   - category,
   - completed/open status,
   - milestone/adventure views.
3. Add streamed Journal export and pruning tools without loading the complete Journal into RAM.
4. Improve Journal → Adventure continuation UX for milestone entries without allowing Journal to create or mutate Adventure progress.
5. Continue converting large display-only row arrays to on-demand visible-row formatting where it materially lowers stack.
6. **Sidecar lookup/index optimization only if hardware shows a real need.**
   - Preserve the current eight-record pager; consider disposable name/last-seen-line hints only if hardware shows repeated scan latency is a real problem.
   - If individual-record seeks are later introduced, a `{name, last_seen_line}` hint may accelerate scans but must remain non-authoritative and self-correct when records are reordered.
   - Do not persist line numbers or make owned records depend on master-catalog ordering.

None of these items requires a save-structure change.

## 3.2.17 — Adventure and pack management

1. Add installed campaign-pack removal scoped entirely to DNDAdventure-owned files.
2. Add a campaign-pack details/validation screen showing ID, name, compatibility, scene count, and conflicts before installation/enabling.
3. Improve campaign discovery and enabled-pack navigation for large pack libraries using offset/index caching rather than loading all campaign content.
4. Improve Adventure milestone/achievement presentation and Journal handoff behavior.
5. Add campaign-variable functionality only if a concrete Adventure feature requires persistent variables. If implemented, store only the new information required by that feature as optional DNDAdventure-owned named fields; do not restructure unrelated campaign progress.

## 3.3 — Combat, Initiative, and rules coverage

1. Expand structured spell-combat mappings, prioritizing:
   - upcast damage/healing,
   - multiple attack rolls,
   - secondary damage,
   - saving-throw resolution,
   - spells whose Notes fallback is currently less precise than a structured mapping.
2. Improve Combat presentation while keeping the current on-demand row formatter and shared Normal/Advantage/Disadvantage attack mode.
3. Improve Initiative combat UX:
   - faster HP/AC/condition editing,
   - clearer round/turn state,
   - better roster-to-combat transitions,
   - preserve standalone ownership.
4. Add completed-combat history only if that feature is selected. Combat history is genuinely new persisted information, so a new DNDInitiative-owned record/file is acceptable; it should not require changing existing character or Initiative roster fields.
5. Continue rules-aware character workflows using existing fields wherever possible rather than persisting derived values.

## 3.4 — Compatibility and scale without schema rewrites

1. Keep character compatibility best-effort and field-name driven rather than introducing a broad schema-conversion framework.
2. Add narrowly scoped field aliases only when real older files demonstrate a renamed field that cannot otherwise be recovered.
3. Keep old character-file relocation limited to the existing character-only source folder behavior; do not expand it into campaign/Journal/Initiative/Bestiary migration.
4. Stress very large campaign/monster indexes and improve streaming/index caches where needed.
5. Add recovery/import tools only when they can operate on existing formats; do not change formats merely to make validation easier.

## Later direction

- More structured spell and class-feature coverage.
- Better inventory/container/ammunition workflows using existing character fields.
- More encounter-generation templates and monster content without loading whole catalogs into RAM.
- Campaign authoring/validation improvements that remain declarative and bounded.
- Accessibility and long-text presentation improvements across all five FAPs.
- Hardware-driven stack reductions only after the relevant high-water paths are structurally reduced.

## Out of scope

- Executable campaign scripting.
- Journal-created Adventure progress.
- Re-embedding campaign, Journal, Initiative, or Bestiary state in the character payload.
- Replacing structured spell-combat rules with free-form Notes parsing.
- Save-format/version changes made only for write safety, validation, atomicity, or implementation cleanliness.
- Whole-file or whole-directory RAM loading where bounded streaming/paging can provide the same feature.
- Runtime heap/stack instrumentation unless a dedicated debugging build is specifically requested.
