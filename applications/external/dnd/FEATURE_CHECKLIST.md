# Feature checklist

## DNDolphins

- [x] Multiple tolerant named-field character profiles.
- [x] DNDolphins profile-selection fallback and legacy character-file relocation.
- [x] Write-only character shadows.
- [x] Launches DNDInventory and DNDSpellbook through full-path teardown-before-handoff rather than retaining their list/editor/catalog UIs in the main FAP.
- [x] DNDolphins keeps no resident Inventory/Spellbook page during startup or ordinary navigation. Weapon/Spell Combat allocates a bounded logical index on entry and hydrates only the first concrete eight-record page requested by combat; pages/indexes are released on exit. Non-combat spell grants/remaps/Long-Rest resets stream the sidecar directly. Currency/Inventory Resources UI is not linked into the main FAP.
- [x] Feature state stored in `feats_{id}.txt` with at most eight Feature records resident when needed.
- [x] Applied deterministic-grant history streamed from `appliedgrants_{id}.txt` rather than retained in the character heap.
- [x] Spell-owned casting ability, DC/attack, shared slots/Pact/points and spell-level rules remain in the character/combat hub.
- [x] Eldritch Knight and Arcane Trickster automatically use Intelligence, Third-caster progression, native single-class subclass slots, multiclass one-third contribution, and the Wizard spell list.
- [x] Dice, skills/saves and Combat spell/weapon attacks.

## DNDInventory

- [x] Opens directly to the active character Inventory list with `+ Add New` selected; no intermediate screen.

- [x] Uses the shared `/ext/apps_data/dndolphins/inventory_{id}.txt` sidecar; no separate DNDInventory character-data copy. Required canonical fields are streamed into a bounded Inventory profile projection instead of retaining `PocketCharacter`.
- [x] Eight-record resident paging with storage-backed whole-collection calculations; normal list has no persistent `<>` glyph, while `+ Add New` remains visible with up to four Items. The first bounded scan learns the three possible aligned sidecar offsets (0/8/16), so later page-boundary loads can seek directly.
- [x] Short or Hold OK on **+ Add New** stages a blank Item and opens the full editor immediately.
- [x] Full 36-field Item editor retained, including quantity/weight, equip/attune, weapon/armor values, ammunition, charges, containers and custom names/notes.
- [x] Item Catalog selection from the Name field, streamed through a bounded 256-byte filtered-page offset map and 128-byte buffered reader rather than whole-catalog loading.
- [x] Hold OK Item Catalog category filters: All, Weapons, Armor, Ammunition, Gear, Tools, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic; Magic remains an aggregate rarity filter while the specific magic-item classes can be selected directly.
- [x] Hold OK on an Inventory row toggles Equipped/Unequipped and saves immediately.
- [x] Successful quick Equip/Unequip restores the earlier `[X]` row acknowledgement; Item Catalog rows use unbracketed category initials, append `*` for magic entries, leave Other entries unprefixed, and show explicit `Page N <>` catalog paging.
- [x] Successful `Item added` is a one-shot status cleared on the next real input; unsaved/error statuses remain persistent.
- [x] Add/Edit/Delete/catalog choices save immediately to the canonical sidecar.
- [x] Hold Up from the Inventory list opens Inventory Tools without replacing any Hold-OK action.
- [x] Inventory list drawing is cache-only and has no timer/pub-sub/background worker; no-op Press/Release input events do not force redraws.
- [x] Currency and Inventory Resources are owned by DNDInventory and retain their previous editing/calculation controls; DNDInventory also initializes `Currency=0,0,0,0,0` when it opens an existing item-only sidecar that lacks Currency. Other FAP Item appenders never create the Currency record.
- [x] Grant Initial Inventory is explicit: opening DNDInventory alone leaves an absent Inventory sidecar absent; Short OK performs the normal one-shot class/species/background grant (`InitialInventory=1`) with Items and the final existing+granted `Currency=` balance committed in the same synced sidecar write. Hold OK after that grant may deliberately regrant once and consumes the override as `InitialInventory=2`; the regrant UI adopts the exact combined balance committed by that rewrite. Existing Items are preserved during the single override.
- [x] Random d100 trinket is fallback-only when normal starting-equipment composition fails or yields no equipment/currency; successful normal starting equipment receives no extra trinket.
- [x] If normal and fallback seeding cannot be written, an empty canonical sidecar can still be established so manual Add New remains usable.

## DNDSpellbook

- [x] Opens directly to the active character Spellbook list with `+ Add New` selected; a missing sidecar is shown as an empty list and is not created until the first save.

