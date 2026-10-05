# Dungeons & Dolphins — Expansion Context

This file preserves planning and engineering context that is useful when extending Dungeons & Dolphins but does not belong in the user manual, feature checklist, changelog or forward feature roadmap. It is intended to help future ChatGPT sessions make consistent decisions, avoid reintroducing old regressions, and distinguish a genuinely new feature from behavior that already exists.

## How to use the project documents

Use the current source as the final authority when documentation disagrees with code. For planning, treat `README.md` and `FEATURE_CHECKLIST.md` as the inventory of current user-visible behavior, `RULES_AUDIT.md` as the current game-rule contract, `SAVE_SCHEMA.md` as the persistence contract, `MEMORY_AUDIT.md` as the memory model, `SOURCE_OWNERSHIP.md` as the module-ownership contract, and `ROADMAP.md` as future work only.

`EXTRA_.md` should hold historical decisions, removed planning notes, expansion guardrails, risk areas and rationale that can improve future implementation choices without cluttering user-facing documentation.

When a roadmap feature is implemented, remove it from the roadmap, document the resulting current behavior in README and the capability-level result in FEATURE_CHECKLIST, and summarize only the actual release change in CHANGELOG. Do not keep implementation-defense notes, validation chatter, manifest-refresh bullets, or statements that something is merely "unchanged" or "still working" in CHANGELOG.

README should remain organized per FAP in the real on-device option order. It should explain normal actions, Hold controls, shortcuts, defaults, implicit automation and hidden conveniences that provide user value. FAP-owned behavior may be discovered from source structure and behavior-bearing routines, but low-level allocation/free, parser, cache, drawing and generic storage helpers are not user features by themselves and should not be promoted into README.

FEATURE_CHECKLIST is a capability inventory, not a duplicate control manual. ROADMAP should contain only genuinely missing features or a clearly scoped improvement to an existing feature. The current roadmap convention is eight features per planned build with full explanatory sentences describing what each feature would do and how it would help.

## Core expansion guardrails

- Keep character-owned Inventory, Spellbook, Feature and applied-grant data centralized under `/ext/apps_data/dndolphins/` so every FAP sees the same character-owned records. Inventory and Spellbook being separate FAPs must not fragment those live records into separate app-data stores.
- Preserve bounded streaming and paging. Do not load whole Item, Spell, Feature, Bestiary, Journal, campaign or custom-pack collections into RAM when a bounded reader/window can serve the feature.
- Keep player-choice progression explicit. Automation may calculate deterministic rules and may apply deterministic grants only through the explicit grant actions, but it must not silently choose feats, ASIs, spells, subclasses, Fighting Styles, Invocations, Metamagic or similar player decisions.
- Save structures are frozen by default. Add a persisted field only when a real user-facing feature requires information that cannot be derived safely from current state. Do not change a save schema merely for validation, cleanup, atomicity or implementation convenience.
- Add campaign variables only when a future campaign feature genuinely requires new persisted campaign state. Do not create speculative variables for convenience or future-proofing.
- Prefer direct play utility over diagnostics/control screens. A new screen should help a player or GM during normal use; internal inspection belongs in engineering documentation unless it has a real user workflow.
- Preserve text-editable pack/catalog formats. Do not add checksum or hash rejection as a normal requirement; current pack/catalog policy intentionally favors best-effort editable text and structural validation.
- Prefer best-effort field-name compatibility where the current schemas already use it. Avoid migrations that rewrite working user data solely to normalize or validate it.
- Cross-FAP launches should remain explicit full-path handoffs with outgoing callbacks/timers/project state quiesced before Loader starts the next FAP. Short Back and Hold Back semantics should remain consistent with the current README unless a deliberate UX feature changes them.
- Canvas/draw callbacks should remain presentation-only: no project heap allocation, storage reads, collection hydration, rewrites or polling from draw time.
- Do not introduce firmware `qsort` dependencies merely for convenience. Existing deterministic ordering uses bounded app-owned approaches or asset ordering.
- Keep large transient work action-local and release it on every success/failure exit. If a companion needs a narrower storage API, prefer adding that bounded API over restoring a resident full-character object.

