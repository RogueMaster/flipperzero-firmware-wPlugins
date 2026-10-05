# DNDDolphins 3.6 Grant Coverage Audit

Version 3.6 treats SRD catalog/grant coverage as a release gate rather than assuming that a selectable name is mechanically implemented.

## Current audited SRD scope

- Species: **28 / 28** SRD selectable entries have grant-bearing metadata.
- Backgrounds: **4 / 4** SRD selectable entries have grant-bearing metadata.
- Classes: **12 / 12** SRD selectable entries have grant-bearing metadata.
- Subclasses: **12 / 12** SRD selectable entries have level-triggered feature progression metadata.
- Feats: **19 / 19** SRD selectable entries have grant-bearing metadata.
- Character grant/rules metadata: **1,575 rows**, including **616 grant-bearing rows**.
- Canonical Character/Spellbook spell catalog: **355 SRD spells**.
- Fixed SRD grant payload values cross-checked against SRD catalogs/vocabularies: **475**.
- Packed files explicitly classified by the release gate: **44 / 44**.
- Item catalog: **615 physical rows** consisting of **579 SRD + 36 project Homebrew/DNDolphins** rows; Homebrew No yields the 579-row SRD view.

Coverage means a selectable SRD option is no longer metadata-empty. It does not mean DNDDolphins copies full rulebook prose. Metadata remains compact and mechanical: feature names, grant levels, choice types/options, spell identifiers, languages, skills, training and other values needed to stage a grant.

## SRD and Homebrew gates

The release audit treats SRD catalog integrity as an invariant. Normal Character, Spellbook, Inventory, starting-equipment, proficiency and grant assets are checked for SRD-valid entries. Homebrew/DNDolphins rows are independently gated by **Homebrew: Yes/No** and are never treated as SRD content.

Compiled picker fallbacks must exactly match their SRD catalog files. The packed asset inventory is explicitly enumerated so a newly bundled file cannot bypass review simply because it was added later.

## Grant behavior gates

Newly eligible grants are staged before they change character state. Pending rows use `?`, applied rows use `A`, and skipped rows use `S`. **Apply All** may apply deterministic grants, but choice-bearing rows remain pending until the player makes the choice.

Choice dispatch supports Language, Spell, Feat/Perk, Skill, Skill/Tool, equipment Proficiency and Size. Dependent grants produced by a selected feat/perk/feature are staged as later review work rather than recursively applied without approval.

Primary-class saving throws and starting languages use the same reviewed grant path. Multiclassing does not silently grant another primary-class save pair.

## Spell gates

Fixed SRD spell grants must resolve in the shared streamed SRD spell catalog. Normal class-capacity selections are distinguished from genuinely free/granted spells so the Magic screen can report Known, knowable and free/granted totals without double-counting ordinary progression.

Choice grants can constrain spell level, school and explicit option lists. The highest learnable level is derived from the level/class context that earned the grant where that rule is represented.

## Performance and persistence gates

- The resident grant-review queue is bounded to **24 rows**.
- Shared Settings reads use a 128-byte buffer and recover valid complete lines best-effort; malformed, truncated or unreadable fields retain defaults.
- Spellbook Prepared/Known/Always filtering builds a fixed **128-byte negative prefilter** once per status mode; possible matches still use exact sidecar lookup.
- Metadata staging resumes from a byte cursor rather than restarting the metadata file for every batch.
- Dependent grant expansion has an **eight-generation ceiling**.
- Persisted stable grant IDs are collision-checked at the stored **23-character** limit.
- Grant scanning is prohibited from periodic tick and Canvas draw paths by host audit.
- Magic spell aggregates refresh on screen entry and are cached; drawing the Magic screen has a regression test requiring **zero storage reads**.
- The current release audit checks **76 direct static draw helpers** for heap/storage calls.

## FAP ownership gates

DNDInventory exclusively owns starting-equipment package writes and requires explicit **Review inventory grant** confirmation. Character progression metadata is audited to reject `item=` grants so equipment writes cannot quietly migrate back into DNDDolphins.

DNDSpellbook streams the canonical DNDolphins spell catalog directly; it does not package a second copy or keep a resident catalog buffer.

## Item source audit

Structured Item rows require a non-empty Source and are alphabetized within category. SRD and project-owned source tags are validated while browsing. Homebrew/DNDolphins rows remain independently filterable and already-owned Items remain visible regardless of the Homebrew setting.

## Packed support asset gate

Starting equipment, proficiencies, Abilities/Features, alignments, languages, progression-spell support, the monster index and project campaign assets are explicitly covered by the release audit. The current packed inventory contains **44 classified files**. Compiled fallback pickers must exactly match the SRD catalogs.

## Remaining target validation

