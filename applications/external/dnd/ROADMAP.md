# Dungeons & Dolphins roadmap

Future releases only. Released work belongs in `CHANGELOG.md`.

## Persistence rules

Save structures are frozen by default. Do not change a schema merely for write safety, validation, cleanup or implementation convenience. Add persisted fields only when a new feature truly needs information that cannot be derived from current state. Add campaign variables only when a future campaign feature genuinely requires new persisted state.

Preserve explicit full-path FAP handoffs, teardown-before-launch, combat-only lazy eight-record Inventory/Spellbook paging, eight-record Feature paging, storage-backed Journal/profile paging, write-only character shadows, explicit DNDInventory-only starting-equipment grant, explicit Grant Initial Traits gating, lazy/no-hash progression metadata reads, and bounded Bestiary/campaign access. Character-owned Inventory, Spellbook, Feature and applied-grant sidecars remain centralized under `/ext/apps_data/dndolphins/` even though Inventory and Spellbook have their own FAPs.

## 3.3.9 — Hardware validation and bounded profile access

- Hardware-validate the RAM-only draw contract under rapid scrolling across Profiles, Journal, Spell Attacks, Rituals, Weapon Attacks, Inventory, Spellbook and Bestiary, and confirm no SD activity/heap growth occurs merely from redraw.
- Run repeated cold-launch and cross-FAP handoff stress tests while monitoring firmware free-heap/fragmentation and stack high-water values; compare device measurements with the source-derived bounds in `MEMORY_AUDIT.md`.
- Hardware-validate the direct 8-record sidecar page offsets and filtered catalog seek maps under repeated forward/back navigation, edits/deletes, filter changes and malformed lines.
- Hardware-validate persisted Spellbook level/name ordering with mixed cantrip/leveled spells, duplicate names, edits that move records between levels, and full 24-record spellbooks.
- Hardware-validate Wizard Ritual Adept with prepared and unprepared known Wizard Ritual spells, multi-page spellbooks, no-Wizard characters and malformed/missing spellbook sidecars; ritual casting must consume no normal spell resource.
- Continue expanding verified deterministic class/subclass/species progression metadata while leaving player choices explicit.
- Add a bounded level-up results/review screen that combines numeric rule changes, deterministic traits, spell-choice notices and pending ASI/Feat choices without retaining progression metadata.
- Prototype bounded per-FAP profile projections for DNDInventory, DNDSpellbook and DNDAdventure so companion apps stop embedding the full character core. Keep the canonical character file unchanged: projections must be streamed by field name and write back only the fields each FAP legitimately owns.
- Split the broad storage implementation into narrow collection/profile interfaces only where measured RogueMaster `.fap` or heap results justify it; do not duplicate parsers merely for source-file aesthetics.

## 3.4 — Combat and Initiative

- Expand structured spell-combat mappings, including upcasting, multiple attacks and secondary effects.
- Improve Combat presentation without growing resident lists or re-linking the full Spellbook/Inventory UIs into DNDolphins.
- If hardware Loader testing still shows marginal DNDolphins headroom, evaluate a standalone `DNDCombat` FAP owning Weapon Combat, Spell Combat and attack-template execution. It should stream the existing character/Inventory/Spellbook sidecars, return through the existing handoff model and add no new persisted state. Do not split Combat solely for code organization if Loader headroom is already healthy.
- Continue improving Initiative roster-to-combat presentation and larger-encounter navigation without increasing resident state.
- Add completed-combat history only as a new Initiative-owned record if explicitly selected.

## 3.5 — Scale and compatibility

- Stress large character, Journal, campaign and monster indexes on hardware.
- Add narrow compatibility aliases only for demonstrated real files; avoid broad schema migrations.
- Continue hardware-driven stack reductions, especially Bestiary encounter writer paths if needed.

## Later

- Optional Item cost/value metadata and catalog price support, only if explicitly selected as a save-schema/UI feature; do not overload existing Item fields.

- More class/spell rules coverage.
- Better inventory/container/ammunition workflows using existing state where possible.
- More declarative campaigns and monster content.
- Accessibility and long-text improvements across all seven FAPs.

## Out of scope

- Executable campaign scripting.
- Journal-created Adventure progress.
- Re-embedding companion-app state into character saves.
- Whole-file list loading where bounded streaming/paging works.
- Save-format changes made only for validation or atomicity.
