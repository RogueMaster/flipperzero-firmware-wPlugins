# Dungeons & Dolphins changelog

Released changes only. Historical recovery spans are consolidated where individual interim builds add little value.

## 4.19 — Spell class filters

- Added **Character Classes** as the default Spellbook class filter, plus **Any Class** and all 13 supported class filters.
- `Allowed` applies character spell-list/level eligibility; `All Spells` browses the selected catalog class directly.

## 3.5.4 — Spell ordering, feat filtering, Inventory paging and ASI

- Sorted the bundled 448-spell catalog by level, then name.
- Tightened progression **Feats: Allowed** to supported prerequisite and duplicate checks; custom/unrecognized feats are available through **All**.
- Repaired stale Inventory page residency and removed the `Page unavailable` placeholder.
- Hardened ASI `+1/+1` so the first pick changes no score and the second applies exactly +1 to two different abilities.

## 3.5.3 — Grant actions and character deletion

- Repaired **Grant Initial Traits** and **Apply Level Grants** persistence/status handling; successful actions report `Updated`, satisfied actions report `No changes`.
- Made character/Feature/Spellbook payloads authoritative when applied-grant markers are stale or missing, and reduced grant metadata rescans.
- Fixed Character Delete for active/profile-0 characters, sidecar cleanup, surviving-profile selection and result messaging.

## 3.5.2 — Explicit level grants, level-up rules and menu repairs

- Moved deterministic progression grants behind **Apply Level Grants**; **Grant Initial Traits** handles starting/level-1 deterministic grants.
- Level increases now add fixed-average Hit Die HP plus Constitution modifier, refresh class/global Hit Dice, and apply the same rule through Journal milestone leveling.
- Restored automatic starting-equipment initialization for a truly empty, never-granted Inventory and repaired Inventory page/window state.
- Added case-insensitive loose-ammunition matching by required ammo token anywhere in the Item name.
- Removed Adventure **Campaign Diagnostics** / **Installed Pack Controls** and Bestiary **Monster Pack Controls**; reordered Bestiary with Generate Encounter second and Pack Diagnostics last.

## 3.5.1 — Level choices, grants, ammunition and Adventure controls

- Repaired Level Choices opening/no-pending feedback and deterministic initial-grant application, including species spell grants.
- Added loose Inventory ammunition consumption for compatible weapons without per-weapon ammo counters.
- Added Adventure full-scene Hold OK viewing, checkpoint Hold Left/Right controls, explicit restart confirmation and narrower preview text.

## 3.5.0 — Feats, Initiative history and content

- Added progression feat **Allowed/All** filtering.
- Added explicit completed-encounter history with end time, rounds, party state and surviving opponents.
- Added Stack Qty editing, safer container deletion/remapping and cached container names.
- Expanded deterministic subclass metadata and structured spell-combat coverage.
- Added **Torii Between Tides**, **Moonlit Market** and six Bestiary creatures.

## 3.4.0 — Profile projections, level review and navigation

- Added bounded Level-Up Review and expanded deterministic progression metadata.
- Added narrow streamed profile projections for Inventory, Spellbook and Adventure to reduce resident character memory.
- Expanded structured Spell Combat mappings and Initiative large-roster navigation.
- Reworked DNDolphins Home/Combat row indexing and section presentation.

## 3.3.8 — Home menu ordering

- Reordered the DNDolphins Home menu into character, resource, encounter and campaign-play groups.
- Replaced raw Home indices with named `DndolphinsHomeIndex` values for dispatch and return focus.

## 3.3.7 — Shared profile handoff

- Consolidated active-profile lookup and cross-FAP Loader handoff into `dnd_profile_handoff.*`.

## 3.3.6 — Inventory currency grants and navigation

- Made starting-inventory currency and Items publish in one synced Inventory-sidecar transaction.
- Added exact persisted currency reporting for the one-time Inventory regrant.
- Updated Initiative Back/previous-turn controls and DNDolphins submenu focus restoration.

## 3.3.5 — Paging, catalogs and Spellbook ordering

