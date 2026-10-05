# Device test matrix

## Launch and handoff

- [ ] Build all eleven FAPs and confirm no unresolved application symbols. All eleven must resolve active-profile/handoff calls from the shared `dnd_profile_handoff.*` module; apps that only need active-profile metadata must not pull in `dnd_storage.c` for that purpose.
- [ ] Open Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook with active IDs 0 and nonzero; confirm `[id]` appears at the top-right on each app's main screen only and disappears from detail/editor/tool/result screens.
- [ ] Launch all eleven FAPs directly: DNDolphins, DNDCharacter Sheet, DNDGrants, DNDCombat, DNDInventory, DNDSpellbook, DNDAdventure, DNDJournal, DNDInitiative and DNDBestiary.
- [ ] In Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook, press Short Back from the main screen and confirm DNDolphins is launched when `/ext/apps/Games/dndolphins.fap` exists. Temporarily remove/rename that FAP and confirm the same Short Back exits cleanly without a dead Loader handoff.
- [ ] In each companion main screen, Hold Back and confirm the companion exits back to firmware without launching DNDolphins. Confirm Initiative and Bestiary no longer contain a normal main-menu Return/Open-DNDolphins row.
- [ ] In companion sub-screens, Short Back must continue to move up one screen rather than immediately returning to DNDolphins; Initiative active-combat Short Back returns to the Initiative main menu without ending the encounter, while Hold Up handles previous-turn navigation.
- [ ] Cycle DNDolphins → DNDInventory → DNDolphins repeatedly.
- [ ] Cycle DNDolphins → DNDSpellbook → DNDolphins repeatedly.
- [ ] Cycle DNDolphins → Bestiary → Initiative → DNDolphins repeatedly.
- [ ] Cycle DNDolphins → Journal/Adventure → DNDolphins repeatedly.
- [ ] Confirm Inventory/Spellbook direct launch resolves the active canonical DNDolphins character and never creates `/ext/apps_data/dndinventory/` or `/ext/apps_data/dndspellbook/` character data.
- [ ] With `custom_active_profile.txt` containing `Active=1`, repeatedly open/close DNDInventory and DNDSpellbook both directly and from DNDolphins; every launch must show `[1]`, never transient `[0]`, and must enter the list rather than `No character`.
- [ ] On all six companion main views, confirm `[4294967295]` never appears. Exercise absent/unreadable metadata, stale IDs, repeated character changes and no-character paths; valid ID `[0]` must remain displayable. On Inventory/Spellbook specifically, confirm the draw callback and input callback agree on the same screen: a displayed `No character / OK: Open DNDolphins` screen must actually launch DNDolphins on OK, and a displayed list/detail screen must perform only that screen's actions.
- [ ] Repeatedly press Back from the Inventory/Spellbook top-level list and no-character screen; confirm the dispatcher exits promptly without an extra redraw or apparent input loop.
- [ ] In Inventory with 0–3 Items, confirm **Bag: Main <>**, **+ Add New** and **Currency** share the first viewport with available Item rows; with more Items, confirm those anchors scroll away only when selection moves into later Item rows. Repeat the original 1–5 **+ Add New** visibility check in Spellbook.
- [ ] Populate more than eight Items and Spells. Confirm Inventory shows right-aligned `PgX<>` while its transient status is clear, and Spellbook shows `Spellbook` with `PgX<>`; Up/Down Repeat remains responsive and performs storage I/O only when crossing an eight-record boundary. If using short Left/Right list page jumps, confirm each changes only one aligned eight-record page and holding does not churn through pages.
- [ ] From a later Inventory page, wrap Up/Down back to **Currency** / **+ Add New**; from a later Spellbook page, wrap back to **+ Add New**. Confirm page zero is reloaded with no `Page unavailable`, empty-page artifact or stale later-page data.
- [ ] Empty never-granted Inventory: delete/no sidecar on a new character and enter Inventory; confirm normal starting equipment is granted automatically. Then delete every granted Item while retaining `InitialInventory=1`, reopen Inventory, and confirm starting equipment is **not** duplicated.
- [ ] Inventory paging acceptance: repeatedly cross 8-record boundaries, wrap to **Currency** / **+ Add New**, open/edit/delete records and return from end actions; confirm the selected row always belongs to the resident page and no normal path renders `Page unavailable`.
- [ ] Inventory page label: with exactly 8 Items no `PgX<>` indicator is required; with 9+ Items confirm `Pg1<>`, `Pg2<>`, etc. follows the resident eight-record Item page and yields temporarily to action/status text when present.
- [ ] Inventory quantity shortcuts: on an owned Item Hold Left subtracts 5 and Hold Right adds 5, clamped to 0..999 and persisted after relaunch. Confirm short Left/Right still page the owned list and the end-action rows do not change quantity.
- [ ] Verify Spellbook list row marks: `A` always prepared, `P` prepared, `K` known, `-` neither, and `F` appended whenever a free cast remains (including `AF`).
- [ ] Repeat Inventory/Spellbook launch/return cycles with empty and populated sidecars; confirm no blank/undrawn main page appears after repeated launches.
- [ ] Force a valid `Active=1` with character 1 unreadable/missing; Inventory/Spellbook must retain `[1]` on the error/no-character screen rather than resetting the displayed ID to `[0]` or choosing another character.
- [ ] On Inventory/Spellbook error/status/detail/tool screens, confirm `[id]` is not drawn; on the main list confirm `[id]` remains correct; for Inventory with more than eight Items, confirm `PgX<>` is shown in the status/page position without leaking onto detail/tool screens.
- [ ] Confirm exact active-character selection, Initiative/Bestiary ID-0 fallback and teardown-before-launch behavior.
- [ ] From a cold boot or low-free-heap state, launch DNDolphins repeatedly and confirm Loader does not show `Not enough RAM to run the app`.
- [ ] With serial logging at debug level, capture DNDolphins Loader/Elf `Total size of loaded sections` on the exact RogueMaster build that exhibits OOM. Note whether any DNDolphins heap checkpoint appears; no app checkpoint means failure occurred before/while entering app code.
- [ ] Enable DNDolphins Settings > Debug and record `Heap after app state`, `Heap after core UI`, `Heap app ready`, and Combat-entry heap lines. Reproduce the failing path and compare free heap across repeated cycles; a monotonic decline indicates a leak/retained framework object, while a stable low baseline points to Loader/code residency.