## Current persistence and ownership assumptions worth protecting

- Canonical character profiles belong to DNDolphins and use exact character-profile filenames; sidecars, exports, shadows, Journal files and Initiative files must never satisfy a canonical-profile lookup.
- Character ID `0` is valid.
- Active-profile resolution is shared across all FAPs and should remain one contract rather than diverging into app-specific parsers.
- Inventory currency is authoritative in the character Inventory sidecar, not the canonical character profile.
- Inventory records, Spellbook records, Features and applied deterministic-grant IDs are separate character-owned sidecars and are streamed/paged rather than embedded back into the canonical character save.
- Character `.shd` files are historical snapshots, never live authority. Current level history is a core SHD plus separately owned Inventory, Spellbook, Feature and applied-grant SHDs. Pre-4.19.0b core-only SHDs remain restorable without treating missing sidecars as evidence that those collections were empty.
- Current-level collection snapshots are history; the live `.txt` sidecars remain authoritative.
- Inventory, Spellbook and Adventure intentionally use narrow profile projections for the fields they need instead of retaining a full character in resident app state.
- Journal, Adventure, Initiative and Bestiary own their own app-specific persistence except for explicit bridges to character-owned sidecars.
- Initiative owns completed-encounter history. Saving history is explicit; it is not a Journal or Adventure responsibility.
- Campaign files should not be deleted as a recovery mechanism. Existing campaign-pack copy/install behavior intentionally leaves failed or existing content recoverable rather than destructively removing it. If future campaign/pack management needs a disable control, prefer an active/inactive state over deleting content or silently removing it from the catalog/index. Apply the same non-destructive principle to monster-pack management.

## Starting Inventory and deterministic grant semantics

Starting Inventory and character progression have several deliberate one-shot/explicit boundaries that future features should not accidentally collapse:

- A truly empty, never-granted Inventory may receive normal starting equipment automatically when Inventory opens.
- Once the starting-Inventory marker exists, deleting all Items must not silently reseed starting equipment.
- The explicit Inventory regrant is a deliberate one-time override and should preserve existing Items while adding the starting package again according to the current marker rules.
- **Grant Initial Traits** is for starting/level-1 deterministic grants.
- **Apply Level Grants** is the explicit catch-up action for currently eligible deterministic species/class/subclass grants.
- Increasing a class level updates deterministic numeric advancement such as fixed-average HP, Hit Dice and derived limits, but must not silently apply class/species/subclass Feature or Spell grants.
- Actual character/Feature/Spellbook payload is authoritative for whether a deterministic grant exists. Applied-grant markers are bookkeeping and must not invalidate a successful real write or suppress repair of a missing record.
- Level Choices owns player-selectable ASI/Feat opportunities. The two-stat `+1/+1` choice must not mutate the first score on the first pick and must apply exactly one point to each selected ability when committed.

## Memory and performance decisions removed from the roadmap

These were previously roadmap/testing notes. They remain useful engineering context but are not product features.

- The design optimizes resident memory first. Inventory, Spellbook, Feature lists, Bestiary access, campaigns and Journal/profile access use bounded windows/streams rather than whole-file hydration.
- The established collection page size is eight records for Inventory, Spellbook and Features. Do not casually increase it without a measured reason.
- Catalog offset maps are bounded acceleration hints; storage remains authoritative and the entire catalog is not materialized in heap.
- Projection-based companion FAPs may still use bounded transient compatibility adapters for a few shared operations. If hardware measurements show those peaks are a problem, prefer narrower collection/storage APIs rather than moving a full canonical character back into resident companion state.
- App stack reservations were intentionally kept tight and app-specific. When adding a feature with substantial stack locals or nested calls, review the current `MEMORY_AUDIT.md` before increasing stack reservations.
- Historical hardware-risk areas included grant/catalog overlap, Inventory/Spellbook sidecar rewrite peaks, Adventure reward bridging, Initiative history/profile synchronization, maximum Bestiary encounter generation and repeated cross-FAP launches. These are engineering test targets when touching those systems, not roadmap items.
- Large assets and catalogs should stay with the FAP that owns the user-facing feature so unrelated FAPs do not grow simply because they link a shared file.