- [x] Uses the shared `/ext/apps_data/dndolphins/spellbook_{id}.txt` sidecar; no separate DNDSpellbook character-data copy. Only name/classes are streamed into a bounded Spellbook profile projection instead of retaining `PocketCharacter`.
- [x] Eight-record resident paging; normal list has no persistent `<>` glyph, while `+ Add New` remains visible with up to four Spells. The first bounded scan learns the three possible aligned sidecar offsets (0/8/16), so later page-boundary loads can seek directly.
- [x] Short or Hold OK on **+ Add New** stages a blank Spell and opens the full editor immediately.
- [x] Full 17-field Spell editor retained, including Source Class, Level, Known/Prepared/Always Prepared/Ritual, free casts, stable ID/source/school/grant metadata and Delete.
- [x] Hold OK on a known Spellbook row toggles Prepared/Unprepared and saves immediately; Always Prepared remains protected. List rows restore `A`/`P`/`K`/`-` state marks plus `F` for an available free cast (for example `AF`).
- [x] Spellbook list drawing is cache-only and has no timer/pub-sub/background worker; no-op Press/Release input events do not force redraws.
- [x] Bundled Spell Catalog contains 448 unique spells with Level, Class list, School, Ritual and Source metadata; catalog paging uses a bounded 256-byte filtered-page offset map and 128-byte buffered reader.
- [x] Add Spell filters Level/Spell Class/School/Ritual/Source/Status during streaming so bounded pages remain filled with matching results; Hold Up from the Spellbook list opens this filter screen directly.
- [x] Spell Class filtering defaults to **All Classes**, the union of currently eligible spells across every class on a multiclass character, with individual-class filtering available explicitly. `Eligibility: Allowed` remains the normal default and `Eligibility: All Spells` restores the older opt-in show-all catalog behavior.
- [x] Spell Catalog headers retain explicit `Page N <>` hints; successful quick Prepare/Unprepare restores the earlier `[X]` row acknowledgement. Successful `Spell added` is a one-shot status cleared on the next real input.
- [x] Eldritch Knight/Arcane Trickster use the Wizard spell list for catalog eligibility.
- [x] Add/Edit/Delete/catalog choices and free-cast changes save immediately to the canonical sidecar. Recorded spells are maintained in spell-level then case-insensitive alphabetical-name order using at most 24 compact transient sort keys, never a second full Spell collection. Sorting is owned by `dndspellbook_collection.c`; shared storage exports no Spellbook-sort function.

## Companion apps
- [x] Wizard Spell Attacks use combat-appropriate preparation state: cantrips remain available; level-1+ Wizard spells require Prepared/Always Prepared or a current Free Cast, and an unprepared free-cast spell offers Free Cast only.
- [x] Combat → Rituals implements Wizard Ritual Adept as a separate known-spellbook Ritual list; preparation is not required, cantrips/non-Wizard/non-Ritual entries are excluded, and ritual casting consumes no spell resource while reporting +10 minutes.
- [x] Routine successful save/add/catalog/Equip/Prepare/grant notices are transient; failure/UNSAVED notices remain visible.
- [x] Shared companion Back convention: Short Back from each companion main screen returns to DNDolphins only when its FAP exists and restores focus to that companion's corresponding DNDolphins home row; Hold Back exits to firmware without a DNDolphins handoff; normal menus contain no redundant Return/Open-DNDolphins row.

- [x] DNDAdventure declarative campaigns, checks, rewards, flags/achievements and Journal milestones; resident character access is a bounded name/class-level/ability/skill projection rather than the full character core.
- [x] Campaign inbox preview validates compatibility, content and entry-scene availability before Hold OK installation.
- [x] Campaign list discovery uses bounded sparse hints plus streaming ID lookup rather than retaining one heap offset per campaign.
- [x] Bundled Reef Wardens and Ghost Protocol campaigns.
- [x] DNDJournal standalone per-character entries, newest first, with one-shot milestone class leveling and Item-entry inventory creation.
- [x] Milestones remain Journal-facing; Continue active Adventure resumes the persisted active campaign/scene without creating duplicate Journal or Adventure progress.
- [x] DNDInitiative standalone roster/combat state and Bestiary handoff, including full numeric participant editing, manual reordering, Setup quick-AC adjustment, active-combat condition controls, Short-Back return to the main menu, Hold-Up previous-turn navigation and End Current Combat; main/combat screens use the dark title bar with `[id]` on the main menu and compact `R# T#/#` during combat; roster/setup/combat/editor navigation keeps the selected/current row visible without increasing resident state.
- [x] DNDBestiary bundled catalog, filters, encounters, custom monsters and installable packs.
- [x] Default custom Dolphin/Capybara seed only when no user custom pack exists.

## Runtime