- [ ] DNDolphins Home focus: enter and Back out of Character, Vitals, Abilities & Saves, Skills, Features & Perks, Combat and Dice Roller; each return must restore the same named Home row rather than row 0. Repeat after returning from Inventory, Spellbook, Bestiary, Initiative, Adventure and Journal and confirm each `focus=` argument resolves to its named Home index.
- [ ] Settings persistence/recovery: open **Settings** after Journal, toggle **Skip Dice Loading** and **Debug**, exit/relaunch DNDolphins, and confirm both values persist in `/ext/apps_data/dndolphins/settings.txt`. Delete the file and confirm defaults are used. Then corrupt one line while leaving another valid and confirm the valid setting is recovered while the malformed/unreadable setting keeps its default; app launch must not fail because Settings are reconstructable. During a Settings save, interrupt SD I/O/power and confirm the prior complete `settings.txt` survives and no stale `.tmp`/`.bak` remains after the next successful save.
- [ ] Skip Dice Loading: with the setting Off, verify the normal DNDolphins rolling animation appears for the general Dice Roller, Spend Hit Die, weapon attack/damage and spell-combat rolls. Turn it On and repeat; the exact same roll paths must show the resolved result immediately without the loading/rolling animation. Verify Adventure skill rolls and Initiative rolls remain immediate and continue reading the shared setting without creating per-FAP settings files.
- [ ] Debug: enable Debug, exercise Settings save, an Adventure skill roll and an SHD restore through DNDBackup & Restore, and confirm diagnostic log messages appear without changing dice totals, save data or gameplay behavior. Disable Debug and confirm those opt-in messages stop.
- [ ] SHD history creation: on a character with Inventory, Spellbook, Features and applied-grant state, save at one level, change level/state and save again. Confirm each retained level has one core `ch_<id>_<name>_<level>.shd`, matching sidecar SHDs only for sidecars that existed at that level, and the internal `_bundle.shd` completeness marker; live `.txt` files remain authoritative.
- [ ] DNDBackup & Restore → **Restore Backup**: verify the active profile is shown, the native `.shd` browser selects an exact core snapshot from the configured backup folder, and restoring a current bundled level replaces the canonical character plus Inventory/Spellbook/Feature/applied-grant/Language/Proficiency live sidecars with that level's matching snapshots. With a `_bundle.shd` marker, a sidecar absent in the selected snapshot must become absent/empty rather than retaining later data.
- [ ] Legacy SHD compatibility: restore a pre-bundle core-only `.shd` while current Inventory/Spellbook/Feature/applied-grant sidecars exist. Confirm the character core restores but current sidecars are preserved because the older SHD did not record collection presence.
- [ ] SHD rollback fault injection: fail the core publish and each sidecar copy in turn. Confirm the restore reports failure and attempts to leave the pre-restore canonical core and all four live sidecars intact; temporary rollback work files must be cleaned afterward.
- [ ] Combat menu order: confirm Weapon Attacks, Spell Attacks, **Spell <ability> Atk+X DCY**, Rituals and Attack Templates appear in the attack section; Initiative Tracker is absent from Combat, and the state-free section header changes correctly across Attacks, Encounter, Recovery, Status and Defenses without allocating or reading storage during redraw. Short OK on the spellcasting-stat row opens Magic.
- [ ] Combat cantrips: add a Known combat-mapped cantrip and a Known utility/non-combat cantrip with no structured combat effect. Confirm the combat cantrip appears in Spell Attacks at zero slot cost across scaling thresholds, while the utility cantrip remains in Spellbook but is not forced into Spell Attacks.
- [ ] Grant status/review rendering: select **Grant Initial Traits** and **Apply Level Grants** on representative profiles. Confirm `Applying` is visibly rendered for at least one UI tick before the bounded scan, then **Review grants before apply** appears for newly eligible rows. Confirm deterministic rows remain `?` until OK/Apply All, applied rows become `A`, choice rows identify the choice type, and failures/skips remain visible instead of being silently committed.
- [ ] Level-up review: level a character through a proficiency-bonus threshold, a deterministic-feature level, a spell-choice-growth level and an ASI/Feat level. Confirm the bounded review reports only the applicable before/after changes/notices and that reopening normal character screens does not retain a progression catalog in RAM.
- [ ] Profile projections: compare steady-state free heap for Inventory, Spellbook and Adventure against the prior full-character builds. Use a canonical profile containing maximum-length percent-encoded text fields; apply Inventory AC/encumbrance/carry-capacity changes and byte-compare all unrelated canonical lines before/after. Spellbook and Adventure must never rewrite the canonical character file during ordinary use.

- [ ] Launch DNDolphins on an ordinary healthy profile and confirm the home header does not persist `Loaded`; the character name is visible. Then exercise a backup recovery or forced save/read error and confirm meaningful recovery/error status still appears.
- [ ] Weapon Combat: test STR, ranged DEX and finesse-best weapons; proficient/unproficient and magic bonuses; advantage/disadvantage; natural 1/20; ammunition decrement/persistence; versatile damage; extra dice; and critical damage-dice doubling.
- [ ] Structured Spell Combat: verify at least one fixed-value heal, Aid at base and higher slot level, a multi-attack spell, and a spell with a secondary effect; confirm upcast deltas/roll-instance counts are deterministic and the 168-entry table still falls back to Notes `XdY` only for unmapped spells.
- [ ] Spell Combat: test a cantrip across character-level scaling thresholds, an upcast mapped spell, a Notes-only `XdY` fallback spell, a spell attack and save spell, plus available normal slot, Pact slot, spell points, free cast and ritual options. Confirm the selected resource is consumed/preserved correctly and results use the spell's source-class casting ability. For a Wizard specifically, confirm an unprepared level-1+ spell with no Free Cast is absent; the same spell appears once Prepared/Always Prepared; and an unprepared spell with a Free Cast appears but offers **Free Cast only**. Wizard cantrips remain available without preparation.
- [ ] Combat → Rituals / Ritual Adept: on a Wizard, add known level-1+ Wizard spells with and without the Ritual tag and with Prepared both on and off. Confirm only known Wizard Ritual entries appear, preparation does not matter, cantrips/non-Wizard rituals are excluded, selection reports `Ritual cast: +10 minutes`, and slot/Pact/points/free-cast counters do not change. Repeat with more than eight Spellbook records to exercise bounded paging.