## Removed engineering follow-up ideas worth retaining

These were once roadmap items but were removed because they describe implementation strategy or testing rather than user features. They can still guide expansion work when the relevant subsystem changes.

- DNDCombat is now a standalone FAP because Loader/runtime isolation justified the split. Preserve that ownership boundary; do not fold Combat back into DNDolphins merely for source convenience.
- If transient full-character compatibility adapters become a measured memory problem, add narrower collection/profile APIs that carry only the fields needed by Inventory, Spellbook or Adventure instead of changing the canonical character schema.
- Improve ammunition and container workflows by reusing existing Item state transactionally before inventing new persisted state.
- Favor declarative campaign and monster content over executable scripting, and keep long-text/accessibility improvements compatible with bounded readers.
- When touching progression, Inventory containers or Initiative history, fault-injection and repeated cold-launch testing are historically high-value because those areas have had persistence/navigation regressions before.
- When touching large campaigns, Bestiary streaming or maximum Initiative rosters, stress the bounded access paths rather than raising resident limits as the first response.

## Source-ownership decision rules

- Shared code is appropriate when multiple FAPs use the same contract or when splitting it would duplicate a canonical parser, scanner, rules primitive or transactional publish path.
- App-specific behavior belongs with the FAP that owns the user workflow. Moving a helper local is appropriate only when no other FAP needs the implementation.
- A shared storage operation may remain shared even with one current caller when moving it would duplicate canonical parsing, scanning or atomic rewrite/publish behavior.
- Implementation-only helpers should stay private to their source. Do not preserve stale wrappers solely to avoid reorganizing callers; remove unused wrappers after confirming the real behavior is still reachable.
- Ownership cleanup must not alter save schemas, active-profile selection, launch/return behavior, paging, draw-time I/O rules, grant semantics or resource-consumption behavior.

## Retired or deliberately removed UI concepts

Do not reintroduce these exact screens merely because older source/history mentions them. If the underlying need becomes useful, redesign it as a clear player/GM feature.

- Adventure **Campaign Diagnostics** was removed because the screen was broken and did not provide useful normal-play value.
- Adventure **Installed Pack Controls** was removed because the control screen was broken. Normal campaign-pack loading remains useful; any future Pack Library/Controls feature should be a redesigned user workflow, not restoration of the old screen.
- Bestiary **Monster Pack Controls** was removed because it did nothing useful. Pack loading remains separate from that retired menu item.
- Bestiary **Pack Diagnostics** belongs at the end of the Bestiary menu and is visible only when the shared DNDolphins **Debug** setting is On; DNDBestiary loads only the minimal two-byte Debug/Homebrew projection. Homebrew Off must exclude Custom Pack monsters from selection/generation while preserving user-created Custom monsters and saved data.

## Roadmap ideas removed because they already exist or overlap current behavior

These names are useful historical context because they can otherwise be mistakenly proposed again as "new" features.

