# Device test matrix

## Launch and handoff

- [ ] Build all seven FAPs and confirm no unresolved application symbols. In particular, Journal, Initiative and Bestiary must resolve `dnd_profile_ref_active_id()` without requiring or importing `dnd_storage_active_profile_load`.
- [ ] Open Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook with active IDs 0 and nonzero; confirm `[id]` appears at the top-right on each app's main screen only and disappears from detail/editor/tool/result screens.
- [ ] Launch all seven FAPs directly.
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
- [ ] With 1, 2, 3 and 4 Items, move through every row and confirm `+ Add New` remains visible; add a fifth Item and confirm it scrolls away only when needed. Repeat the same 1–5 check in Spellbook.
- [ ] Populate more than eight Items and Spells. Confirm the normal lists show no persistent `<>`; Up/Down Repeat remains responsive and performs storage I/O only when crossing an eight-record boundary. If using short Left/Right list page jumps, confirm each changes only one aligned eight-record page and holding does not churn through pages.
- [ ] From a later Item/Spell page, wrap Up/Down back to `+ Add New` and confirm page zero is reloaded: first-page rows render normally with no `Page unavailable`, empty-page artifact or stale later-page data.
- [ ] Verify Spellbook list row marks: `A` always prepared, `P` prepared, `K` known, `-` neither, and `F` appended whenever a free cast remains (including `AF`).
- [ ] Repeat Inventory/Spellbook launch/return cycles with empty and populated sidecars; confirm no blank/undrawn main page appears after repeated launches.
- [ ] Force a valid `Active=1` with character 1 unreadable/missing; Inventory/Spellbook must retain `[1]` on the error/no-character screen rather than resetting the displayed ID to `[0]` or choosing another character.
- [ ] On Inventory/Spellbook error/status/detail/tool screens, confirm `[id]` is not drawn; on the main list confirm only `[id]` occupies the top-right and no persistent `<>` is present.
- [ ] Confirm exact active-character selection, Initiative/Bestiary ID-0 fallback and teardown-before-launch behavior.
- [ ] From a cold boot or low-free-heap state, launch DNDolphins repeatedly and confirm Loader does not show `Not enough RAM to run the app`.

- [ ] Launch DNDolphins on an ordinary healthy profile and confirm the home header does not persist `Loaded`; the character name is visible. Then exercise a backup recovery or forced save/read error and confirm meaningful recovery/error status still appears.
- [ ] Weapon Combat: test STR, ranged DEX and finesse-best weapons; proficient/unproficient and magic bonuses; advantage/disadvantage; natural 1/20; ammunition decrement/persistence; versatile damage; extra dice; and critical damage-dice doubling.
- [ ] Spell Combat: test a cantrip across character-level scaling thresholds, an upcast mapped spell, a Notes-only `XdY` fallback spell, a spell attack and save spell, plus available normal slot, Pact slot, spell points, free cast and ritual options. Confirm the selected resource is consumed/preserved correctly and results use the spell's source-class casting ability. For a Wizard specifically, confirm an unprepared level-1+ spell with no Free Cast is absent; the same spell appears once Prepared/Always Prepared; and an unprepared spell with a Free Cast appears but offers **Free Cast only**. Wizard cantrips remain available without preparation.
- [ ] Combat → Rituals / Ritual Adept: on a Wizard, add known level-1+ Wizard spells with and without the Ritual tag and with Prepared both on and off. Confirm only known Wizard Ritual entries appear, preparation does not matter, cantrips/non-Wizard rituals are excluded, selection reports `Ritual cast: +10 minutes`, and slot/Pact/points/free-cast counters do not change. Repeat with more than eight Spellbook records to exercise bounded paging.

## Stability / heap / draw audit