## Stability / heap / draw audit

- [ ] Build with allocator/free-heap instrumentation if available. Repeatedly enter/exit Profiles, Journal, Inventory, Spellbook, Weapon Attacks, Spell Attacks, Rituals, Adventure and Bestiary screens while forcing redraw/scroll. Confirm free heap returns to the same steady-state range after leaving each screen and does not decrease monotonically across cycles.
- [ ] Confirm redraw alone does not cause SD activity: hold/move through screens that only redraw resident data and verify storage reads occur on screen entry, explicit input/cache boundaries or writes—not from the canvas callback.
- [ ] Measure actual device heap and stack high-water across the representative paths in `MEMORY_AUDIT.md`; include page-transfer overlap and firmware/framework allocations, which host layout figures do not measure.
- [ ] Repeat Bestiary allocation-failure tests at startup/window/detail/encounter creation and confirm partially allocated project blocks are released without a later cumulative heap loss.
- [ ] Run stack high-water checks under the manifest reservations (6/4/4/4/4/4/6 KB) and compare with the ARM compiler measurements and working-set arithmetic rather than shrinking a stack solely to reduce Loader pressure.

## Character / Inventory / Spells

- [ ] Create a character and confirm character creation alone does not create an Inventory sidecar; open DNDInventory and confirm a truly empty, never-granted Inventory receives its starting package and `InitialInventory=1`.
- [ ] On an ungranted Inventory, select **Grant Initial Inventory** and confirm **Review inventory grant** appears before any package is written. After OK applies it, confirm the grant state reports Granted and **no extra trinket** was added when normal class/species/background defaults matched.
- [ ] Force a normal starting-equipment composition with no matching Items/currency and confirm the automatic grant uses one random d100 trinket fallback and records the one-shot grant marker.
- [ ] If normal equipment and trinket fallback both cannot be written, confirm manual Add New remains usable and can establish the canonical Inventory sidecar.
- [ ] Reopen Inventory and confirm no duplicate starting items/currency are added automatically.
- [ ] Inventory Resources, Weapon Attacks and Adventure do not create or seed missing inventory.
- [ ] In Spellbook, short OK on **+ Add New** creates a blank spell and immediately opens its full editor; short OK on the Name field opens the spell catalog and hold OK on Name allows a custom name.
- [ ] In Inventory, short OK on **+ Add New** creates a blank item and immediately opens its full editor; short OK on the Name field opens the item catalog and hold OK on Name allows a custom name.
- [ ] Hold OK on **+ Add New** in Spellbook/Inventory follows the same blank-record/full-editor path rather than becoming a no-op.
- [ ] After automatic Inventory initialization or manual Item Add, confirm `/ext/apps_data/dndolphins/inventory_{id}.txt` exists; after the first actual Spell Add/save, confirm `/ext/apps_data/dndolphins/spellbook_{id}.txt` exists. Both sidecars remain distinct from `ch_{id}_{name}_{level}.txt` profiles.
- [ ] Add three Items consecutively without leaving Inventory; confirm all three appear, the file contains three valid `I|` records, then delete the middle item and confirm it stays deleted after app restart.
- [ ] Add three Spells consecutively without leaving Spellbook; confirm all three appear, the file contains three valid `S|` records, then delete the middle spell and confirm it stays deleted after app restart.
- [ ] Exercise the 8→9 Item and Spell boundary; confirm the first eight records persist before the next resident page is opened and the ninth record survives restart.
- [ ] With generated starting equipment already present, add three Items in succession. After each Add New, load a catalog item, Back to Inventory, confirm the new item remains visible/focused without restarting, and verify the live inventory file already contains the final catalog-populated record.
- [ ] Exercise starting-equipment tail sizes around an eight-record boundary (especially 7→8→9 and 15→16→17). Confirm no MPU fault, no `<read error>` rows, and no render-time pause/storage access while scrolling between pages.
- [ ] After editing an existing Item/Spell by catalog, text input, numeric input, or left/right adjustment, inspect the live sidecar before leaving the app and confirm the change is already present.
- [ ] After a successful Item/Spell save, add, catalog choice, Equip/Prepare action or grant/regrant, confirm the success notice is visible initially and clears on the next real input; force a write failure and confirm `UNSAVED`/error feedback does not clear as a routine success notice.
- [ ] Add at least three items and three spells consecutively, close/relaunch the respective collection FAPs, and confirm every record persists.
- [ ] Delete a middle item and middle spell, close/relaunch the respective collection FAPs, and confirm the remaining records persist in order.
- [ ] In Spell Filters, confirm Class defaults to **Character Classes**; on a multiclass character it shows the union of the character's spell lists, and Left/Right also reaches Any Class plus every supported class even when the character does not own that class.
- [ ] In Add Spell, exercise Level, School, Ritual and Source filters separately and in combination; confirm each page is filled from matching streamed results rather than showing sparse rows from an already-selected page.
- [ ] Cycle the bundled SRD Source filter and confirm selected spell Source/School/Ritual metadata is copied into the owned Spell record and survives restart.
- [ ] Hold OK on a known Spellbook row and confirm Prepared toggles immediately, the `S|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back; Always Prepared remains unchanged.
- [ ] Hold OK on an Inventory row and confirm Equipped toggles immediately, the row marker updates, the `I|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back.
- [ ] Simulate/retry after an SD write failure and confirm an interrupted append does not leave a partial spell/item record or permanently disable later Add/Delete attempts.
- [ ] Exercise >8 items and >8 spells across page boundaries.
- [ ] Verify spell attack/DC, slots/Pact/points and weapon attack/damage behavior.


## Inventory actions / initial grant