- **Pending Progression** was removed as a broad roadmap feature because Level Choices and Level-Up Review already expose pending ASI/Feat and progression information. Future progression work must add a capability those screens do not provide.
- **Send Encounter to Initiative** was removed because Bestiary already hands generated/saved encounter participants into Initiative. Future encounter handoff work should add something beyond the existing full-encounter transfer.
- **Grant Preview** was removed because the application already has structured grant review/retry/edit behavior and the concept overlapped that workflow. The current roadmap's **Grant Review Center** is specifically about making unresolved grant review directly discoverable from a normal Character menu, not recreating existing grant mechanics.
- A generic **Timed Effects** concept was removed because it overlapped better-scoped Initiative condition-duration and Spell duration/effect tracking features. Keep duration ownership tied to the system that understands the effect.
- Hardware validation, soak testing, fault-injection, stack-high-water measurement and regression confirmation were removed from ROADMAP because they are engineering acceptance work, not user features. Use `DEVICE_TEST_MATRIX.md` and `MEMORY_AUDIT.md` for that work.

## Historically out-of-scope approaches

Treat these as strong defaults unless a future user request explicitly changes direction:

- Do not add executable campaign scripting merely for extensibility; prefer declarative campaign content and bounded metadata.
- Do not make Journal entries implicitly drive Adventure progress.
- Do not re-embed Inventory, Spellbook, Feature or applied-grant sidecars into canonical character saves.
- Do not replace bounded paging with whole-file list loading.
- Do not make save-format changes solely for validation or atomicity.
- Do not split a FAP solely for source organization. A new FAP should solve a real runtime-memory, ownership or user-workflow problem.

## Existing behavior that future features must account for

Before proposing or implementing an expansion, check README and FEATURE_CHECKLIST for these hidden/convenience behaviors because new work can easily duplicate or break them:

- Loose ammunition uses case-insensitive name-token matching, so an Item such as `Fire Arrow` can satisfy a weapon that requires `arrow`; weapon-local ammunition counters take priority when configured.
- Spell catalogs are storage-streamed, sorted by spell level/name, and support Character Classes as the default plus Any Class/individual class browsing. Eligibility and pure catalog-class browsing are separate concepts.
- Progression feat choices default to conservative Allowed filtering; custom/unknown rows remain reachable through All rather than being guessed eligible.
- Companion FAPs share the active character but keep app-owned screens/state separate. Returning to DNDolphins may refocus the originating Home option.
- Inventory catalog presets populate real weapon/armor mechanics; custom records can still be edited.
- Initiative synchronizes relevant main-character combat state and owns automatic Turn/Encounter Feature recharge behavior.
- Adventure uses one-shot guards for rewards/milestones so revisiting a scene cannot silently duplicate guarded rewards.
- Bestiary tracks Recent monsters and persistent encounter settings and can hand encounter participants to Initiative.

## Feature-expansion heuristics

When searching the code for expansion ideas, use behavior-bearing FAP-owned logic as clues to user value: automation, rules application, filtering, syncing, hidden defaults, cross-app handoff, recovery, navigation shortcuts, resource consumption and content workflows. Do not turn allocation/free routines, generic parsers, cache plumbing, drawing helpers or incidental implementation details into feature proposals.

When an existing feature is useful but limited, document the current behavior in README and put only the missing improvement into ROADMAP. Examples include a browser for already-saved combat history, choosing among multiple currently matching ammo stacks, making existing grant review directly accessible, or adding richer organization around an existing collection.

Prefer improvements that reduce table friction, repeated navigation, duplicate data entry, manual arithmetic or forgotten per-turn/per-rest state. Keep optional automation explicit when a D&D rule can vary by table or ruleset.

For any feature that adds persistence, first ask whether the state can be derived from existing character, collection, Initiative, Adventure or Journal records. If it can, derive it. If it cannot, add the smallest app-owned persisted state that satisfies the user feature and document ownership in SAVE_SCHEMA.

For any feature that touches multiple FAPs, define one owner for the authoritative state and make the other FAPs consume or bridge that state rather than creating parallel copies.

## Documentation decision rules preserved from earlier cleanup