- [x] Seven FAPs: DNDolphins, DNDInventory, DNDSpellbook, DNDAdventure, DNDJournal, DNDInitiative and DNDBestiary.
- [x] DNDolphins internal submenu Back navigation restores the same highlighted home-menu row instead of resetting Home focus to the first row.
- [x] Explicit full-path FAP launches.
- [x] Outgoing-app teardown before handoff; no artificial pre-launch sleep is used because delaying the outgoing FAP delays reclamation rather than creating Loader headroom.
- [x] Stack reservations re-audited: DNDolphins 6 KB; DNDInventory 4 KB; DNDSpellbook 4 KB; DNDAdventure 4 KB; DNDJournal 4 KB; DNDInitiative 3 KB; DNDBestiary 6 KB. Projection character lines are bounded 640-byte transient heap buffers rather than large stack arrays.
- [x] Companion profile badges reject the internal `UINT32_MAX` sentinel; `[4294967295]` cannot be displayed as a character ID while valid `[0]` remains supported.
- [x] Seven-FAP draw-path call-graph audit: project-owned draw paths perform no storage I/O, heap allocation/free, collection rewrite or storage-backed page advancement; remaining draw-time loops are bounded fixed-buffer text formatting only.
- [x] Main DNDolphins collection UI/catalog code and full Item/Spell catalogs are no longer linked/packaged into the main FAP asset set.
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
- [x] Level progression does not auto-select arbitrary class spells and prompts when spell choices expand. A bounded Level-Up Review summarizes numeric changes, deterministic traits, spell-choice notices and pending ASI/Feat choices without retaining progression metadata.
- [x] Supported species progression uses total character level and automatically grants deterministic species spells/features when their level gates are reached.
- [x] Verified deterministic class/subclass metadata is expanded while selection-bearing Fighting Styles, Invocations, Metamagic, subclass selection, spell selection and similar choices remain explicit.
- [x] Applied-grant stable IDs are normalized to the stored representation and known truncation collisions use compact unique IDs.

- [x] Active-profile loading: companion selection comes only from `custom_active_profile.txt` (`Active=<id>`); no companion discovers another character or accepts a launch-time profile override. Inventory/Spellbook/Adventure stream their app-specific projections from that exact canonical profile; Journal requires that exact profile. Initiative/Bestiary use ID `0` only when active-profile metadata is absent/unreadable, never when a present `Active=<id>` is merely stale.
- [x] All seven FAPs use `dnd_profile_handoff.c` for the same persisted active-profile/exact-profile/handoff contract. Inventory/Spellbook/Adventure alone also link `dnd_profile_projection.c`; FAPs link `dnd_storage.c` only when they need canonical/collection persistence, not merely to read `Active=<id>`.
- [x] Inventory/Spellbook reserve the dispatcher/main view before variable-size character/collection loading so low-heap collection parsing cannot strand the app without its main drawable view.
- [x] Inventory/Spellbook main-list header reserves the top-right for `[characterId]` only; `<>` is limited to explicit catalog/name-selection paging and detail/editor/tool headers do not show the character ID.
- [x] Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook show `[characterId]` at the top-right of the main screen only.

### Combat parity
- [x] Weapon Attacks still stream owned weapon records, consume ammunition from the Item sidecar, apply STR/DEX/Finesse/Ranged choice, proficiency, magic and exhaustion modifiers, support advantage/disadvantage, natural 1/20, versatile/extra damage dice and critical doubling.
- [x] Spell Attacks still stream castable owned spells, preserve the 168-entry structured combat table plus Notes `XdY` fallback, class-specific spell attack/save modifiers, cantrip scaling/upcasting, and cantrip/slot/Pact/spell-point/free-cast/ritual resource choices.
- [x] Structured spell combat supports fixed/dice upcasting, multiple attack/roll instances, secondary effects and vitality/healing outcomes; current additions include Aid vitality scaling and Heal fixed/upcast healing.
- [x] Combat presentation uses named row indices and state-free Attacks/Encounter/Recovery/Status/Defenses headers; Rituals is immediately below Spell Attacks.

## Item catalog scroll coverage

- [x] Scrolls are a first-class granular Item Name-catalog filter.
- [x] Bundled generic Spell Scroll rows classify as Scroll rather than Gear.
- [x] Bundled Spell Scroll catalog is compact: one Cantrip row plus Levels 1–9, all under the Scroll category with level-appropriate rarity; no per-spell Scroll rows are bundled.
- [x] No fake GP value is written because the current Item/catalog schema has no cost field.

## Current regression additions

- [x] Progression/grant feat catalog opens Allowed, Hold OK toggles All, and manual Features catalog is unrestricted.
- [x] Stack Qty is editable through normal Item OK numeric entry and list Hold Left/Right; persistence survives reopen.
- [x] Deleting a container releases its children to Carried and shifts higher container indexes correctly.
- [x] End Current Combat saves no history unless Save History is explicitly chosen; saved history includes date/rounds/party state/surviving opponents.
- [x] Torii Between Tides and Moonlit Market campaign graphs load and branch without retained full-pack state.
- [x] All bundled Bestiary index IDs have exactly one statblock.