- [ ] With no `inventory_{id}.txt` and no prior grant marker, open DNDInventory and confirm no starting package is silently created. Select **Grant Initial Inventory**, confirm the review screen, cancel once with Back, then approve with OK and verify the package is created exactly once.
- [ ] Confirm Inventory has no hidden Hold-Up tools menu: **Bag: <name> <>** is first, **+ Add New** second and **Currency** third; **Inventory Resources** and **Grant Initial Inventory** are the final action rows, and Hold Up on the list performs no alternate action.
- [ ] Confirm Hold OK gestures: Inventory row = Equip/Unequip; + Add New = blank full editor; Item Catalog = category filter.
- [ ] Exercise all five Currency fields with Left/Right and direct numeric entry, restart, and confirm values persisted.
- [ ] Add a `Currency=` line to a character profile with no Inventory currency record and confirm DNDInventory ignores it; only `inventory_{id}.txt` may supply persisted currency.
- [ ] Exercise Inventory Resources: encumbrance toggle, capacity override, armor/shield AC application and coin normalization.
- [ ] After approving the initial Inventory grant, confirm `inventory_{id}.txt` contains the granted Item rows, expected `Currency=cp,sp,ep,gp,pp` total and `InitialInventory=1`. Reopen Inventory and confirm no duplicate package is added. Short OK on **Grant Initial Inventory** must review/report the already-granted state without duplication; Hold OK must show the one-time regrant review before appending the deliberate regrant and publishing `InitialInventory=2`; a second Hold OK adds nothing.

- [ ] Character → Level Choices: verify it always opens. At a level with no pending ASI/Feat, confirm the explicit “No pending choices” screen; at an ASI/Feat level, confirm the choice screen and Allowed feat catalog default.
- [ ] High Elf grants: on a fresh level-1 High Elf, run Grant Initial Traits and confirm Prestidigitation is written immediately. Raise total level to 3/5 and confirm Detect Magic/Misty Step are **not** auto-written; run Apply Level Grants and confirm they appear. Delete one deterministic granted spell while leaving its applied marker, re-run the appropriate grant action, and confirm the missing spell is repaired once without duplicating present species spells.
- [ ] Weapon Combat loose ammunition: with Longbow + Arrows and Light Crossbow + Bolts using zero per-weapon ammo counters, confirm each attack decrements a matching Inventory stack and refuses only at stack quantity 0. Rename/add stacks such as Fire Arrow, Silvered Arrow and Crossbow Bolt Bundle and confirm case-insensitive token-anywhere matching works. Confirm a weapon with explicit ammo_max/current continues to consume its internal counter instead.

## Adventure

- [ ] In an active scene, Hold OK opens the full-text viewer; Up/Down scroll one line, Left/Right page, Short OK/Back returns without changing the selected adventure choice. Confirm normal scene preview is 22 characters wide.
- [ ] Hold Right saves the current scene checkpoint; move elsewhere and Hold Left loads it. From Campaigns choose Restart Current Adventure, confirm default selection is Cancel, then confirm Restart resets scene, checkpoint, quest flags and achievements only after explicit confirmation.
- [ ] Adventure Campaign menu: confirm Campaign Diagnostics and Installed Pack Controls are absent; verify campaign selection, restart and bundled/enabled campaign discovery.

- [ ] Reef Wardens loads and completes.
- [ ] Ghost Protocol appears as bundled content and loads `audit_brief`.
- [ ] Exercise successful and failed checks through several branches.
- [ ] Confirm rewards are granted once when guarded and milestone journaling does not intentionally duplicate.
- [ ] Confirm Ghost Protocol contains no external/device-operation dependency.

## Bestiary

- [ ] Fresh app data: Dolphin and Capybara appear as custom monsters after first launch.
- [ ] Existing custom pack: launch does not replace or merge the default seed.
- [ ] Partial custom files: launch preserves them for existing recovery/manual repair behavior.
- [ ] View/edit/delete custom monsters, install/enable packs and generate encounters.
- [ ] Send individual and generated monsters to Initiative.
- [ ] Bestiary filters: set Search, Max CR, Type, Source, Environment and Role away from defaults, then press Left on each filter row and confirm it resets to empty/Any/default immediately; Right continues to advance/cycle filters normally.

## Stress

- [ ] Repeated launch/back cycles without heap growth or crash.
- [ ] Alternate DNDInventory and DNDSpellbook launches for at least 25 handoff cycles and confirm DNDolphins continues to load each time.
- [ ] Large profile and Journal counts.
- [ ] Large monster/campaign indexes.
- [ ] Maximum-size encounter save/rename/delete paths.

- [ ] New character starts STR 15 / DEX 14 / CON 13 / INT 12 / WIS 10 / CHA 8; existing profiles retain their saved ability scores.
- [ ] Selecting class/species/background at level 1 does not auto-grant traits. Character > **Grant Initial Traits** must stage starting grants for review, including Common/starting language choices and the primary-class save pair. Confirm nothing changes until reviewed; then approve/choose them once. Raise a level and confirm no new Feature/spell appears until **Apply Level Grants** is selected and reviewed.
- [ ] Increasing a class level updates Hit Dice and applicable spell/resource progression, XP floor, and deterministic class features without rereading progression metadata outside that action.
- [ ] Initiative Start New Combat opens setup with Roll for All, per-member roll, short/repeat left-right roll adjustment, hold-OK full participant editing, hold left/right participant reordering, + Temporary Member, and Begin Combat.
- [ ] Change the active character's Dexterity, Initiative Misc, exhaustion, name, HP and AC in DNDolphins; launch Initiative and verify the existing main-character roster/combat entry refreshes without duplicating or changing monster/temp modifiers.
- [ ] Set Initiative Roll to Normal, Advantage and Disadvantage; verify Roll for All and individual automatic rolls use the selected mode, while a directly edited Initiative total remains unchanged until that participant is rolled again.
- [ ] Initiative Hold OK in combat opens participant editing including name, roll/modifier, AC, HP, conditions and delete. Short Back returns to the Initiative main menu without ending combat; Resume returns to the same encounter. Hold Up moves to the previous turn and correctly crosses from round N turn 1 to round N-1's final participant.
- [ ] Bestiary full-stat-line view wraps at up to 26 characters without the old 20-character buffer truncation.
- [ ] Adventure campaign selection shows campaign names, falling back to campaign ID only when a name is absent.

## Adventure campaign selection

- [ ] Put a valid campaign pack in the inbox and confirm Preview shows name, pack/app compatibility and entry scene before installation; Hold OK installs only when validation passes. Interrupt installation during the second file copy and during registry/index publish; confirm no partial installed path blocks retry, the prior registry/index remains usable, and the unchanged inbox can be installed successfully afterward.
- [ ] Test malformed index, missing `scenes.txt`, missing declared entry scene, incompatible min/max app range and duplicate campaign ID; each must refuse installation without deleting existing campaign content.
- [ ] With a large campaign index, navigate rows before and beyond the sparse-hint window and confirm names remain correct without a campaign-sized heap allocation.
- [ ] From a Journal milestone entry choose **Continue active Adventure** and confirm Adventure opens the persisted active campaign/current scene directly; if no valid active campaign exists, confirm it falls back safely without creating progress.