- Added companion return-focus handoff and compact catalog paging presentation.
- Added bounded sidecar/catalog offset caches for Inventory and Spellbook.
- Added deterministic owned-Spellbook level/name ordering and expanded Item catalog category filters.
- Added generic Spell Scroll catalog rows for cantrip and levels 1–9.
- Moved app-specific helpers out of shared modules and removed a stale unused spell-save wrapper.

## 3.3.4 — Ritual Adept and draw-path stability

- Added Combat → Rituals for eligible known Wizard ritual spells.
- Moved storage/allocation work out of project draw callbacks and tightened allocation cleanup.
- Guarded companion profile badges against the internal invalid-profile sentinel.

## 3.3.3 — Wizard combat eligibility and Inventory regrant

- Corrected Wizard Spell Attacks preparation/free-cast eligibility.
- Added the one-time Hold OK starting-inventory regrant (`InitialInventory=2`).
- Made routine save/add/equip/prepare/grant success notices transient.

## 3.3.2 — Standalone Inventory and Spellbook parity

- Finalized standalone Inventory/Spellbook collection UIs with full editors, immediate persistence and bounded paging.
- Standardized persisted active-character lookup and companion profile badges.
- Added low-memory startup/list behavior and standardized companion Back handling.

## 3.3.1 — Inventory ownership and campaign discovery

- Moved Currency, encumbrance and starting-equipment controls into DNDInventory.
- Switched starting equipment to an explicit grant workflow for that release.
- Added bounded campaign discovery/index handling and Journal → Adventure continuation intent.

## 3.3 — Inventory/Spellbook FAP split

- Split Inventory and Spellbook into standalone FAPs.
- Moved their large catalogs/assets to their owning FAPs while keeping character-owned sidecars under `/ext/apps_data/dndolphins/`.
- Reduced DNDolphins loader/runtime pressure by removing collection-heavy UIs from the main FAP.

## 3.2.32–3.2.35 — Progression and spell-catalog update

- Moved Features and applied-grant history into lazy character sidecars.
- Added supported species progression, normalized grant IDs and expanded Item/Spell catalog filtering.
- Corrected Eldritch Knight/Arcane Trickster third-caster and Wizard-list behavior.
- Reduced resident progression memory.

## 3.2.6–3.2.31 — Collection recovery period

- Reworked Inventory/Spellbook persistence into character sidecars with bounded eight-record paging.
- Restored reliable Add/Edit/Delete, repeated adds, page-boundary handling and Hold OK Equip/Prepare actions.
- Removed render-time collection I/O/cache mutation and restored collection/catalog workflows under Flipper memory limits.
- Progressively separated/hardened Adventure, Bestiary, Journal and Initiative companion workflows.

## 3.2.5

- Restored Bestiary saved/generated encounter transfer to Initiative and expanded the saved Party Roster.
- Added a lower-memory Initiative launch path and related setup/navigation fixes.

## 3.2.4

- Added named saved-encounter management and encounter Difficulty/composition warnings.
- Added the transactional Bestiary-to-Initiative encounter handoff foundation.

## 3.2.3

- Changed Bestiary transfer to one-button Initiative launch for single monsters and encounters.

## 3.2.2

- Hardened parent-directory creation and deferred Loader handoff.
- Added single-monster Initiative transfer with AC/HP and active-combat append support.

## 3.2.1

- Persisted Bestiary party settings and hardened saved-encounter/Initiative transfer failure handling.

## 3.2

- Fixed startup stack/MPU pressure from large save/migration temporaries.
- Added saved-encounter management and encounter-to-Initiative workflow foundations.

## 3.1

- Added schema-3 character saves with rollback snapshots.
- Added transactional campaign/monster pack installation and Bestiary favorites, recents, filter presets and saved encounters.

## 3.0.3

- Added buffered profile/campaign/Bestiary readers, reusable offsets and delayed autosaves.

## 3.0.2

- Restricted migration to mutable user data; packaged catalogs/campaigns/monster tables use app assets.

## 3.0.1

- Moved mutable profiles/progress/custom content into app data with non-destructive first-launch migration.
- Hardened failed profile loads and custom-monster persistence.

## 3.0

- Added generated-encounter drill-down/full-screen stat reading and atomic custom-monster storage.
- Reduced spell-picker memory and sorted packaged spells by level then name.