- README: current user behavior, per FAP, actual menu order, controls and hidden conveniences.
- FEATURE_CHECKLIST: current capability-level inventory; avoid duplicating every button instruction.
- CHANGELOG: concise released changes only.
- ROADMAP: future features/improvements only; no testing chores, implemented features or implementation-defense notes. Keep detailed explanations for each planned feature.
- Technical schema/audit documents: current technical truth, not release-history narration.
- EXTRA_: AI/planning context, historical rationale, removed guardrails, retired ideas and expansion heuristics that improve future implementation decisions.

## Completed scalable-collection work and future guardrails

The former 4.19.0c WIP is historical. Version **4.19.1** is the active release line and carries forward the audited scalable collections plus the reviewed grant/catalog work. Do not restart from an older ZIP or infer current behavior from pre-3.6 grant notes.

Item, Spell and Feature logical indexes/counts are 16-bit; resident pages remain eight records. Combat retains eight filtered logical indexes and five formatted rows. Keep filtering and storage reads outside Canvas callbacks. A 32-offset seek table accelerates the first 256 owned records; later pages stream safely without allocating a larger index. Language/proficiency windows stream their sidecars using a 96-byte buffer and 128-byte line. Their asset catalogs use 256-byte reads and a 24-name selection page; this is not a catalog-total ceiling.

Spell records and all four parallel flag arrays have one allocation owner, `spell_storage`. Borrowed adapters detach every pointer before cleanup; transferred pages clear the source owner. Never individually free/reallocate any flag-array interior pointer. Ritual status must never force Known during parsing or serialization. Manual/catalog additions initialize Known explicitly.

Normal character creation does not create an Inventory sidecar. Default **Get Elevated=420** is applied after normal starting equipment is generated. Off-to-420 applies a bundle to the active character only when Inventory exists. Catalog visibility is independent: **Homebrew=Yes/No** alone admits/rejects `Homebrew`/`DNDolphins` catalog rows in either Catalog mode. A whole bundle uses one batch append and checked publication. Do not add speculative campaign variables or a Character Sheet FAP for this work.

Language/proficiency legacy core fields are dropped without migration. Live filenames are `languages_{id}.txt` / `proficiencies_{id}.txt`. SHD bundle version 2 declares six companions; version 1 declares only the original four, so a missing new companion in an old bundle must never clear a current collection.

The former roadmap training entry is now implemented as catalog-backed lists. Its remaining rule-integration opportunity is represented by Training Impact Preview in the future roadmap. Original planning text:

> 2. **Structured Training & Proficiency Sheet:** Add dedicated user-facing sections for tool proficiencies, armor training and weapon training instead of relying on one free-form Other Proficiencies field. Structured entries would make granted training easier to inspect, edit and reuse for future rules checks while still allowing custom text for homebrew proficiencies.

## Current memory-audit context

`MEMORY_AUDIT.md` is authoritative for the current 4.19.1 memory state. Older pre-3.6 app-size and grant-size tables were removed from this file because they became easy to mistake for current measurements. The current regenerated 32-bit regression proxy reports the common `DndDolphinsApp` state at **3,416 B** (down from 4,676 B; 26.9% less fixed state), `DndCharacter` **3,148 B**, `DndGrant` **180 B**, and SHD restore context **2,029 B**. Language and Proficiency owned rows remain lazy rather than resident; the shared allocation now tracks its row kind and is recreated when switching between those differently sized record types.

The highest-priority OOM discriminator is *when* the error happens. Before DNDolphins Home, prioritize FAP Loader/code residency and contiguous app/framework allocations. During Grant Review, prioritize DNDGrants grant `realloc` overlap and fragmentation. DNDCombat is now isolated as a standalone FAP; its Item/Spell attack pages remain bounded eight-record windows with independent totals.

DNDCombat must preserve active-profile handoff, keep DNDInitiative separate, stream the existing Item/Spell sidecars, and transactionally persist only combat-mutated character/resource fields. **Jump to Initiative** returns to DNDCombat only for Combat-originated Initiative sessions. Do not duplicate Inventory or Spellbook ownership.

