# Feature checklist

## DNDolphins

- [x] Multiple tolerant named-field character profiles.
- [x] Active profile fallback and legacy character-file relocation.
- [x] Write-only character shadows.
- [x] Eight-record spellbook and inventory paging.
- [x] Live collection filenames are unambiguous from character profiles: `spellbook_{id}.txt` and `inventory_{id}.txt`.
- [x] Spell catalog class filtering defaults to **All Classes**, the union of currently eligible spells across every class on a multiclass character, with individual-class filtering available explicitly.
- [x] Spellbook/Inventory Add New stages a blank resident record before storage I/O, opens the full editor immediately, and persists through bounded sidecar append/save paths without rereading the new record.
- [x] Inventory-only first-time starting equipment plus hidden d100 trinket.
- [x] Item-owned weapon/armor/currency/carrying rules.
- [x] Spell-owned casting ability, DC/attack, slots/Pact/points and spell-level rules.
- [x] Dice, skills/saves and Combat spell/weapon attacks.

## Companion apps

- [x] DNDAdventure declarative campaigns, checks, rewards, flags/achievements and Journal milestones.
- [x] Bundled Reef Wardens and Ghost Protocol campaigns.
- [x] DNDJournal standalone per-character entries, newest first, with one-shot milestone class leveling and Item-entry inventory creation.
- [x] DNDInitiative standalone roster/combat state and Bestiary handoff, including full numeric participant editing, manual reordering, active-combat AC/condition controls and End Current Combat.
- [x] DNDBestiary bundled catalog, filters, encounters, custom monsters and installable packs.
- [x] Default custom Dolphin/Capybara seed only when no user custom pack exists.

## Runtime

- [x] Explicit full-path FAP launches.
- [x] Outgoing-app teardown before handoff.
- [x] Stack reservations: 6 KB / 4 KB / 4 KB / 4 KB / 6 KB.
- [x] No checksum requirement for editable text packs.

## Initiative and progression verification

- [x] Active-character initiative refresh uses current Dexterity, Initiative Misc and exhaustion.
- [x] Alert and Jack of All Trades are recognized for the active character without double-counting proficiency.
- [x] Normal / Advantage / Disadvantage can be set per participant; menu roll mode is the default for new participants.
- [x] Generated rolls respect per-participant roll mode; typed d20 results are not transformed.
- [x] Initiative ties use modifier as the first tie-breaker.
- [x] Main-character Initiative HP/AC edits synchronize back to the canonical character profile.
- [x] Turn and Encounter feature recharge are applied at their Initiative cadence.
- [x] Grant Initial Traits stages grants into Grant Review before application.
- [x] Level progression does not auto-select arbitrary spells and prompts when spell choices expand.
