# Dungeons & Dolphins roadmap

Future releases only. Released work belongs in `CHANGELOG.md`.

## Persistence rules

Save structures are frozen by default. Do not change a schema merely for write safety, validation, cleanup or implementation convenience. Add persisted fields only when a new feature truly needs information that cannot be derived from current state. Add campaign variables only when a future campaign feature genuinely requires new persisted state.

Preserve explicit full-path FAP handoffs, teardown-before-launch, combat-only lazy eight-record Inventory/Spellbook paging, eight-record Feature paging, storage-backed Journal/profile paging, write-only character shadows, explicit DNDInventory-only starting-equipment grant, explicit Grant Initial Traits gating, lazy/no-hash progression metadata reads, and bounded Bestiary/campaign access. Character-owned Inventory, Spellbook, Feature and applied-grant sidecars remain centralized under `/ext/apps_data/dndolphins/` even though Inventory and Spellbook have their own FAPs.

## 3.5.1 — Hardware validation and bounded refinements

- Hardware-validate feat filtering, Inventory container remapping/quantity edits and opt-in Initiative history under fault injection and repeated cold launches.
- Measure stack high-water/free heap during Initiative history publication and maximum Inventory/Spellbook/Adventure projection adapter overlap; optimize only from measured pressure.
- Continue deterministic progression and structured spell-combat coverage only where player choices remain explicit.
- Stress Torii Between Tides, Moonlit Market, 346-monster Bestiary streaming and maximum Initiative rosters without increasing resident state.

## 3.6 — Combat and storage follow-up

- If hardware Loader testing shows marginal DNDolphins headroom, evaluate standalone Combat only if measurement justifies it; do not split solely for source organization.
- Consider narrower collection APIs to remove transient full-character compatibility adapters from companion FAPs while keeping the canonical character schema unchanged.
- Improve ammunition/container workflows further only when they can reuse existing state transactionally; do not add speculative persisted state.
- Continue declarative campaign/monster content and accessibility/long-text improvements.

## Out of scope

- Executable campaign scripting.
- Journal-created Adventure progress.
- Re-embedding companion-app state into character saves.
- Whole-file list loading where bounded streaming/paging works.
- Save-format changes made only for validation or atomicity.