- Open DNDAdventure with the bundled campaigns present and confirm **Reef Wardens** and **Ghost Protocol** are visible immediately in the campaign list.
- Scroll/wrap through campaign rows and confirm labels remain visible while selected and unselected.
- Enter a campaign, return to the campaign list, and confirm names remain visible without an SD-read/render stall.
- Toggle/install campaign packs and confirm the visible campaign rows refresh after the campaign index changes.

## Initiative / progression additions

- Change the active character Dexterity, Initiative Misc, exhaustion, Alert/Jack-of-All-Trades state where applicable; relaunch Initiative and confirm the main-character modifier refreshes without changing monster/temp modifiers.
- Give different participants Normal, Advantage and Disadvantage; verify Roll for All and single generated rolls use each participant's own mode.
- Hold OK to open full participant editing and enter a numeric Initiative total; confirm that total is preserved until the participant is rolled again.
- Create tied initiative totals with different modifiers and confirm the higher modifier sorts first.
- On a fresh level-1 character, use Grant Initial Traits and confirm starting grants are staged in Review grants before apply rather than committed immediately. Exercise Apply All plus at least one Language/Spell/Feat/Proficiency choice. After later level increases, use Apply Level Grants and confirm the next bounded review batch resumes without a long repeated scan.
- Increase a caster level across a cantrip/prepared allowance increase and confirm deterministic progression updates while the status asks the player to choose spells rather than adding arbitrary spells.

### Initiative no-character / profile resolution

- With no DNDolphins character files present, launch DNDInitiative directly: confirm the no-character screen appears, **Launch DNDolphins** launches the main app, **Exit Initiative** exits, and no Initiative `ch_0.txt` sidecar is created.
- With no `custom_active_profile.txt`, create character ID 0 plus its Inventory and Spellbook sidecars, then launch Initiative and Bestiary: confirm both select ID 0 as the default metadata ID; Initiative validates only the primary character file and the sidecars are never selected as the profile.
- Change the active character's Dexterity, Initiative Misc, exhaustion, HP, AC, and name; reopen Initiative and confirm only changed values are persisted. Reopen again without changes and confirm behavior is unchanged.
- Repeat direct launches and DNDolphins/Bestiary handoffs with a nonzero active character ID; confirm both apps select that exact ID.
- Set `Active=7` while removing canonical character 7 but leaving character 0 present; confirm Initiative reports no character rather than switching to 0, and Bestiary continues to show/use `[7]`.
- Give the Bestiary-to-Initiative payload a different leading ID than `Active`; confirm Initiative keeps the persisted active profile while importing the transferred monsters.
- Remove `custom_active_profile.txt` or make its `Active` value unreadable, leave profiles 0 and 1 present, and confirm Initiative and Bestiary select only ID 0 rather than discovering ID 1.

### Startup-order stress

- Repeatedly launch Bestiary with fresh/default, existing, and partially populated custom-monster storage and confirm no startup OOM or callback-before-init behavior.
- Repeatedly launch Adventure with multiple campaign packs enabled/disabled and large indexes; confirm campaign names remain visible and no startup OOM occurs.

## Level choices / campaign packs

- Level Fighter from 3→4 and verify Character > Level Choices offers ASI/Feat; apply +2 and confirm the score caps at 20 and the choice does not reappear.
- Apply +1/+1 and verify the same ability cannot be selected twice. Back out before the second pick and verify no score changes were committed.
- Choose Feat, back out of the catalog, and verify no blank feature remains and the choice stays pending. Then choose a feat and verify the choice is recorded once.
- Verify Fighter 6/14 and Rogue 10 produce their additional choices; multi-level jumps expose each unclaimed choice sequentially.
- On an installed Adventure pack row, verify short OK does nothing; Hold OK toggles Active/Inactive in both directions. Confirm the registry entry and all campaign content files remain present.

- [ ] Bestiary Monster Packs: short OK on an existing pack does nothing; Hold OK toggles Active/Inactive in both directions; confirm the registry row and installed monster pack files remain present.
- [ ] Bestiary Monster Packs: short OK on the inbox/install row still installs a valid inbox pack.
## Lazy progression sidecars

- [ ] On an older character containing embedded Features/Grants but no progression sidecars, launch repeatedly and confirm Home opens without OOM, no `feats_{id}.txt` / `appliedgrants_{id}.txt` is created merely by loading, and the embedded rows are ignored. Then apply a new deterministic grant and confirm only the required current-format sidecar(s) are created.
- [ ] Open a character with more than eight Features and page through the Features list; confirm only the visible eight-record page is required and edits persist after relaunch.
- [ ] Spend a Feature, then Short/Long Rest as appropriate; confirm `feats_{id}.txt` updates and the use count survives relaunch.
- [ ] In Initiative, advance a Turn and start/end encounters with Turn/Encounter-recharge Features; confirm the Feature sidecar recharges without requiring DNDolphins to remain open.
- [ ] Level a High Elf through total character levels 1/3/5 and confirm Prestidigitation, Detect Magic and Misty Step are granted once at the appropriate gates. Repeat representative Drow/Wood Elf, Tiefling, Aasimar, Dragonborn and Goliath checks.
- [ ] On a multiclass character, confirm species progression follows total character level rather than the level of any individual class.
- [ ] Re-run progression checks after the grants are applied and confirm `appliedgrants_{id}.txt` prevents duplicates.
- [ ] In Item Catalog, Hold OK opens **Catalog Filter**. Confirm the picker contains All, Weapons, Armor, Ammunition, Gear, Tools, Instruments, Trinkets, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic; OK applies the selected filter and returns to the catalog. Confirm Instruments includes the generic Musical Instrument plus its listed variants, Trinkets exposes all 100 bundled compact trinket labels, Tea Set appears under Gear, Magic remains an aggregate non-Mundane filter, and Inventory-list Hold OK remains Equip/Unequip.
- [ ] Cross-check each exact Item name emitted by bundled class/background/trinket starting-equipment assets against Item Catalog search; every grant-owned name must have a catalog entry, including DNDolphins convenience aliases where the stored grant name differs from the SRD catalog label.
### Stack-reservation validation