- [ ] Build with allocator/free-heap instrumentation if available. Repeatedly enter/exit Profiles, Journal, Inventory, Spellbook, Weapon Attacks, Spell Attacks, Rituals, Adventure and Bestiary screens while forcing redraw/scroll. Confirm free heap returns to the same steady-state range after leaving each screen and does not decrease monotonically across cycles.
- [ ] Confirm redraw alone does not cause SD activity: hold/move through screens that only redraw resident data and verify storage reads occur on screen entry, explicit input/cache boundaries or writes—not from the canvas callback.
- [ ] Stress the source-derived project-owned working-set bounds from `MEMORY_AUDIT.md`: DNDolphins Spell/Ritual Combat 7,892 B project blocks, Inventory normal/save 7,456/8,736 B, Spellbook normal/ordinary-save/sort-with-page 7,736/9,016/9,880 B, Adventure+scene 5,625 B, Journal transient rewrite 2,888 B, Initiative fixed 5,276 B (6,812 B during profile sync) and Bestiary main window 4,108 B, remembering firmware/framework allocations are additional.
- [ ] Repeat Bestiary allocation-failure tests at startup/window/detail/encounter creation and confirm partially allocated project blocks are released without a later cumulative heap loss.
- [ ] Run stack high-water checks under the manifest reservations (6/4/4/4/4/3/6 KB) and compare with the source-derived estimates rather than shrinking a stack solely to reduce Loader pressure.

## Character / Inventory / Spells

- [ ] Create a character and confirm no Inventory sidecar exists after character creation or merely opening DNDInventory.
- [ ] Use Hold Up → Inventory Tools → Grant Initial Inventory with matching defaults; confirm class/species/background equipment is seeded and **no extra trinket** is added.
- [ ] Force a no-match/failed normal starting-equipment seed, invoke Grant Initial Inventory, and confirm DNDInventory rolls one random d100 trinket as fallback; invoke the grant again and confirm it does not seed again.
- [ ] If normal equipment and trinket fallback both cannot be written, confirm manual Add New remains usable and can establish the canonical Inventory sidecar.
- [ ] Reopen Inventory and confirm no duplicate starting items/currency are added automatically.
- [ ] Inventory Resources, Weapon Attacks and Adventure do not create or seed missing inventory.
- [ ] In Spellbook, short OK on **+ Add New** creates a blank spell and immediately opens its full editor; short OK on the Name field opens the spell catalog and hold OK on Name allows a custom name.
- [ ] In Inventory, short OK on **+ Add New** creates a blank item and immediately opens its full editor; short OK on the Name field opens the item catalog and hold OK on Name allows a custom name.
- [ ] Hold OK on **+ Add New** in Spellbook/Inventory follows the same blank-record/full-editor path rather than becoming a no-op.
- [ ] After Grant Initial Inventory or manual Item Add, confirm `/ext/apps_data/dndolphins/inventory_{id}.txt` exists; after the first actual Spell Add/save, confirm `/ext/apps_data/dndolphins/spellbook_{id}.txt` exists. Merely opening an absent collection remains read-only. Both sidecars remain distinct from `ch_{id}_{name}_{level}.txt` profiles.
- [ ] Add three Items consecutively without leaving Inventory; confirm all three appear, the file contains three valid `I|` records, then delete the middle item and confirm it stays deleted after app restart.
- [ ] Add three Spells consecutively without leaving Spellbook; confirm all three appear, the file contains three valid `S|` records, then delete the middle spell and confirm it stays deleted after app restart.
- [ ] Exercise the 8→9 Item and Spell boundary; confirm the first eight records persist before the next resident page is opened and the ninth record survives restart.
- [ ] With generated starting equipment already present, add three Items in succession. After each Add New, load a catalog item, Back to Inventory, confirm the new item remains visible/focused without restarting, and verify the live inventory file already contains the final catalog-populated record.
- [ ] Exercise starting-equipment tail sizes around an eight-record boundary (especially 7→8→9 and 15→16→17). Confirm no MPU fault, no `<read error>` rows, and no render-time pause/storage access while scrolling between pages.
- [ ] After editing an existing Item/Spell by catalog, text input, numeric input, or left/right adjustment, inspect the live sidecar before leaving the app and confirm the change is already present.
- [ ] After a successful Item/Spell save, add, catalog choice, Equip/Prepare action or grant/regrant, confirm the success notice is visible initially and clears on the next real input; force a write failure and confirm `UNSAVED`/error feedback does not clear as a routine success notice.
- [ ] Add at least three items and three spells consecutively, close/relaunch the respective collection FAPs, and confirm every record persists.
- [ ] Delete a middle item and middle spell, close/relaunch the respective collection FAPs, and confirm the remaining records persist in order.
- [ ] In Spell Filters, confirm Spell Class defaults to **All Classes**; on a multiclass character it shows the union of eligible spells, and selecting a specific class restricts the catalog to that class.
- [ ] In Add Spell, exercise Level, School, Ritual and Source filters separately and in combination; confirm each page is filled from matching streamed results rather than showing sparse rows from an already-selected page.
- [ ] Cycle Source through Core, Xanathar, Forgotten Realms, Ravenloft and Other; select a spell and confirm its Source/School/Ritual metadata is copied into the owned Spell record and survives restart.
- [ ] Hold OK on a known Spellbook row and confirm Prepared toggles immediately, the `S|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back; Always Prepared remains unchanged.
- [ ] Hold OK on an Inventory row and confirm Equipped toggles immediately, the row marker updates, the `I|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back.
- [ ] Simulate/retry after an SD write failure and confirm an interrupted append does not leave a partial spell/item record or permanently disable later Add/Delete attempts.
- [ ] Exercise >8 items and >8 spells across page boundaries.
- [ ] Verify spell attack/DC, slots/Pact/points and weapon attack/damage behavior.