## 3.6 grant/catalog implementation notes

- Complete catalog/support files use the `*_All.txt` suffix. These `_All` files are the catch-all `Catalog: All` assets and are the intended destination for future non-SRD findings; the matching unsuffixed files remain the SRD-facing assets. Keep this provenance rule internal to development/compliance documentation.
- Grant review is mandatory for newly eligible character grants. `?` means pending, `A` applied and `S` skipped. Apply All skips choice-bearing rows so player decisions are never guessed.
- Choice dispatch covers Language, Spell, Feat/Perk, Skill, Skill/Tool, Proficiency and Size. Dependent feat/feature grants are staged in later bounded batches.
- Review metadata uses a 24-row resident queue with byte-cursor resume and an eight-generation dependency ceiling. No grant scanner is called from the periodic tick or Canvas draw paths.
- Magic Known/knowable/free-granted totals are refreshed on screen entry and cached for drawing. Normal class-capacity selections are excluded from the free-granted count.
- DNDInventory exclusively owns starting equipment and requires a Review inventory grant confirmation; DNDolphins metadata is audited to reject `item=` grants.
- The Item catalog has 682 rows with non-empty Source fields. Ravenloft sources are split between CoS and VRGtR; structured catalog rows display compact Source tags.
- Release-gate coverage is 103 species, 36 backgrounds, 13 classes, 139 subclasses and 33 feats. Both complete spell catalogs contain 485 names (355 in the SRD-facing catalog) and every fixed spell grant must exist in both.
- Grant metadata is physically scope-separated: `metadata/options.txt` is SRD-only (1,575 total / 616 grant-bearing rows) and complete `options_All.txt` retains all 3,642 / 2,382 rows. `Catalog: All` directly selects the complete metadata file and is unavailable if it is missing. The complete file must preserve every SRD metadata row byte-for-byte at the field level.
- The built-in Class/Subclass/Species/Background/Feat name arrays are missing-asset SRD picker fallbacks only. Recognized class Hit Die/spellcasting mappings are separate runtime rules and should not be removed just because catalog names are externalized.


## Documentation visibility policy

Internal non-SRD catalog/provenance names, source-specific examples, compatibility details and split-file implementation notes may be retained in **EXTRA_.md** for future development and compliance work. Do not surface those details in README.md, CHANGELOG.md, ROADMAP.md, normal audit/policy documentation, device-test documentation or public validation summaries; those documents should describe the SRD/Homebrew-facing behavior only.

### 3.6 internal spell-catalog ownership / Artificer notes

- `character_assets/catalogs/spells.txt` and `spells_All.txt` are the canonical runtime spell catalogs. DNDSpellbook prefers `/ext/apps_assets/dndolphins/catalogs/` and keeps only a code-level fallback to the old DNDSpellbook asset root for older SD installations. New packages no longer bundle `spellbook_assets`.
- Spellbook already supports `Any`, `Cantrip`, and level `1` through `9` filtering; host regression coverage now protects this behavior.
- Artificer uses known cantrips and prepared level-1+ spells. Tinker's Magic grants Mending independently of the normal cantrip allowance. The complete spell catalog extends Artificer associations only in `spells_All.txt`, and the Mending/Tinker's Magic progression grants exist only in `options_All.txt`.
- Shared SRD spell rows remain authoritative in `spells.txt`; the complete file may extend only their class-association field. The release audit rejects changes to level, school, ritual flag, source, name, or loss of any base class association.

## 3.6.7 Full Bug Audit

Release-candidate audit additionally checked all 11 FAP manifests, production source ownership, unsafe C string APIs, conditional menu indexes, cross-FAP launch/return ownership, bounded collection/search behavior, backup/clone transaction boundaries, Initiative/Bestiary participant ownership, and 128x64 menu visibility. Concrete fixes are recorded in CHANGELOG.md.