- [ ] Stress DNDInventory Add/Edit/Delete, Currency/Resources, Grant Initial Inventory and multi-page catalog navigation under the restored 4 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] Stress DNDSpellbook Add/Edit/Delete, all catalog filters, Hold-OK Prepare and multi-page navigation under the restored 4 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] From DNDolphins, switch to a non-first character and launch DNDInventory; confirm the main header shows that exact active character ID and the matching `inventory_{id}.txt` contents.
- [ ] From DNDolphins, switch to a non-first character and launch DNDSpellbook; confirm the main header shows that exact active character ID and the matching `spellbook_{id}.txt` contents/class filters.
- [ ] Launch every companion directly from Apps; confirm each reads `custom_active_profile.txt` without directory discovery or launch-argument override. With a valid `Active=<id>`, confirm the exact ID is used. With metadata absent/unreadable, confirm Inventory/Spellbook/Journal/Adventure show no character and Initiative/Bestiary select ID 0. With metadata present but pointing to a missing character, confirm Inventory/Spellbook/Journal/Adventure/Initiative do **not** switch to ID 0 or another character, while Bestiary keeps the persisted ID and remains usable.
- [ ] Stress DNDInitiative full participant editing, reorder, repeated Turn/Encounter recharge, save/reload and combat navigation under the 4 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] Leave DNDAdventure/DNDJournal/DNDolphins/DNDBestiary at their larger reservations unless device high-water measurements demonstrate additional safe margin.

### Inventory / Spellbook direct-entry checks

- Launch DNDInventory from DNDolphins with a populated Inventory: the first frame is the Item list with **Bag: Main <>** row zero, **+ Add New** row one and **Currency** row two; owned Items follow and **Inventory Resources** / **Grant Initial Inventory** are at the end. Hold Up has no hidden Inventory Tools action.
- Launch DNDInventory with no Inventory sidecar: the first frame shows **Bag: Main <>**, **+ Add New**, **Currency**, then the end actions; opening alone does not create the Main sidecar. Adding the first Main Item creates Inventory-owned `Currency=0,0,0,0,0`.
- Launch DNDSpellbook from DNDolphins with a populated Spellbook: the first frame is the Spell list with `+ Add New` row zero.
- Launch DNDSpellbook with no Spellbook sidecar: the first frame is an empty Spell list with `+ Add New`; opening alone does not create the sidecar, and the first saved Spell creates it.

### Return focus / paging performance / ordering

- [ ] From each companion main screen, Short Back and confirm DNDolphins opens with the corresponding home row already highlighted: Inventory, Magic & Spells for Spellbook, Journal, Adventure, Bestiary and Initiative. Repeat with Hold Back and confirm it exits to firmware instead of launching DNDolphins.
- [ ] Confirm the DNDolphins Home menu order is Characters, Character, Vitals, Abilities & Saves, Skills, Features & Perks, Inventory, Magic & Spells, Bestiary, Initiative, Combat, Dice Roller, Adventure, Journal, Settings. Enter and return from each internal submenu and companion FAP; confirm the same named row is restored rather than a stale numeric position.
- [ ] In DNDolphins, highlight each internal home row that opens a submenu (Profiles, Character, Vitals, Abilities, Skills, Magic, Features, Combat and Dice), enter it, then Short Back; confirm Home returns to the same highlighted row/scroll position rather than row 0.
- [ ] Open Initiative and confirm the title bar is dark on the main menu and during Combat; `[id]` is right-aligned on the main menu and compact `R# T#/#` replaces it during Combat.
- [ ] Fill Initiative to the maximum roster. In Roster, Setup, Combat and participant Edit, move through every row and wrap both directions; the selected/current participant must remain inside the visible window. Start/Resume, next turn and Hold Up previous turn must recenter the active participant without changing roster capacity or encounter state.
- [ ] Open Item Name Catalog and compare with the established presentation: category initials are **not bracketed**, magic entries append `*`, Other entries show only their names, and the header shows `Page N <>`.
- [ ] Page repeatedly forward/back through Item and Spell Name catalogs with restrictive and broad filters. Confirm later pages remain responsive and correct, Left/Right never skips or duplicates filtered entries, and changing a filter resets the learned page offsets safely.
- [ ] Populate exactly 7/8/9 and 15/16/17 owned Items/Spells. Cross the 8-record boundaries repeatedly and confirm the correct records appear without blank/stale pages; after an edit/delete/rewrite, confirm the next page load remains correct after offset invalidation.
- [ ] Create an intentionally unsorted Spellbook containing mixed levels and names. Open Spellbook and confirm the persisted/displayed order becomes level ascending, then case-insensitive alphabetical within each level. Add a spell, rename a spell and change a spell level; Back out of the editor and confirm ordering is restored without losing Known/Prepared/Always Prepared/free-cast metadata.
- [ ] Build all companion FAPs and confirm Spellbook sorting resolves entirely from `dndspellbook_collection.c`: `dnd_storage.c/.h` must expose no Spellbook-sort symbol, and DNDolphins/Inventory/Adventure must not require any Spellbook-sort implementation.
- [ ] Repeat the Inventory/Spellbook paging/catalog tests while monitoring device stack high-water/free heap. Confirm 4 KB stacks remain stable and the bounded offset caches do not show launch-to-launch heap growth.

### Inventory / Spellbook parity acceptance

- Add a new Item and Spell. Confirm `Item added` / `Spell added` appears once in the editor header and clears on the next Short/Repeat/Long input without a timer or background worker; an UNSAVED/error notice must not be auto-cleared as a success notice.
- Add a blank Spell and add a Spell from the catalog; confirm both newly created records are **Known** immediately, persist as Known after relaunch, and are not automatically Prepared unless another rule/action sets Prepared.
- Spellbook main list: Hold Up opens Spell Filters. Confirm `Class: Character Classes` is the default. Left/Right must cycle Character Classes → Any Class → the bundled SRD class filters regardless of the active character's classes; Character Classes returns the union of the character's actual spell lists.
- Spell Filters: confirm `Eligibility: Allowed` is default and `All Spells` is opt-in. Allowed must require real character list/level access; All Spells must preserve the selected class as a catalog-membership filter while bypassing character eligibility. `Any Class + All Spells` should show all matching bundled SRD rows before the other explicit filters.
- Spell and Item Catalogs: confirm `Page N <>` is visible; Left/Right changes catalog pages; Item rows use unbracketed category initials, append `*` for magic entries, and leave Other entries unprefixed.
- Hold OK on a known non-always-prepared Spell and on an Item: confirm immediate persistence and a temporary `[X]` prefix on the affected row; the acknowledgement clears on the next input.
- Reconfirm full editor parity against the recovery baseline: 17 Spell fields and 36 Item fields, Name-field catalog, Hold-OK custom name, Delete, free-cast controls, Equip/Prepare quick actions and A/P/K/F Spell list marks.