## Inventory tools / explicit initial grant

- [ ] With no `inventory_{id}.txt`, open DNDInventory and confirm **no Inventory sidecar is created merely by opening the screen**.
- [ ] Hold Up from the Inventory list and confirm the special menu contains Currency, Inventory Resources and Grant Initial Inventory.
- [ ] Confirm existing Hold OK gestures still work: Inventory row = Equip/Unequip; + Add New = blank full editor; Item Catalog = category filter.
- [ ] Exercise all five Currency fields with Left/Right and direct numeric entry, restart, and confirm values persisted.
- [ ] Add a `Currency=` line to a character profile with no Inventory currency record and confirm DNDInventory ignores it; only `inventory_{id}.txt` may supply persisted currency.
- [ ] Exercise Inventory Resources: encumbrance toggle, capacity override, armor/shield AC application and coin normalization.
- [ ] Select Grant Initial Inventory on an absent sidecar and confirm Short OK grants defaults once, returns directly to the populated Inventory list, and the same resulting `inventory_{id}.txt` contains the granted Item rows, the expected background starting `Currency=cp,sp,ep,gp,pp` total, and `InitialInventory=1`. Reopen Inventory and confirm that exact balance reloads. Re-enter Inventory Tools and confirm the row reports Granted; Short OK again must not duplicate equipment/currency. Then Hold OK on that same row once: confirm existing Items are preserved, starting equipment/currency are appended again, the file now contains the exact combined Currency total plus `InitialInventory=2`, and the UI shows that same persisted total. Hold OK a second time and confirm no additional records/currency are added. With manual Item rows but no grant marker, confirm the normal grant remains blocked rather than silently doing nothing.

## Adventure

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

## Stress

- [ ] Repeated launch/back cycles without heap growth or crash.
- [ ] Alternate DNDInventory and DNDSpellbook launches for at least 25 handoff cycles and confirm DNDolphins continues to load each time.
- [ ] Large profile and Journal counts.
- [ ] Large monster/campaign indexes.
- [ ] Maximum-size encounter save/rename/delete paths.

- [ ] New character starts STR 15 / DEX 14 / CON 13 / INT 12 / WIS 10 / CHA 8; existing profiles retain their saved ability scores.
- [ ] Selecting class/species/background at level 1 does not auto-grant traits; Character > **Grant Initial Traits** applies the selected deterministic initial traits once.
- [ ] Increasing a class level updates Hit Dice and applicable spell/resource progression, XP floor, and deterministic class features without rereading progression metadata outside that action.
- [ ] Initiative Start New Combat opens setup with Roll for All, per-member roll, short/repeat left-right roll adjustment, hold-OK full participant editing, hold left/right participant reordering, + Temporary Member, and Begin Combat.
- [ ] Change the active character's Dexterity, Initiative Misc, exhaustion, name, HP and AC in DNDolphins; launch Initiative and verify the existing main-character roster/combat entry refreshes without duplicating or changing monster/temp modifiers.
- [ ] Set Initiative Roll to Normal, Advantage and Disadvantage; verify Roll for All and individual automatic rolls use the selected mode, while a directly edited Initiative total remains unchanged until that participant is rolled again.
- [ ] Initiative Hold OK in combat opens participant editing including name, roll/modifier, AC, HP, conditions and delete. Short Back returns to the Initiative main menu without ending combat; Resume returns to the same encounter. Hold Up moves to the previous turn and correctly crosses from round N turn 1 to round N-1's final participant.
- [ ] Bestiary full-stat-line view wraps at up to 26 characters without the old 20-character buffer truncation.
- [ ] Adventure campaign selection shows campaign names, falling back to campaign ID only when a name is absent.

