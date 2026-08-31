# Device test matrix

## Launch and handoff

- [ ] Launch each FAP directly.
- [ ] Cycle DNDolphins → Bestiary → Initiative → DNDolphins repeatedly.
- [ ] Cycle DNDolphins → Journal/Adventure → DNDolphins repeatedly.
- [ ] Confirm active-character fallback and teardown-before-launch behavior.

## Character / Inventory / Spells

- [ ] Create a character and confirm no inventory exists until Inventory is opened.
- [ ] First Inventory open seeds class/species/background equipment plus exactly one hidden d100 trinket.
- [ ] Reopen Inventory and confirm no duplicate starting items/currency.
- [ ] Resources and Weapon Attacks do not create missing inventory.
- [ ] In Spellbook, short OK on **+ Add New** creates a blank spell and immediately opens its full editor; short OK on the Name field opens the spell catalog and hold OK on Name allows a custom name.
- [ ] In Inventory, short OK on **+ Add New** creates a blank item and immediately opens its full editor; short OK on the Name field opens the item catalog and hold OK on Name allows a custom name.
- [ ] Hold OK on **+ Add New** in Spellbook/Inventory follows the same blank-record/full-editor path rather than becoming a no-op.
- [ ] After first Inventory/Spellbook open, confirm `/ext/apps_data/dndolphins/inventory_{id}.txt` and `/ext/apps_data/dndolphins/spellbook_{id}.txt` are created and remain distinct from `ch_{id}_{name}_{level}.txt` profiles.
- [ ] Add three Items consecutively without leaving Inventory; confirm all three appear, the file contains three valid `I|` records, then delete the middle item and confirm it stays deleted after app restart.
- [ ] Add three Spells consecutively without leaving Spellbook; confirm all three appear, the file contains three valid `S|` records, then delete the middle spell and confirm it stays deleted after app restart.
- [ ] Exercise the 8→9 Item and Spell boundary; confirm the first eight records persist before the next resident page is opened and the ninth record survives restart.
- [ ] With generated starting equipment already present, add three Items in succession. After each Add New, load a catalog item, Back to Inventory, confirm the new item remains visible/focused without restarting, and verify the live inventory file already contains the final catalog-populated record.
- [ ] Exercise starting-equipment tail sizes around an eight-record boundary (especially 7→8→9 and 15→16→17). Confirm no MPU fault, no `<read error>` rows, and no render-time pause/storage access while scrolling between pages.
- [ ] After editing an existing Item/Spell by catalog, text input, numeric input, or left/right adjustment, inspect the live sidecar before leaving the app and confirm the change is already present.
- [ ] Add at least three items and three spells consecutively, close/relaunch DNDolphins, and confirm every record persists.
- [ ] Delete a middle item and middle spell, close/relaunch, and confirm the remaining records persist in order.
- [ ] In Spell Filters, confirm Class defaults to **All Classes**; on a multiclass character it shows the union of eligible spells, and selecting a specific class restricts the catalog to that class.
- [ ] Hold OK on a known Spellbook row and confirm Prepared toggles immediately, the `S|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back; Always Prepared remains unchanged.
- [ ] Hold OK on an Inventory row and confirm Equipped toggles immediately, the row marker updates, the `I|` record is already updated on SD before leaving the screen, and a second Hold OK toggles it back.
- [ ] Simulate/retry after an SD write failure and confirm an interrupted append does not leave a partial spell/item record or permanently disable later Add/Delete attempts.
- [ ] Exercise >8 items and >8 spells across page boundaries.
- [ ] Verify spell attack/DC, slots/Pact/points and weapon attack/damage behavior.

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
- [ ] Large profile and Journal counts.
- [ ] Large monster/campaign indexes.
- [ ] Maximum-size encounter save/rename/delete paths.

- [ ] New character starts STR 15 / DEX 14 / CON 13 / INT 12 / WIS 10 / CHA 8; existing profiles retain their saved ability scores.
- [ ] Selecting class/species/background at level 1 does not auto-grant traits; Character > **Grant Initial Traits** applies the selected deterministic initial traits once.
- [ ] Increasing a class level updates Hit Dice and applicable spell/resource progression, XP floor, and deterministic class features without rereading progression metadata outside that action.
- [ ] Initiative Start New Combat opens setup with Roll for All, per-member roll, short/repeat left-right roll adjustment, hold-OK full participant editing, hold left/right participant reordering, + Temporary Member, and Begin Combat.
- [ ] Change the active character's Dexterity, Initiative Misc, exhaustion, name, HP and AC in DNDolphins; launch Initiative and verify the existing main-character roster/combat entry refreshes without duplicating or changing monster/temp modifiers.
- [ ] Set Initiative Roll to Normal, Advantage and Disadvantage; verify Roll for All and individual automatic rolls use the selected mode, while a directly edited Initiative total remains unchanged until that participant is rolled again.
- [ ] Initiative hold OK in combat opens participant editing including name, roll/modifier, AC, HP, conditions and delete; short Back moves to the previous turn.
- [ ] Bestiary full-stat-line view wraps at up to 26 characters without the old 20-character buffer truncation.
- [ ] Adventure campaign selection shows campaign names, falling back to campaign ID only when a name is absent.

## Adventure campaign selection

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
- Create character ID 0, plus its Inventory and Spellbook sidecars, then launch Initiative: confirm ID 0 refreshes from the primary character file and the sidecars are never selected as the profile.
- Change the active character's Dexterity, Initiative Misc, exhaustion, HP, AC, and name; reopen Initiative and confirm only changed values are persisted. Reopen again without changes and confirm behavior is unchanged.
- Repeat direct launches and DNDolphins/Bestiary handoffs with a nonzero active character ID.

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