## Scroll catalog acceptance

- Open Inventory -> Add New -> Name catalog, cycle to **Scrolls**, and confirm exactly the generic Spell Scroll (Cantrip) plus Spell Scroll (Level 1) through Spell Scroll (Level 9) rows are selectable. Confirm rarity is Common for Cantrip/L1, Uncommon for L2–3, Rare for L4–5, Very Rare for L6–8, and Legendary for L9; confirm no bundled per-spell Scroll rows appear.
- Page the Scroll filter beyond page 64 and back through nearby pages; confirm the rolling 64-page seek window keeps all generated Scroll pages reachable and sequential paging remains correct.
- Confirm generated Scroll rarity presentation treats levels 0–1 as Common, 2–3 as Uncommon, 4–5 as Rare, 6–8 as Very Rare and 9 as Legendary where rarity/magic metadata is surfaced.
- Confirm selecting a generated Scroll does not fabricate or overwrite Item weight, currency, Detail or another field with a GP price; Item value is not part of the current schema.

## Feat, Inventory and Initiative-history acceptance

- **Feat Allowed default:** trigger a progression feat choice; confirm Allowed is initial mode, Hold OK switches to All, and returning to Allowed restores prerequisite filtering. Test Grappler below/above prerequisites, Fighting Style, Epic Boon and an already-owned non-repeatable feat.
- **Stack Quantity:** edit Stack Qty through normal Item OK entry at 0, 1 and 999; verify Hold Left/Right list shortcuts and persistence after cold reopen.
- **Containers:** create nested/container references, delete a lower unrelated item and then the container; verify indexes shift and children become Carried rather than pointing at another item.
- **History opt-in:** End Without History creates no file. Save History creates exactly one timestamped record; verify date, round, all party states and surviving opponents, then fault the write and confirm combat remains active.
- **Declarative content:** complete at least one branch of Torii Between Tides and Moonlit Market and inspect each new folklore-inspired monster stat screen.

- [ ] Level HP/Hit Dice: for d6/d8/d10/d12 classes with several Constitution modifiers, increase one and multiple levels and confirm HP gains are 4/5/6/7 + CON per level (minimum 1), current HP gains the same amount without erasing prior damage, class Hit Dice current=max=class level, and global Hit Dice current=max=total level. Repeat through a Journal milestone.
- [ ] Bestiary home menu order: Browse Monsters, Generate Encounter, **Custom Encounter**, Party Level, Party Size, encounter settings, Saved Encounters, browse settings/lists, Create Custom Monster, Pack Diagnostics. Confirm Monster Pack Controls is absent and Pack Diagnostics is last when Debug is enabled.
## Grant/delete focused hardware checks

- On a fresh Human Fighter, run **Grant Initial Traits**: confirm the review contains starting language/save/Feature grants. Apply deterministic rows, resolve every choice, verify Second Wind and the selected languages/saves, then rerun and expect **No new grants**.
- On a leveled character with unapplied grants, run **Apply Level Grants**, approve deterministic rows and resolve choices, then verify the resulting Features/species/subclass spells. Rerun and expect **No new grants**.
- Delete a non-active character, an active nonzero character, and active profile 0. Confirm the row disappears, a surviving character becomes active when present, and deleting the final profile leaves only **+ New Character**.


## Catalog/paging/ASI focused hardware checks

- Open Add Spell with no level filter and page through transitions between Cantrip/1st/2nd/etc.; confirm levels never decrease and names are alphabetical within a level. Repeat with class/source/status filters and confirm the retained order is still level then name.
- Trigger a progression feat choice. In **Allowed**, confirm bundled valid feats appear, unmet prerequisite feats and already-owned non-repeatable feats do not, and custom/ability Feature names are absent. Hold OK to **All** and confirm custom/ability Feature rows become reachable except `Ability Score Improvement`, which remains excluded from the nested feat picker because ASI uses the dedicated +2/+1+1 choices.
- Exercise Inventory at 7/8/9 and 15/16/17 records, including delete on a page boundary and return from Item Editor. Confirm no `Page unavailable` text appears and the selected row always maps to the correct Item.
- Choose ASI +1/+1. Record all six scores before the first pick; verify the first pick changes none. Pick a second different ability; verify exactly those two scores increase by one and the other four remain byte-for-byte unchanged. Repeat across two separate pending ASIs to confirm no prior first-pick state carries over.

## Spell-class-filter focused hardware checks

- [ ] Open Spell Filters on a single-class character and confirm the initial class row reads `Class: Character Classes`.
- [ ] Cycle right through Any Class and all bundled SRD class labels; confirm classes absent from the character are still selectable.
- [ ] With a Bard that has no Wizard access, select Wizard + Allowed and confirm no Wizard-only spells leak through; switch to Wizard + All Spells and confirm Wizard catalog rows appear.
- [ ] Select Any Class + All Spells and confirm all matching bundled SRD rows remain browseable.

## Scalable collection and training device regression

- [ ] Add at least 40 Spells, 40 Items and 30 Features; test records 18–24 and later pages, then repeat with a fixture above 255 records.
- [ ] Preserve Known/Prepared/Ritual/Always Prepared/free-cast values through add, delete, sorting, page navigation and reboot, including Disguise Self, Find Familiar, Identify and Sacred Flame.
- [ ] Confirm manual and catalog spell additions are Known and succeed regardless of spells-known allowance.
- [ ] Confirm Spellbook/Features page indicators are visible and page controls work; no file access occurs in Canvas callbacks.
- [ ] Exercise Languages/Proficiencies add, replace, delete and page navigation; verify Allowed/All and long names. Load an old character and confirm no legacy training/language migration occurs.
- [ ] Verify all primary-class save pairs, manual changes and multiclass behavior.
- [ ] Exercise all six accessory outcomes, quantity bounds, repeated-strain wrap and exact final names. Confirm Homebrew No hides the 420 filter/results while Get Elevated Off does not hide catalog rows and owned items remain.
- [ ] Toggle Off to 420 with existing Inventory and with no Inventory; the latter must still receive normal starting equipment later.
- [ ] Interrupt failed writes/renames in a controlled SD test; verify whole-bundle publication and preservation of previous live collections.
- [ ] Duplicate/export/import/archive/delete a character with all six fixed sidecars plus Group and at least two named Inventory bags. Confirm every bag stays separate. Restore a v3 SHD and confirm Main/Group/named bags match that level; restore version-1 and core-only SHD history and confirm older snapshots preserve bag state they never recorded.