## Adventure campaign selection

- [ ] Put a valid campaign pack in the inbox and confirm Preview shows name, pack/app compatibility and entry scene before installation; Hold OK installs only when validation passes.
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
- On a fresh level-1 character, use Grant Initial Traits and confirm Grant Review appears before any pending initial grants are applied.
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
- [ ] In Item Catalog, Hold OK cycles All → Weapons → Armor → Ammunition → Gear → Tools → Mounts/Vehicles → Potions → Rings → Rods → Scrolls → Staffs → Wands → Wondrous → Magic → All. Confirm each specific filter restricts the visible catalog to that category, Magic remains an aggregate non-Mundane filter, and Inventory-list Hold OK remains Equip/Unequip.
### Stack-reservation validation

- [ ] Stress DNDInventory Add/Edit/Delete, Currency/Resources, Grant Initial Inventory and multi-page catalog navigation under the restored 4 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] Stress DNDSpellbook Add/Edit/Delete, all catalog filters, Hold-OK Prepare and multi-page navigation under the restored 4 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] From DNDolphins, switch to a non-first character and launch DNDInventory; confirm the main header shows that exact active character ID and the matching `inventory_{id}.txt` contents.
- [ ] From DNDolphins, switch to a non-first character and launch DNDSpellbook; confirm the main header shows that exact active character ID and the matching `spellbook_{id}.txt` contents/class filters.
- [ ] Launch every companion directly from Apps; confirm each reads `custom_active_profile.txt` without directory discovery or launch-argument override. With a valid `Active=<id>`, confirm the exact ID is used. With metadata absent/unreadable, confirm Inventory/Spellbook/Journal/Adventure show no character and Initiative/Bestiary select ID 0. With metadata present but pointing to a missing character, confirm Inventory/Spellbook/Journal/Adventure/Initiative do **not** switch to ID 0 or another character, while Bestiary keeps the persisted ID and remains usable.
- [ ] Stress DNDInitiative full participant editing, reorder, repeated Turn/Encounter recharge, save/reload and combat navigation under the 3 KB stack reservation; confirm no stack overflow/MPU fault.
- [ ] Leave DNDAdventure/DNDJournal/DNDolphins/DNDBestiary at their larger reservations unless device high-water measurements demonstrate additional safe margin.

### Inventory / Spellbook direct-entry checks

- Launch DNDInventory from DNDolphins with a populated Inventory: the first frame is the Item list, `+ Add New` is row zero, and Hold Up opens Inventory Tools.
- Launch DNDInventory with no Inventory sidecar: the first frame is an empty Item list with `+ Add New`; opening alone does not create the sidecar. Adding the first Item creates Inventory-owned `Currency=0,0,0,0,0`.
- Launch DNDSpellbook from DNDolphins with a populated Spellbook: the first frame is the Spell list with `+ Add New` row zero.
- Launch DNDSpellbook with no Spellbook sidecar: the first frame is an empty Spell list with `+ Add New`; opening alone does not create the sidecar, and the first saved Spell creates it.

### Return focus / paging performance / ordering