- [ ] **Grant performance stress:** use a level-20 multiclass/profile with many species/background/class/subclass/feat grants. Confirm opening a review batch is a bounded one-time scan, applying a batch does not freeze on repeated whole-file rescans, the next batch continues promptly, and no draw/tick slowdown occurs while simply leaving the review/Magic screens visible.
- [ ] **Magic totals draw path:** open Magic with a large Spellbook and confirm Known / knowable / free-granted totals appear. Leave the screen idle through multiple marquee/tick refreshes and confirm there is no repeated SD activity or visible periodic stall.
- [ ] **Item Source tags:** browse bundled SRD and project Homebrew Items and confirm compact Source tags are present and survive selection into owned records.
- [ ] **SRD catalog:** confirm Character, Spellbook and Inventory SRD rows browse normally and paging does not duplicate, skip or restart unexpectedly.
- [ ] **Homebrew independence:** toggle **Homebrew Yes/No** and confirm Inventory `[DND]`/`[HB]` rows appear/disappear while already-owned Homebrew/DND items remain visible.


- [ ] In DNDCombat, use **Jump to Initiative** and verify Short Back returns to DNDCombat. Launch Initiative from DNDolphins and verify Short Back returns to DNDolphins. Hold Back from Initiative exits without a parent relaunch.
- [ ] Verify Combat can browse and select attacks well past record 24. Host coverage uses 300 weapons and 300 spells; repeat a large real sidecar test on device.
- [ ] Verify **Settings → Menu Type** persists both **Text** and **Graphical** across cold launches. In Graphical mode verify all Home actions remain reachable with Up/Down/Left/Right, the selected icon is framed, the selected menu name appears in the header, and page transitions remain correct across all 15 entries.

### 3.6 Combat/Spellbook/Bestiary additions

- [ ] In DNDCombat verify **Spell Attacks** contains attack-roll spells while **Combat Utility Spells** (above Rituals) contains castable non-attack spells such as save/heal/control spells.
- [ ] Verify Spell Combat rows display the spell-specific casting ability/attack modifier, e.g. `(WIS/+5)`, without overwriting long spell names or damage text.
- [ ] Verify Unarmed Strike displays/uses attack = ability + PB (+ attack-only modifiers), damage = `1 + ability modifier`, and Grapple/Shove DC = `8 + ability modifier + PB`.
- [ ] In DNDSpellbook verify right-aligned source tags for class-sourced and grant-sourced spells (e.g. `WIZ`, `HIG`, `MAG`).
- [ ] With Debug Off, DNDBestiary Home must omit Pack Diagnostics; relaunch after enabling Debug and verify Pack Diagnostics appears.
- [ ] Cycle DNDBestiary Source through **Homebrew** and verify custom-pack monsters are included while other source filters still work.
- [ ] Set **Homebrew: No**, relaunch DNDBestiary and confirm the Homebrew Source choice disappears; Source: Any, Favorites, Recents, generated encounters and Custom Encounter cannot surface/pick custom-pack monsters. User-created **Custom** monsters must remain available. A saved encounter containing a custom-pack monster must remain saved but refuse Resume/Add to Initiative until Homebrew is enabled again.
- [ ] Open **Custom Encounter** and confirm it starts empty. Add a monster with **Hold OK directly on a Monster Catalog row** and through the monster-detail **Add to Custom Encounter** action; adding the same monster again increments quantity. Exercise Difficulty Simulator, Save Encounter, Composition and Add to Initiative; Initiative must receive the full custom composition.
- [ ] Inventory bags: Left/Right on **Bag** cycles Main, Group and named bags. Hold OK opens Manage Bags; add two named bags, remove one with the confirmation step, and confirm Main/Group are protected. Verify Main uses `inventory_<id>.txt`, Group uses `invGroup_<id>.txt`, and named bags preserve their display name while using a safe `inv<BagName>_<id>.txt` filename.
- [ ] Put Items in Main, Group and named bags; confirm only the selected bag page is shown while Inventory Resources aggregates weight/equipped/attuned totals across every bag. Confirm adding/editing/deleting in one bag does not modify another bag.

### Storage interruption checks added by 3.6.8 audit

- [ ] From Journal, convert an Item-category entry into an Inventory item while exercising an SD-card write interruption. After restart, confirm the pre-existing Inventory remains intact and no partial item row is visible; retry should add the item exactly once.
- [ ] During external SHD backup, interrupt an SD-card read. Confirm the backup is reported failed and no bundle completeness marker is published for the partial copy.
- [ ] Create a directory/non-file collision at a fresh `inventory_<id>.txt` path, then trigger starting-equipment initialization. Confirm initialization fails without removing the collision. Repeat with the next generated Adventure milestone Journal filename and confirm the choice rolls back while the collision remains untouched.

### 2026-10-04 shared-core refactor acceptance

These host-passing workflows remain unchecked on a physical device:

- [ ] Cold-launch Hub, navigate in Text/Graphical modes and open/exit Quick Rolls; confirm no OOM/null runtime fault.
- [ ] Create/switch/delete characters before using a collection; verify exact selected data and final deletion persistence.
- [ ] Open Languages/Proficiencies first, alternate row kinds, and leave/reopen; check low-heap failure/retry behavior.
- [ ] Confirm Catalog choice, Short Back and Hold Back preserve destination/selection and release memory.
- [ ] Favorite Spell select/cast/Back round trip; attack mode survives Combat Magic statistics.
- [ ] Select a catalog Feat in Hub, confirm save/teardown/Grants review and parent return.
- [ ] Get Elevated toggle without previously loading Items; editor accept/cancel/Hold Back and repeated input hook lifetimes.
- [ ] Build all eleven apps with the exact installed RM SDK/API and verify Loader/FAL, asset loading, cumulative stack high-water and repeated-session free heap.