- [ ] From each companion main screen, Short Back and confirm DNDolphins opens with the corresponding home row already highlighted: Inventory, Magic & Spells for Spellbook, Journal, Adventure, Bestiary and Initiative. Repeat with Hold Back and confirm it exits to firmware instead of launching DNDolphins.
- [ ] Confirm the DNDolphins Home menu order is Characters, Character, Vitals, Abilities & Saves, Skills, Features & Perks, Inventory, Magic & Spells, Bestiary, Initiative, Combat, Dice Roller, Adventure, Journal. Enter and return from each internal submenu and companion FAP; confirm the same named row is restored rather than a stale numeric position.
- [ ] In DNDolphins, highlight each internal home row that opens a submenu (Profiles, Character, Vitals, Abilities, Skills, Magic, Features, Combat and Dice), enter it, then Short Back; confirm Home returns to the same highlighted row/scroll position rather than row 0.
- [ ] Open Initiative and confirm the title bar is dark on the main menu and during Combat; `[id]` is right-aligned on the main menu and `Round N` replaces it during Combat.
- [ ] Open Item Name Catalog and compare with the established presentation: category initials are **not bracketed**, magic entries append `*`, Other entries show only their names, and the header shows `Page N <>`.
- [ ] Page repeatedly forward/back through Item and Spell Name catalogs with restrictive and broad filters. Confirm later pages remain responsive and correct, Left/Right never skips or duplicates filtered entries, and changing a filter resets the learned page offsets safely.
- [ ] Populate exactly 7/8/9 and 15/16/17 owned Items/Spells. Cross the 8-record boundaries repeatedly and confirm the correct records appear without blank/stale pages; after an edit/delete/rewrite, confirm the next page load remains correct after offset invalidation.
- [ ] Create an intentionally unsorted Spellbook containing mixed levels and names. Open Spellbook and confirm the persisted/displayed order becomes level ascending, then case-insensitive alphabetical within each level. Add a spell, rename a spell and change a spell level; Back out of the editor and confirm ordering is restored without losing Known/Prepared/Always Prepared/free-cast metadata.
- [ ] Build all companion FAPs and confirm Spellbook sorting resolves entirely from `dndspellbook_collection.c`: `dnd_storage.c/.h` must expose no Spellbook-sort symbol, and DNDolphins/Inventory/Adventure must not require any Spellbook-sort implementation.
- [ ] Repeat the Inventory/Spellbook paging/catalog tests while monitoring device stack high-water/free heap. Confirm 4 KB stacks remain stable and the bounded offset caches do not show launch-to-launch heap growth.

### Inventory / Spellbook parity regression

- Add a new Item and Spell. Confirm `Item added` / `Spell added` appears once in the editor header and clears on the next Short/Repeat/Long input without a timer or background worker; an UNSAVED/error notice must not be auto-cleared as a success notice.
- Spellbook main list: Hold Up opens Spell Filters. Confirm `Spell Class: All Classes` is the default, Left/Right cycles All Classes and each character class, and All Classes returns the union of currently eligible multiclass spells.
- Spell Filters: confirm `Eligibility: Allowed` is default and `All Spells` is opt-in; All Spells bypasses class/level eligibility only and still honors explicit Level/Ritual/School/Source/Status filters.
- Spell and Item Catalogs: confirm `Page N <>` is visible; Left/Right changes catalog pages; Item rows use unbracketed category initials, append `*` for magic entries, and leave Other entries unprefixed.
- Hold OK on a known non-always-prepared Spell and on an Item: confirm immediate persistence and a temporary `[X]` prefix on the affected row; the acknowledgement clears on the next input.
- Reconfirm full editor parity against the recovery baseline: 17 Spell fields and 36 Item fields, Name-field catalog, Hold-OK custom name, Delete, free-cast controls, Equip/Prepare quick actions and A/P/K/F Spell list marks.

## Scroll catalog regression

- Open Inventory -> Add New -> Name catalog, cycle to **Scrolls**, and confirm exactly the generic Spell Scroll (Cantrip) plus Spell Scroll (Level 1) through Spell Scroll (Level 9) rows are selectable. Confirm rarity is Common for Cantrip/L1, Uncommon for L2–3, Rare for L4–5, Very Rare for L6–8, and Legendary for L9; confirm no bundled per-spell Scroll rows appear.
- Page the Scroll filter beyond page 64 and back through nearby pages; confirm the rolling 64-page seek window keeps all generated Scroll pages reachable and sequential paging remains correct.
- Confirm generated Scroll rarity presentation treats levels 0–1 as Common, 2–3 as Uncommon, 4–5 as Rare, 6–8 as Very Rare and 9 as Legendary where rarity/magic metadata is surfaced.
- Confirm selecting a generated Scroll does not fabricate or overwrite Item weight, currency, Detail or another field with a GP price; Item value is not part of the current schema.
