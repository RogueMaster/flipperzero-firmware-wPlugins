<p>
  <h1 align="center"><b>Hello, I'm <a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a> 👋</b></h1>
</p>
<h1 align="center"><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1><br>

# GTA VI Companion

**An offline Grand Theft Auto VI Story Mode guide, location reference, and collection tracker for Flipper Zero.**

GTA VI Companion puts collectible checklists, vehicle locations, stunt challenges, optional missions, hidden weapons, bottle messages, property information, cheat inputs, safe and door combinations, and an illustrated loading-screen gallery in a handheld app. It works as a **reference and progress tracker**: it does not connect to the game, enter cheats on your behalf, unlock content, or change an in-game save.

Open **GTA VI Companion** from the Flipper Zero **Tools** menu. The guide data and images are packaged with the app; checklist progress and loader preferences are saved to the Flipper's SD card.

## Features

The home screen has 16 icons across two pages. Icons appear in this order, from left to right: **page 1** - Infinite Money Glitch, PS5 Cheats, Xbox Cheats, Collections, Safe & Door Codes, Secret Missions, Randy's Wish List, Moses Drop-Offs; **page 2** - Wyman's Classic Cars, Standout Vehicles, Garages, Houses / Cribs, Dispatch, Mock Loaders, Mock Loader Settings, Quick guide / warnings.

| Menu | What you'll find |
| --- | --- |
| **Infinite Money Glitch** | A five-step cars-and-cocaine walkthrough using a registered 10-slot van and registered second car, warehouse respawn pickups, an explicitly unverified two-car Dunk drop-off claim, confirmed-balance checks, and manual-save fallback. |
| **PS5 Cheats** | Reported PlayStation controller sequences, effects, unlock prerequisites, and unverified-input labels. |
| **Xbox Cheats** | Reported Xbox inputs and effects, including confidence notes for converted sequences. |
| **Collections** | Checklists and directions for collectible cards, WAiNK scans, art, treasure, flamingos, publications, styles, **43 reported special-weapon pickups and variants**, **32 individually tracked optional missions**, **6 Messages in Bottles**, **58 guide-reported meteorite sites**, **5 Missing Persons investigation stages**, 12 endings and proposed variations, stunt challenges, and 17 location search leads. **Activities / Robberies** covers bank jobs, operations, store robberies, drone trials, shooting, racing, arcade games, pool, golf, **100 numbered basketball hoops**, and challenge milestones. |
| **Safe & Door Codes** | 46 reported safe combinations, door codes, and puzzle passwords, with location clues and source-confidence labels. |
| **Secret Missions** | 16 checkable optional encounters, hidden weapon finds, cash routes, stat rewards, and Easter eggs, grouped by the earliest reported story chapter. Uncertain unlocks appear at the end. |
| **Randy's Wish List** | 22 vehicle requests with reported pickup clues, access conditions, and delivery progress. |
| **Moses Drop-Offs** | Seven reported vehicle delivery jobs and their location clues. |
| **Wyman's Classic Cars** | Four restoration checklists with directions to 15 wreck parts, plus a checklist entry for claiming the final car. |
| **Standout Vehicles** | 280 trackable vehicle entries, including parked variants, the Annis Pixel Elegy in a Capri container (existing checklist entry enriched), the Maibatsu Sunset Sanchez at Grayson, aircraft, watercraft, mission and stunt rewards, and reported leads. |
| **Garages** | 15 garage entries with location, capacity, acquisition cost, or unlock requirements where known. |
| **Houses / Cribs** | Five story safehouses with locations, chapter access, and facilities. |
| **Dispatch** | Region-based browsing of 865 indexed pickups and activities, with progress tracking and a next-unchecked shortcut. |
| **Mock Loaders** | Gallery of the enabled image sets. The app ships 30 GTA VI and 15 Dungeons & Dolphins images. |
| **Mock Loader Settings** | Enable or disable each image set in the gallery and startup rotation. |
| **Quick guide / warnings** | Button controls, cheat entry, saving precautions, achievement warnings, flamingo identification, style scanning, and underwater search tips. |

## Using the app

### Menu navigation

Use the **D-pad** to select a menu icon, **OK** to open it, and **Back** to return. The selected feature name appears at the top of the screen.

For collection and vehicle lists:

- **OK** on an item opens its description, location clues, and related notes.
- **Right** in a list, or **OK** in an item detail, checks or unchecks the item.
- **Back** returns to the list; the list header displays the completion count.

All populated entries in these areas have individual **Completed** checkmarks:

| Area | What you can check or uncheck |
| --- | --- |
| **Bridges** | Each **Under the Bridge** crossing, organized by county (202 reported crossings). |
| **Cars** | Each Randy's Wish List vehicle, Moses drop-off, Wyman's Classic Cars restoration, and Standout Vehicle; garages and houses have their own acquired/visited-style checklists. |
| **Stunts** | Each Stunt Jump, Under the Bridge crossing, Knife Flight, and Base Jump (397 reported stunts in total). A bridge crossing appears **once** in progress, even when viewed as a stunt. |
| **Collections** | Each cataloged collectible or discovery, plus Special Weapons acquisitions, Optional Missions, Messages in Bottles, and viewed story endings, with individual checkmarks. |

**Right** toggles the selected entry from its list. **OK** opens its details, where the center **Check / Undo** action also toggles completion. An empty category marked *Locations pending* has no entries to check until location data is available. The count in each list header reflects checked entries, and returning to a list updates the checkmark state. All these lists share one persistent tracking system, so checking a bridge or vehicle through a different route does not create a second completion record.

Checklist progress is saved automatically to the Flipper Zero SD card. See **Saved progress and backups** below for file locations and upgrade behavior.

For **Secret Missions**, press **OK** to read the walkthrough, **Right** to check or uncheck an entry in the list, or choose **Check / Uncheck** with **OK** in its detail view. The menu shows the number completed out of 16, and completion is restored when the app restarts.

### Saved progress and backups

Collection and Dispatch checkmarks are saved in **`/ext/apps_data/gta_vi_cheats/`** using `progress2.a` and `progress2.b`. When upgrading from an older installation, the app can import checks from `progress.a` and `progress.b` if the newer save files are not present. The original files remain untouched as a backup; progress made after the upgrade is stored in the newer files and will not be visible if you revert to an older app build.

**Secret Missions** keep their own `secrets.a` and `secrets.b` files. **Mock Loader Settings** use `mock_loaders.a` and `mock_loaders.b`. Back up the app's SD-card data directory before changing firmware, formatting the card, or reverting to an older app build.

### Region-aware Dispatch

Dispatch groups **865 location stops** into **Vice City**, **Leonida Keys**, **Grassrivers**, **Port Gellhorn**, **Ambrosia**, **Mount Kalaga**, **Other areas**, and **All regions**.

Choose a region to open its first unchecked stop. Press **OK** to change the current stop's checkmark, **Right** to jump to the next unchecked stop, or **Back** to choose another region. To revisit a completed stop, open its collection or vehicle entry. Checkmarks are shared between the two menus.

**Other areas** includes places outside the six main regions and locations with uncertain region assignments. Alternate spawns can appear under more than one region. Dispatch covers all seven Moses drop-offs and 16 of the 22 Randy requests. The home menu has the complete in-app vehicle lists and Standout catalog.

## Infinite Money Glitch

The in-app guide now prioritizes the **cars-and-cocaine warehouse loop** using vehicle storage rather than an immediate manual-save reset. The main location is **Apollo Aggregates**, the red warehouse east of Hamlet in South Vice-Dale County, beside the teddy-bear van. The method is an exploit reported in **Story Mode**, not an official cheat code, and may change with title updates.

1. **Register both vehicles before collecting.** Bring a **10-bag-slot van or delivery truck** and, for the experimental two-car swap, a **6-bag-slot car**. **Both must be owned/registered**, including the second car; register a stolen vehicle using the **Register** service at a Pay 'n' Spray or through Moses before storing cocaine. Do not rely on an unregistered stolen car retaining its cargo. Keep an untouched manual save. Chapter 2 is recommended to avoid Chapter 1 story-related money loss.
2. **Collect cocaine and cash.** Empty the teddy-bear van outside, the parked van inside, the office table, and the office safe. Retrieve the cash-filled stash bag from the central open van. Store filled bags in the 10-slot van and the second car. **Do not drive away the interior warehouse van**, which is used as a repeat pickup.
3. **Refresh the warehouse by switching characters.** Keep the collected cocaine stored in your vehicles, switch to the other playable character, then switch back and return to Apollo Aggregates. Recheck the indoor van and office safe for respawned cocaine; refill your vehicles and repeat as needed. If the loot does not reset, the older **personal-vehicle manual save/reload** method remains a fallback; always use a separate save slot.
4. **Try the two-car Dunk's trailer swap (unverified).** Back the full 10-slot van up to Dunk's drop-off and unload it, move it away, then reverse the filled 6-slot car into place and unload. A community poster claims that repeated character pickup/drop-off after swapping vehicles lets the storage keep paying, citing **$13,200 per drop-off**. **No independent confirmation of actual credited payments was found**; reports warn that the counter can change without payment. Do not assume the amount is repeatable.
5. **Cash out and verify.** Check the real bank balance, pending sales and safehouse mailbox. If the two-car claim fails, use the repeatable warehouse pickup and sell full bags through a story-unlocked fence instead. Early-game buyers include Donnie or Dunk; **Tyrone** reportedly offers the strongest narcotics rate starting in Chapter 3. **Brian's Lockup** in Key Lento has a reported keypad code of `7700` and may pay after several in-game days. Fence demand, payout, and availability vary.

**Evidence distinction:** Published guides describe the 10-slot van, cocaine pickup, and vehicle-loading route. This walkthrough uses **switching characters and returning to refresh the warehouse**; exact respawn behavior may vary by save or game version. Other guides describe leaving the area instead. The two-car Dunk payout is **a single community claim**, not a verified working exploit. A separate vehicle save/reload loop remains a fallback. Be cautious with autosaves and any bag that appears to remain in storage after selling.

### Money-glitch sources

- [VICE - Apollo Aggregates loot method](https://www.vice.com/en/article/gta-6-money-glitch/) and [character switching](https://www.vice.com/en/article/how-to-switch-characters-in-gta-6/).
- [Games.gg - registered vehicle and Register service at Pay 'n' Spray or Moses](https://games.gg/grand-theft-auto-vi/guides/grand-theft-auto-6-money-glitch-warehouse-location-how-to-repeat/).
- [Insider Gaming - leave-area reset](https://insider-gaming.com/gta-6-infinite-money-glitch/).
- [KeenGamer - save/reload method](https://www.keengamer.com/articles/guides/gta-6-unlimited-money-glitch-guide/).
- [Guías GTA 6 - Brian's Lockup method](https://guiasgta6.com/en/guides/gta-6-unlimited-money-glitch-brians-lockup/).
- [Reddit r/gtaglitches - community discussion](https://www.reddit.com/r/gtaglitches/comments/1y1gou5/gta_6_unlimited_money_red_barn_apollo_aggregates/).
- [GTA VI Discussion - tested 10-slot van and drive-away cocaine loop](https://gtavidiscussion.com/guides/gta-vi-money-glitch-unlimited-money/).
- [Reddit r/GTA6unmoderated - unverified two-vehicle Dunk payout claim](https://www.reddit.com/r/GTA6unmoderated/comments/1y1qu37/found_this_new_money_glitch/).
- [Reddit r/GTA6unmoderated - warehouse + two-character car-storage report](https://www.reddit.com/r/GTA6unmoderated/comments/1y24h5k/money_glitch_apollo_warehouse_bag_dupe/).

## Secret Missions and Easter eggs

**Secret Missions** is a guide to 16 optional activities, hidden weapon pickups, cash opportunities, and Easter eggs. Entries are organized by the **earliest reported chapter or prerequisite**: Chapter 1 (with missable opportunities first), later Chapter 1 unlocks, Chapter 2 and its follow-ups, then locations with **unknown story requirements**. Within a chapter, exploration activities can usually be attempted in different orders. Check off activities as you finish them; you can uncheck any entry later. This is a discovery guide, **not** a required mission sequence. The menu uses `C1`, `C1+`, `C2`, and `C2+` for reported story windows; `?` means the chapter requirement has not been established.

| Earliest access | Hidden activity | Location / how to start | Outcome |
| --- | --- | --- | --- |
| Chapter 1 | **Three Sheets talking parrot** | Behind Three Sheets Liquor in Key Lento during Shirley's rum errand | Hidden cash near generator and in safe |
| Chapter 1 only | **Cal's lost drugs** | Follow Cal's text and search the Recreation Area, Burned Houses, Pelican Valley, and Thorpe Adventures circles | Sellable packages; reported buyer payouts differ; missable |
| Chapter 1, *Don Gone* | **Donnie's hidden revolver** | Open Donnie's Blista trunk before destruction | Unique H&L .38; missable |
| Chapter 1 | **Hamlet ~$20,000 loot route** | Apollo Aggregates east of Hamlet, teddy-bear vans and warehouse; fence items and pawn the card | A reported combined route approaching $20K after sales; payout varies |
| Chapter 1 start | **The Final Flush** | Scan `#NOFLUSHERS` QR poster on Corsair Key; meet Dr. Merle at night in Tequesta | **Permanent stamina increase**, not health |
| Chapter 1 start, Chapter 2 finish | **Kirk's Skunk Ape hunt** | Read a flyer, meet Kirk in the swamp at night, follow his multi-part quest | Cash and unique rifle during the line |
| Chapter 1 / boat | **Derelict Spy Vessel** | Southern Permit Keys, deck hatch in wreck | Suppressed Assassin Nipper .38 pistol |
| Chapter 1 / boat | **Smuggler's boat** | Northern Permit Keys island, shotgun on beach | Smuggler Gorse 425 |
| Chapter 1 | **Crabshack outpost** | Pelican Valley swamps, power box and safe clues | Crabshack VF5 and cash; reported codes `0072` and `0212` |
| Chapter 1 after *Building a Life* (reported) | **Satellite-dish RV bunker** | Balsero junkyard, concealed RV hatch | Type 9 SMG and H-AT launcher |
| Chapter 2 night | **UFO abduction** | Abandoned flashing car on a Kelly County rural road, near Starlet Motel | Hidden encounter and reported trophy |
| After UFO abduction/dream | **Alien rescue at South Point Airfield** | Dream clue, roof hatch, variable keypad, light puzzle and alien extraction | **Permanent max health, stamina and Focus increases** (reported) |
| Early open world, chapter uncertain | **Hyde Island Manhunt** | Travel alone to Hyde Island in northern Leonida, survive armoured hunter and dog | **Island Poacher FF-50** heavy sniper and reported cash; no confirmed suppressor |
| Port Gellhorn access, chapter uncertain | **Satan / Circles of Hell** | Open garage in northern Port Gellhorn from midnight to 4 AM, choose *Rock On* | Hellspawn Comshot 12, tattoo and shirt; **no verified health increase** |
| Vice City access, chapter uncertain | **Secret radio stations** | WAiNK poster scans at The Vault and four La Perle/Crosstown shops | Hidden stations (reported) |
| Vice City access, tower uncertain | **Literal Easter egg** | Nest atop a high broadcast tower | Short secret scene; no verified material reward |

The in-app entries contain the detailed steps and specific location clues. For the Hamlet cash route, the total combines several items and fences; it is **not** a guaranteed $20,000 cash pickup. The separate **Infinite Money Glitch** menu covers the disputed respawn/reset method. Some activities have a spoiler-heavy outcome, and certain Chapter 1 opportunities cannot be revisited after the story advances.

### Secret Missions sources

- [The Big Lead - Hyde Island Manhunt and Island Poacher FF-50](https://www.thebiglead.com/saw-style-killer-gta-6/); [Cultured Vultures - Hyde Island mission walkthrough](https://culturedvultures.com/gta-6-how-to-find-the-manhunt-mission/); [GTA Base - Island Poacher variant](https://www.gtabase.com/gta-6/weapons/island-poacher-ff-50).
- [LeonidaHub - Apollo warehouse and parrot cash](https://leonidahub.org/en/news/easy-early-money-in-gta-6-the-apollo-aggregates-warehouse-and-the-parrot-that-knows-where-the-cash-is); [GTA6CommunityWiki - Chapter 1 $20K route](https://gta6communitywiki.com/tips/early-money-route); [GTA6Direct - warehouse valuables](https://gta6direct.com/free-loot).
- [GTA6.free - Permit Keys, outpost and free weapons](https://gta6.free/weapons/gta-6-unique-weapon-locations); [GTA6 Vice City Hub - secret weapon leads](https://www.gtavicecityhub.com/secrets).
- [GamesRadar+ - The Final Flush and permanent stamina](https://www.gamesradar.com/games/grand-theft-auto/gta-6-final-flush-increase-maximum-stamina/); [PowerPyx - optional mission chapter windows](https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/); [Nexzy - missable optional missions](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/gta-6-optional-missions).
- [The Big Lead - UFO dream, airfield keypad and stat rewards](https://www.thebiglead.com/gta-6-keypad-door-code-south-point-airfield/); [Vice City DB - alien rescue evidence](https://vicecitydb.org/easter-eggs/alien-rescue-dream-quest).
- [Indiatimes - Satan ritual and reported shotgun reward](https://www.indiatimes.com/game-guides/how-to-summon-and-kill-satan-in-gta-6-satanist-locations-hell-mission-beelzebub-dave-encounter-and-exclusive-shotgun-reward-explained/articleshow/136066345.html); [GTA Scoop - additional secrets and hidden radio stations](https://gtascoop.com/guides/gta-6-easter-eggs-and-secrets/).

**Source confidence:** These are third-party launch-period reports, not gameplay-tested by the companion. Names, spawns, unlock chapters, puzzle combinations and rewards can vary or be corrected by newer guides. Unknown requirements are explicitly labeled rather than inferred from their location.

### Optional Missions checklist

The **Collections → Optional Missions** checklist tracks **32 separately checkable mission entries**. The table below documents the original eight strands; another 24 individual mission steps from Billy, Andy, Yasmani, Tyrone, Dunk, Cal, Eddie, Shayna, Yago and the bank-vault tutorial are also included. They cover the reported start chapters and important missable access windows. The original eight are preserved without changing their IDs. This is distinct from the **Secret Missions** home-menu guide; a mission listed here is not a second copy of Cal's Lost Drugs, Kirk's Skunk Ape hunt, or an existing secret. Select a row to read its chapter and location clues, then use **Right** or **Check / Undo** to track it. Some original strands contain multiple follow-up objectives, while newly added strands are separated into individual checkable steps.

| Mission strand | Reported access and location | Important notes |
| --- | --- | --- |
| **Get Shirley's Rum** | Chapter 1, Key Lento; buy Salty Squid rum at Three Sheets Liquor for Shirley. | **Missable before Tourist Trap.** Buy the rum before investigating strange bird noises: the reported shootout can close the shop. |
| **Ring the Bell!** | Chapter 1; Cal's invitation to the Rusty Anchor after *Day Release*. | Go for drinks and drive Cal home. The Rusty Anchor shirt and cap become **purchasable**, rather than free mission rewards. |
| **Steal the Scooter** | Chapter 1; Shirley's follow-up call, Peggy's driveway across the water in Key Lento. | Bring the scooter back to Shirley; reported unavailable after *Tourist Trap*. |
| **Pick Up a Gun** | Chapter 2; Lucia at Gellhorn Outfitters in Port Gellhorn after *Girls' Trip*. | Retrieve locker weapons or purchase a gun; no separate cash reward is reported. |
| **Kyle / McKittrick Farm** | Lucia; flyer near Starlet Motel, then McKittrick Farm; Chapter 2 start and Chapter 3 follow-ups. | Multi-stage crop-duster, chemical-recovery, and standoff strand; rewards vary by choices. |
| **Bret Vogel Hunting** | Jason alone, northern Lummox County after *Coffee Break*. | Hunting follow-ups; the **Killtelligence SM-69** is reported as a weapon reward and has its own acquisition checkmark. |
| **Yago Limited Race** | Jason; Salton, Vice City, after *Bumper to Bumper*. | Watch for the **limited-time hourglass marker**; the reported race payout is not guaranteed. |
| **Pilot Pete's Flights** | Jason; Sundown Private Airfield in Chapter 4 after *Pool Day*. | Start *Passing the Torch*; **Pete's KZ-59** lies on the hangar backroom **table**, while safe `3718` is separate loot. Later flying jobs follow. |

These are **guide-reported story windows and rewards**. Check the in-game phone, map, and mission state for availability; do not assume an optional activity is still accessible after its chapter cutoff. Sources: [PowerPyx optional-missions list](https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/) and [Nexzy optional-missions guide](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/gta-6-optional-missions).

## Safe & Door Codes

An offline catalog of **46 published codes and passwords**. Choose **All codes**, **Mission safes**, **Operations**, **Doors / garages**, **Puzzles / other**, **Browse by area**, or **Random safe tips**. The area browser groups codes into Vice-Dale / Balsero, Keys / islands, Port Gellhorn, Swamps / Grassrivers, Mariana County, Inland / north, and Other areas. Each entry shows the published combination, area, landmark or access instructions, story context when known, and a confidence label. Codes appear at the start of list rows so they are visible on the Flipper's small display.

**Weapon access:** Black Eagle's Balsero warehouse office uses `789987`, and the safe inside uses `031462` to obtain the Kalinga .50. The Pelican Valley research outpost uses password `CLEAVES`, safe `0072`, and boathouse keypad `0212` for the Crabshack VF5. Published bunker clues connect the island door (`010317`) and safe (`2975`) to the IAA Black Site hatch (`085355`), reportedly holding the Cold War 556. The Marbelle Hotel safe (`955214`) is reported to contain a collectible watch. These combinations are also listed in the relevant weapon details. The Black Eagle M306 TMR has a separate reported access route through the stacked boxes and fence.


| Category | Entries | Examples |
| --- | ---: | --- |
| **Mission safes** | **8** | Delights Cabaret `2008`; Donovan Banks & Co. `0104`. |
| **Illegal Operations** | **14** | Nova Auto Junkyard `7777`; Kingpin Bowling `0300` (conflicting report). |
| **Doors / garages** | **15** | Brian's Lockup `7700`; Key Lento Party House `092319`; Mount Kalaga Tower `69185` (conflicting report). |
| **Puzzles / other** | **9** | Pelican Valley research codes and two text passwords. |
| **Total** | **46** | Four- to six-digit PINs and text passwords. |

At **Father Scrumpy's Red Apple Orchard**, enter `360782` at the farmhouse garage to claim the **Applesauce Bobcat**. At **Southside Storage**, enter `1010` after photographing all five **1010 Murals** to claim the **1010 Cheetah**. Both vehicles have entries in Standout Vehicles.

At **Kingpin Bowling** in Port Gellhorn, the **Gutterball Moreland 850 shotgun is on the northeast pool table**. The **northeast office safe** (reported `0300`, with `0330` in a conflicting guide) holds cash and the southeast room holds the **Crowex Octane 'Orca'** luxury watch. At the **Cottonmouth Lake trailer** on the southwest spit, collectible **Laki Maivia (Bronze)** is in the **safe beneath the kitchen microwave**, opened with `44017`. The highlighted bedroom-wall photos/postcards give the clue. This card and safe already have entries; do not count either twice. [Kingpin Bowling watch](https://gta6.house/library/items/crowex-octane-orca-luxury-watch)

**Vanessa's pink Pfister Growler:** Before **Chapter 3 ends**, inspect the memorial photo (**Muffins**), vanity sketch (**Queen**), and number **15** on Boobie Ike's laptop, enter `MuffinsQueen15`, and **read the WhatsUp message**. Then find the pink-bow garage on **Del Rio Blvd, La Perle**, through the run-down motel lot past Aline's Thrift Store near Auntie's Place; enter `92665` to claim the car. The garage keypad may not activate before reading the message. Access may be lost when Chapter 4 begins; the vehicle has its own existing Standout checklist entry. These are published walkthrough claims, not guaranteed access conditions.

**Leading zeros matter.** Published sources disagree on Kingpin Bowling (`0300`/`0330`), Saber Shipping Facility (`1122`/`1965`), and Mount Kalaga Tower (`69185`/`691865`). South Point Airfield `2341` is also disputed. Certain robbery safes may randomize their combinations; if a number fails, look for an in-game note, computer, or other clue.

**Confidence labels:** *Guide-tested* means the linked guide publisher reports testing it, not that the companion author tested it; *Multiple reports* means more than one published source; *Reported* means limited corroboration; *Variable* means the result may differ by session or replay; and *Conflicting* means sources disagree. A `?` beside a code signals uncertainty.

### Safe and door code sources

- [VICE - Illegal Operations safe codes](https://www.vice.com/en/article/all-gta-6-safe-codes-illegal-operations/).
- [GamesRadar+ - safe codes](https://www.gamesradar.com/games/grand-theft-auto/gta-6-safe-codes/).
- [GTA Guide - safes, doors, and garages](https://gta.guide/gta-6/guides/safe-door-garage-codes).
- [GTA 6 Player Tools - mission and puzzle codes](https://gta6playertools.com/locations/safe-codes).
- [GTA6Heists - robbery safe clues](https://www.gta6heists.com/robberies/safe-codes).
- [Radio Times - door codes](https://www.radiotimes.com/technology/gaming/gta-6-door-codes-list).
- [ToGame - safe-code catalog](https://togame.io/b/gta-6-safe-codes/).
- [GTA Base - safe-location map](https://www.gtabase.com/gta-6/map/?feature=safe).
- [GamerHour - Black Eagle office and safe codes](https://gamerhour.net/guides/black-eagle-all-safe-codes-in-gta-6/).
- [GTA Scoop - safe codes and weapon rewards](https://gtascoop.com/guides/gta-6-safe-codes/) and [President's bunker route](https://gtascoop.com/guides/gta-6-president-bunker/).
- [NerdSnack - hidden weapon-related codes](https://nerdsnack.com/gta-6-safe-codes/).
- [PowerPyx - Kingpin Bowling operation and room rewards](https://www.powerpyx.com/gta-6-hit-kingpin-bowling-walkthrough/).
- [MetaForge - chapter-bound Vanessa garage prerequisites](https://metaforge.app/gta-6/gta-6-chapter-3-missables-point-of-no-return).
- [Black Eagle office and safe access](https://gamerhour.net/guides/gta-6-black-eagle-import-kalinga-black-eagle-warehouse-safe-code/)
- [Black Eagle Kalinga and M306 TMR entry routes](https://www.followchain.org/gta-6-black-eagle-door-code-safe-code/)
- [Pelican Valley research puzzle and Crabshack VF5](https://gta6.house/guides/gta-6-pelican-valley-outpost-codes-crabshack-smg)
- [President's Bunker sequence and IAA Black Site](https://gtascoop.com/guides/gta-6-president-bunker/)
- [Haunted Marbelle Hotel safe and watch](https://gta6.house/guides/gta-6-silent-hill-haunted-hotel-safe-code)
- [Hideout safe combinations and weapon rewards](https://nerdsnack.com/gta-6-safe-codes/)

## Special Weapons

Under **Collections → Special Weapons**, browse **43 individual guide-reported weapon pickups and variants** covering rifles, pistols, SMGs, shotguns, launchers, a knife, and a speargun lead. Each row has a name, location or access clue, and acquisition notes. Several entries share a location but have independent checkmarks. Locations without adequate corroboration are labeled **reported** or **unconfirmed**, rather than shown as established spawns.

**Mark acquired:** select a weapon and press **Right** to check or uncheck it. Press **OK** for details and choose **Check/Undo**. The status reads **Acquired**, the list shows progress, and checkmarks are saved to the app's SD-card progress files. Acquiring one of a paired set does not mark the other.

Example searches include the **Assassin Nipper .38** on a Permit Keys wreck, the **Unpacked Comshot 12** inside a Key Lento house, the **Drug Mule KZ-59** near a sunken Riquezas Keys speedboat, and the **Deever Tech GLR40** plus **Cockroach Xtreme 556** at the Sunrise Motel in Tequesta. These are reported community discoveries, not guaranteed spawns.

The expanded catalog also tracks **Kingfishers Akulus** (Sunrise RV Park), **Chromium W-10** (La Perle Trap House), **Zodiac Nipper .38** (Ocean Beach Lab), **Quarter Mile ES9** (Chatfield Storage on Waxman Island), **Killtelligence SM-69** (Bret Vogel's hunting strand), **Shark Knife** (a shark cage north of Saint Attaway Islands), **The Patriarch DRX** (submerged vehicle southwest of Kalaga Reservoir), **Deadringer 1992** and **Money Bag 1992** (Moreau River mobile-home area), **Cargo H&L .38** (yellow Scoop container at Port Vice City), **Couch Potato Mustang .357** (suspended couch off Tanglewood Lane), and **Pete's KZ-59 Assault Rifle** (Sundown Private Airfield hangar). These named variants are **guide-reported leads**. Some have broad search areas, conflicting variant names, or limited independent verification; do not interpret all 43 as confirmed free pickups.

**Updated pickup directions:** The **Couch Potato Mustang .357** is hidden in a hanging couch by a mobile home off Tanglewood Ln in Port Gellhorn. Shoot the supporting ropes, then sit on the fallen couch to claim the revolver. The *Bombazo!* publication also has a copy at the Formosa Ave apartments in Hamlet, inside the second ground-floor unit from the right (on the toilet).

**Keep rewards and codes distinct:** The **Gutterball Moreland 850** is on the northeast **pool table**, not in the Kingpin Bowling office safe. **Pete's KZ-59** is on the hangar backroom **table**, not in the `3718` safe. **Killtelligence SM-69** is described as a hunting-quest reward, not an Ammu-Nation purchase. Named shop-exclusive variants should not be mistaken for world pickups.

A weapon's detail includes any required combination when known. For example, the Kalinga .50 gives both the office and safe codes; the Crabshack VF5 gives the dock code and optional puzzle chain; and Russo's Pistol and the Cold War 556 list their bunker combinations. Individual acquisition checkmarks are independent even when weapons share a building or puzzle.

### Special Weapons sources

- [GTA6Bugs - unique weapon variants and locations](https://gta6bugs.com/collectibles/unique-weapon-variants)
- [GTA Scoop - unique weapons and gang-operation rewards](https://gtascoop.com/guides/gta-6-unique-weapons/)
- [GAMES.GG - unique weapons and pickup locations](https://games.gg/grand-theft-auto-vi/guides/grand-theft-auto-6-all-unique-weapons-where-to-get-them/)
- [GTA Explained - hidden weapon clues and missable finds](https://gtaexplained.com/gta-6-hidden-weapons/)

## Endings

**Major story spoilers.** Under **Collections → Endings**, browse **12 finale routes and epilogue variations**, including **nine reported outcomes or scene variations** and **three speculative variants that have not been demonstrated reliably**. Each entry explains the relevant chapter, choices, preparation, and reported result. The final three entries are marked with `?` in their names so they cannot be mistaken for confirmed unlocks.

**Track endings seen:** Press **Right** on an ending to mark it **Seen** or uncheck it; press **OK** to open its requirements and use **Check / Undo**. The completed count and checkmarks are saved to the regular Collections progress files on the SD card. This tracker does not change the game or its save data.

Before the finale, make a **manual save**. Published guides identify four major influences: Jason and Lucia's relationship, each character's Criminal Profile, money deposited in **BuckMe Family Savings**, and the choices in **Protective Custody** and **The End of the Road**. Sources disagree on exact thresholds for some branches. The commonly cited best-outcome requirements are a **Hot Together** relationship, relatively clean profiles, **$2,000,000 in Family Savings**, and **Swim for It** at the bridge.

| Ending / variant | Reported way to see it |
| --- | --- |
| **The Swamp** | Wait for **Take Deal** at the officer's standoff in *Protective Custody*, then accept it. |
| **The Sacrifice** | Take the deal; if the alternate armed tower/elevator standoff triggers, choose **Stay Down**. Trigger uncertain. |
| **Defend Jason** | Reach the reported alternate standoff, then select **Defend Jason**. Trigger uncertain. |
| **No Surrender** | Shoot the officer; at the bridge select **No Surrender**. |
| **No Surrender + Two Years Later** | Same bridge choice, with a full savings account and strong relationship; reported alternate family epilogue. |
| **Armed Swim for It** | Build a high Criminal Profile, then choose **Swim for It**. An armed Jason reportedly leads to a fatal police response. |
| **Armed Swim + Two Years Later** | Armed Swim route with full savings; a different post-credit family scene is reported. |
| **Swim for It - Empty Handed (Prison)** | Strong relationship, clean Criminal Profiles, savings below $2,000,000; choose **Swim for It**. |
| **Happily Ever After** | Hot Together relationship, clean profiles, $2,000,000 Family Savings; choose **Swim for It**. |
| **The Swamp + Savings (?)** | *Speculative:* Take Deal with full Family Savings; an additional ending scene is unverified. |
| **The Sacrifice + Savings (?)** | *Speculative:* Stay Down on the alternate deal route with full savings; not reliably documented. |
| **Defend Jason + Savings (?)** | *Speculative:* Defend Jason on the alternate route with full savings; not reliably documented. |

The **12-item count includes variations and predictions, not 12 independently verified, materially different endings**. GTA Base documents five principal endings; other written guides distinguish seven or nine outcomes and epilogue variants. The final three checklist entries are hypotheses, not demonstrated unlocks. Mark an entry Seen only after observing that version in your own playthrough.

### Endings sources

- [GTA Base - five principal endings and preparation](https://www.gtabase.com/gta-6/articles/gta-6-endings-explained).
- [GTA Base - final mission choices](https://www.gtabase.com/gta-6/missions/the-end-of-the-road).
- [GTA 6 Player Tools - seven reported finale paths](https://gta6playertools.com/guides/endings).
- [U4N - nine ending/epilogue variants](https://www.u4n.com/news/gta-6-endings-explained-good-bad-and-secret-paths.html).
- [VICE - the four ending-influencing factors](https://www.vice.com/en/article/gta-6-multiple-endings-explained/).

## Collections and exploration

Select **Collections** to browse categories and individual sets. Each entry provides the available location, landmark, chapter, or activity clue and a checkmark for tracking completion.

| Collection or activity | Content |
| --- | --- |
| **Collectible Cards** | **480 cards across 18 sets**, organized by card set with pickup clues. John Morgan (Dusty Pink) is reported on a floor-level poster at the back of the cream villa's garage on Casanova Island. |
| **WAiNK** | **31 landmarks**, **35 curiosities**, **23 sculptures**, and **15 mapped No Flusher tags**, with identifying landmarks and scan instructions where available. |
| **Peewees Billboards** | **4 roadside billboards** with highway, interchange, and nearby-business clues, plus the reported Belville reward puzzle. |
| **AHOL Street Art** | **112 art scans** organized by art type, including an option to mark a whole type complete. |
| **1010 Murals** | **5 photo locations** and framing clues. The reported completion reward is the 1010 Cheetah at Southside Storage, code `1010`. |
| **Kenton's Treasures** | **7 named treasure sets** with item lists and dive-site clues. Four sets have additional directions to individual relics within the wrecks. |
| **Underwater Discoveries** | **6 named shipwrecks**, now with piece counts. Belpoint Strait and Bobcat Dive Site are treasure-set dives, but **do not** count among these 33 discoveries. |
| **Magazines, Books & Comics** | **27 reading entries**, including *Law & Litigation in Leonida*. That Hamlet copy has a published location and an unconfirmed contribution to the game's 27-title counter. |
| **Book Swaps** | **7 Little Library locations**. |
| **Meditations** | **18 character-specific completions** across **10 places**, with separate Jason and Lucia entries when applicable. |
| **Cocaine Flamingos** | **41 reported pickups**, including the Cottonmouth trailer marsh, Watson Bay's damaged wall, a Capri grill, a Port Gellhorn highway house, and the Mariana County sunburst doorway. Look for black-and-white beaks. |
| **Unique Playlists** | **2 playlists** and **5 related poster locations**. |
| **Discovered Styles** | **35 checklist entries** across the four submenus. Jason's Magazine Styles has **4 of 13**, Lucia's has **6 of 15**, Magazine Tattoos has **12 of 17**, and Luxury Watches has **13 location entries** against the game's 34-watch total. The Bobby Ambrose watch is a documented Chapter 3 reward with unverified counter credit. |
| **Optional Missions** | **32 mission and side-activity entries**, with character, earliest reported chapter, missability, and reward clues. Distinct from the Secret Missions home-menu guide. |
| **Messages in Bottles** | **6 numbered shoreline pickups** around Key Lento and the Shoeless Keys. These have their own checklist and do not count toward publications or Book Swaps. |
| **Missing Persons investigation** | **5 checkable investigation stages**: Starlet Motel poster and warehouse, three missing-tourist reports, and the optional warehouse/mine finale. The guide has **three victims, not five**. |
| **Meteorite Chunks** | **58 guide-reported locations**, including five Selkie Hole tunnel finds, both C0-B8 impact craters, the Mount Kalaga crash camp, and separate grass-edge and tideline pickups at the Capri boat party. The full game total is unconfirmed. |
| **Stunt Jumps** | **130 launch points** grouped by county, with reported launch directions and stunt goals. |
| **Under the Bridge** | **202 bridge crossings**, with individual spans and nearby landmarks. |
| **Knife Flights** | **40 aircraft gaps**. |
| **Base Jumps** | **25 jump starts** with published par-time notes where available. |
| **Location Search Leads** | **17 checkable leads** for map pins that may match an existing pickup or still need their unlock name or counter credit verified. Includes the Underwater Hotel off Key Lento and Halifax's custom '32 Hustler. |
| **Endings** | **12 finale outcomes and proposed variations**: nine reported outcomes or epilogues and three speculative savings-based variants, each with story choices, suggested prerequisites, and confidence notes. |

**Newer meteorite map leads** add the Watson Bay beach dinghy, several different Key Lento tree and rock pickups, a Capri under-dock fragment, Ambrosia river rocks, Port Gellhorn shoreline finds and a Waning Sands waterside campsite. These are guide-reported map points, not an established final total. [GTABase meteorite map](https://www.gtabase.com/gta-6/map/?feature=meteorite).

**Meteorite Chunks** include the five Selkie Hole tunnel finds, two pieces on an SOS island, and locations at the Sandbars and Frampton Islands. New clues cover the beach north of the Derelict Spy Vessel, Dudz Island's metal structure, the Capri beached whale, and the C0-B8 Debris and Fairing craters. [Named meteorite guide](https://gta.tech4gamers.com/gta-6/collectibles/meteorite-chunks/) and [meteorite map pins](https://www.gtabase.com/gta-6/map/?feature=meteorite).

**New watch pickups:** Crowex Octane Midas at Leonida Carrier Warehouse; Novak Laurent Argonaut at Kamar Devon Community Center; B&G Ark Royal Quantum at Watson Bay Safehouse; Crowex Octane Elite at Palmer Fishing Warehouse. An existing B&G Ark Royal Ashore entry now notes alternate gang-operation locations without a second progress checkbox. [Watch research](https://leonipedia.com/guides/luxury-watches/).

**Discovered Styles** includes the Cowboy Horseshoe Mustache and Tiger Eyes makeup, the Freedom Raft and Home Street Home tattoo magazines, and the B&G Ark Royal Ashore watch. The Capri bedroom and Key Lento VR Gamer house safe are tracked as watch location leads while their models are unconfirmed. [Magazine pins](https://www.gtabase.com/gta-6/map/?feature=magazine) and [luxury watch pins](https://www.gtabase.com/gta-6/map/?feature=luxury-watch).

**Missing Persons (story spoilers):** Start with the poster at the Starlet Motel and the warehouse across from Moses' garage. Inspect the warehouse clues, then follow leads at the pink `PRTYGRL` car in the junkyard, the trailer near the water tower, and the submerged car at Wild Hog Point. A final warehouse/mine investigation follows the three missing-person reports. See [KeenGamer's walkthrough](https://www.keengamer.com/articles/guides/gta-6-all-missing-persons-locations-and-serial-killer-walkthrough/) and [GTAXTREME's location guide](https://www.gta-xtreme.com/en/gta-6-missing-persons-locations).

**Alternate publication copies:** If you miss a pickup, look for *Body Copy* at Ajax Gym, *Foiled* at a Grace Avenue garage, *High Rollerz* on a Port Vice cruise ship or at a Bayside Drive gazebo, *¡Bombazo!* in an NW 4th Avenue apartment or a Caledonia Boulevard mobile home, *Drive Shaft* in a yellow Draper Island railway container, and *Endowed* at a south Waning Sands villa. Different copies of the same title share a single checkmark. [GTA 6 GPS publications map](https://gta6gps.com/map/game/magazines).

**No Flusher scans:** Search the *ground-level pavement* with WAiNK rather than scanning walls. The 15 documented sites include street, intersection, and building clues. Some guides report a larger total, so use these as the companion’s mapped locations. See the [street-level location guide](https://thegameswiki.com/gta6/wiki/noflusher-tags), [PowerPyx WAiNK guidance](https://www.powerpyx.com/gta-6-all-no-flusher-tag-utility-markings-locations/), and [WAiNK scan guide](https://game-checklists.com/gta6/waink-scans-checklist/).

**Unique Playlist posters:** Four posters in La Perle/Crosstown unlock *The Sniper of La Perle* under **On Demand**. A Downtown nightclub poster unlocks *The Vault* under **Radio**. Use the five companion checkmarks to track scanned posters. [PowerPyx poster walkthrough](https://www.powerpyx.com/gta-6-all-unique-playlist-collectible-locations/).

**Kenton's Treasures and underwater wrecks:** There are seven named Kenton sets; published completion guides call for five. The *Nuestra Senora del Ocio* wreck in *Island Dreams* yields 10 Gold Doubloons and counts as an underwater discovery, **not** an eighth Kenton set. **Belpoint Strait** and **Bobcat Dive Site** contain Kenton treasures but do not count toward the 33 underwater discoveries. Each available entry lists its known items or piece counts. See [Push Square's dive guide](https://www.pushsquare.com/guides/gta-6-underwater-discoveries-locations-and-kentons-treasures) and [Kenton's Treasures](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures).

**Collection-specific techniques:** For **Cocaine Flamingos**, look for **black-and-white beaks** and destroy the figures; the published guide identifies black-and-yellow beaks as decoration, and collecting the loose cocaine is optional. For **Discovered Styles**, scan applicable magazines with the in-game phone, switch between **Jason and Lucia** for character-specific unlocks, and enter homes where the description calls for it. For **Underwater Discoveries**, finish **Island Dreams** to unlock diving, bring scuba gear, choose the **metal detector** from the lower-right weapon wheel, follow chest markers, and dig when its light turns **green**. The reported adjacent wrecks southeast of Key Lento can be explored during one dive route.

**Efficient shared-location searches:** The Key Lento **Dolphin Dr party house** may yield *Peche*, **Lionel Taber** card, and access to a garage associated with code `092319`. The **Sunset Dr waterfront house** contains a reported *Trigger Happy* copy and **Dartanyon Pearson** card. A **wooded lookout east of Kalaga Reservoir** may yield *11:59* and a **Laird 350**. These are independent finds/checkmarks even when they share a stop. The **Laki Maivia (Bronze)** card in the Cottonmouth Lake trailer is in the **microwave-side safe**, `44017` (bedroom-wall picture/postcard clue).

**Messages in Bottles** follows the numbers written on the six notes. Find note 1 beside the blue boat wreck, note 2 near the south end of the long narrow island, note 3 among rocks on a small sandbar, note 4 on the wedge-shaped sandbar, note 5 on the northeast shore of the curved island, and note 6 on the hook-shaped mangrove island. Examine the bottle on land to read its note. [Numbered walkthrough](https://itemlevel.net/gta-6-all-6-messages-in-bottles-locations-in-the-shoeless-keys-sunken-boat-mystery/) and [bottle map pins](https://www.gtabase.com/gta-6/map/?feature=message-bottle).

**Stunts:** Stunt Jumps, Under the Bridge, Knife Flights, and Base Jumps account for **397 listed challenges**. Published completion guides report that **300** count toward the game's 100% requirement. Mark each stunt in the companion after completing it in-game.

### Collection and stunt sources

- [PowerPyx - GTA VI collectibles and Progress database](https://www.powerpyx.com/gta-6-collectible-guide-progress-database/) and [100% completion checklist](https://www.powerpyx.com/gta-6-100-completion-checklist-guide/).
- [GTA Game Intel - interactive map](https://gtagameintel.com/en/interactive-map/) and [collection markers](https://gtagameintel.com/en/interactive-map/?location=community-1090704).
- [GTA Base - stunt jump map](https://www.gtabase.com/gta-6/map/?feature=stunt-jumps), [WAiNK landmarks](https://www.gtabase.com/gta-6/map/?feature=waink-landmark), and [AHOL Street Art map](https://www.gtabase.com/gta-6/map/?feature=ahol-street-art).
- [Game Checklists - collectible card sets](https://game-checklists.com/gta6/card-sets-checklist/), [Cocaine Flamingos](https://game-checklists.com/gta6/cocaine-flamingos-checklist/), [Discovered Styles](https://game-checklists.com/gta6/styles-checklist/), [underwater discoveries](https://game-checklists.com/gta6/underwater-discoveries-checklist/), and [publications](https://game-checklists.com/gta6/magazines-books-comics-checklist/).
- [GTA6 Life - collectible cards map](https://gta6-life.com/en/tools/interactive-map/collectibles/) and [No Flusher tags](https://gta6-life.com/en/guides/gta-6-graffiti-tags/); [Vice City DB - collectible cards](https://vicecitydb.org/collectibles/collectible-cards).
- [GTA Six Map - WAiNK curiosities](https://www.gtasixmap.com/guides/waink-curiosities) and [sculptures](https://www.gtasixmap.com/guides/waink-sculptures); [The Games Wiki - curiosities](https://thegameswiki.com/gta6/wiki/curiosities), [sculptures](https://thegameswiki.com/gta6/wiki/sculptures), and [AHOL Street Art](https://thegameswiki.com/gta6/wiki/ahol-street-art).
- [GTA 6 Everything - Peewees billboards](https://www.gta6everything.com/activities/peewees-billboards), [Push Square - billboard locations and prize puzzle](https://www.pushsquare.com/guides/gta-6-peewees-billboard-locations-and-grand-prize-puzzle-solution), and [The Games Wiki - billboard locations](https://thegameswiki.com/gta6/wiki/peewee-billboards).
- [Nexzyapp - Kenton's Treasures](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures).
- [GamerHour - Ética Laxa](https://gamerhour.net/guides/all-nuestra-senora-de-la-etica-laxa-treasures-gta-6/), [La Noche](https://gamerhour.net/locations/gta-6-all-nuestra-senora-de-la-noche-treasures-locations/), [Santa Sierra](https://gamerhour.net/locations/gta-6-all-6-urca-de-santa-sierra-treasures-locations/), and [Lujuriosa](https://gamerhour.net/locations/gta-6-all-6-nuestra-senora-lujuriosa-treasures-locations/).
- [GTA Base - meteorite pins](https://www.gtabase.com/gta-6/map/?feature=meteorite), [GTA Scoop - Mount Kalaga crash route](https://gtascoop.com/guides/gta-6-meteorite-locations/), and [GTA6RealMap - Cottonmouth Lake flamingo](https://gta6realmap.com/cocaine-flamingos-and-cards).
- [PowerPyx - Bobby Ambrose watch](https://www.powerpyx.com/gta-6-dubious-provenance-visit-the-antique-store-walkthrough/), [Leonipedia - Marbelle Hotel directions](https://leonipedia.com/guides/luxury-watches/), and [GTA Base - watch pins](https://www.gtabase.com/gta-6/map/?feature=luxury-watch).
- [GTA Base - magazine pins](https://www.gtabase.com/gta-6/map/?feature=magazine), [GTA6 GPS - Underwater Hotel](https://gta6gps.com/?p=gamersky-917805), [TrovePin - Maroon Key treasure](https://trovepin.com/#loc=gs:261:915432), and [TrovePin - Key Lento treasure](https://trovepin.com/#loc=gs:261:915433).
- [Vice City DB - Discovered Styles](https://vicecitydb.org/collectibles/discovered-styles), [GTA6 House - Marbelle watch](https://gta6.house/library/items/crowex-ambassador-breakfast-collectibles-watch), and [PowerPyx - Kingpin Bowling watch](https://www.powerpyx.com/gta-6-hit-kingpin-bowling-walkthrough/).
- [Nexzy - John Morgan card variants](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/vice-city-manatees-cards), [GTA Base - breakable flamingo pins under Hidden Packages](https://www.gtabase.com/gta-6/map/?feature=hidden-packages), and [Hamlet book pin](https://www.gtabase.com/gta-6/map/?feature=magazine#magazine-065).


**Publication chapter cutoffs:** The five copies in Jason's Key Lento house become unavailable after Chapter 1, the two Starlet Motel copies after Chapter 2, and the two Bayside Apartment copies after Chapter 3. These are **missable copies**, not necessarily permanently missable titles; alternate copies may exist. Check these locations before advancing the story. See [PowerPyx's publication guide](https://www.powerpyx.com/gta-6-all-magazines-books-comics-locations/) and the [Key Lento route](https://www.pushsquare.com/guides/gta-6-key-lento-collectibles-and-secrets-walkthrough).

## Activities / Robberies

The **Collections > Activities / Robberies** group contains guide-reported targets with individual checkboxes. It adds no new home-screen icon and uses the same saved-progress format as other Collections. Open an entry for the location clue, chapter unlock, and any special completion rule. All entries are **offline guide references**; the app cannot verify in-game completion.

| Checklist | Tracked entries | Guide-reported completion rule |
| --- | ---: | --- |
| Gang Operations | 39 | Clear all 39 by taking every key valuable. Ultimate Edition-only bonus operation is excluded. |
| Small Bank Jobs | 14 | All 14; Chapter 3 *Credit Check* unlock. Take drawer and safe. |
| Large Bank Vaults | 8 | All 8 for the bank-job total; unlock after Chapter 4 *That's Where The Money Is...*. Take vault, drawer, and safe. |
| Payday Loan Jobs | 7 | All 7; Chapter 3 *Credit Check* unlock. Take teller cash and back-room safe. |
| Fence Sale Milestones | 4 | Manually mark when in-game cumulative sales reach $100k narcotics, $100k jewelry, $250k marked bills, and $200k vehicles. |
| Challenge Milestones | 19 | Two reported vehicle-cheat thresholds per challenge family (5/10 and 10/10), plus 50 challenges for a trophy, 70 for 100%, and all 80 for the P-996 Lazer cheat. These are **19 milestones, not 80 individual checkboxes**. |
| Smash and Grabs | 35 | At least **34/35** for 100%; pawn shops after *Coffee Break*, jewelry after *Yasmani's Jewelry Jobs*. Check the back room. |
| Stick-Up Leads (partial) | 17 named candidate stores | The game counts **91** eligible stick-ups, of which **50** are needed. This list is **not** an exhaustive or verified subset of 91; check the in-game robbery-map filter before claiming completion. |
| Drone Time Trials | 42 trials at 14 venues | All 42. Beat par on first two trials per venue; finish the third once. |
| Shooting Gold Medals | 42 medals at two ranges | All 42 individual gold medals, across 12 courses. |
| Race Challenges | 13 | Gold in at least **10/13**; separate from unique races. |
| Arcade Video Games | 4 | Play all four; simply starting and exiting counts for the guide's 100% target. |
| Pool Opponents | 7 | Beat at least **5/7** different opponents. |
| Golf Courses | 5 total: 3 mini, 2 full | Play **2/3 mini golf** courses. The two regular golf courses are optional reference entries. |
| Unique Race Venues | 13 site leads covering 40 races | **30/40 unique races** needed for 100%; a checked venue means **visited**, not that its races are all won. Individual race names were not published. |
| Basketball Hoops | 100 individually numbered baskets | **90/100 hoops** required for the reported 100% threshold. Score at each distinct basket you check. |
| Hoop Search Leads | 9 older guides' location clues | Earlier checkmarks remain available here while the numbered match for each lead is unconfirmed. |

**Basketball Hoops** includes a hoop and ball location for each guide number from 0 to 99, including the four Hamlet Correctional baskets split between the prison wings and kitchen courts. Pick up the nearby ball, aim, and score before checking off that basket. The same court can have several numbered hoops. [The Games Wiki's numbered locations](https://thegameswiki.com/gta6/wiki/basketball-hoop-locations) and [GTA6Bugs county guide](https://gta6bugs.com/collectibles/basketball-hoops/).

**Challenge milestones:** The game lists **80 named Challenges across 8 families of 10**: Combat, Criminal, Exploration, Fitness, Shopping, Sports, Survival, and Vehicle. Their full objectives are under **Progress > Challenges**. The companion tracks the **5/10 and 10/10 milestone for each family** and the overall **50/70/80** thresholds; it does not claim to store separate checkmarks for all 80. Published guides associate the family milestones with vehicle cheat unlocks. Cheat use remains gated behind story completion, and the app does not activate cheats. For the full named objectives, see the Challenge sources below.

The **22 Bank Jobs** comprise **14 small banks** and **8 large vaults**. Some banks share names and neighborhoods; refer to the entry notes to distinguish each vault. In-game filters: **Map > Robberies** for crime, or **Map > Activities** for racing/sports. Completion thresholds are guide-reported, not official app tracking.

**Related optional missions:** Billy, Andy/Cellerica, Tyrone, Yasmani, Dunk, Eddie Acosta, Shayna, and Yago have individual checklist steps under *Optional Missions*. Shayna's Lucia-only mission and Yago's timed race require prompt attention. Check the *That's Where The Money Is...* optional mission before attempting large vaults. These are not duplicated under *Secret Missions*.

**Bank and racing tips:** Payout ranges are guide estimates, not guaranteed rewards. At small banks, search the drawer and safe; at large vaults, take the vault loot, drawer, and safe, and prepare for a higher wanted level. The 13 Unique Race Venue entries cover access points and unlock requirements; checking a venue does not mark every race held there as complete.

**Gang Operation pickup tips:** The 39 operation entries now identify reported rare-weapon and Standout Vehicle pickups alongside their related watches, within their original checklists. Examples include the **Truth & Justice Caracara** at June Warehouse, **Smuggler Caracara** at Brooks Boatworks, **Firebug Dominator** at La Cascara Motel, **Searing Zombie Bobber** at Farm Fix Meth Lab, and **Royal Ruby Emerus** at Vespucci Mansion. The **Ashore** watch is also reported at Southside Trap House and Saber Shipping Facility; these are alternate acquisition sites, not separate model checkmarks. The **M7 Compound** is also called **Nova Auto Junkyard** in some guides. Safe combinations are guide-reported, and the Safe & Door Codes menu retains conflicting combinations where sources disagree. [PowerPyx operations list](https://www.powerpyx.com/gta-6-all-illegal-gang-operations-locations/), [operation key valuables and codes](https://leonidaplayers.com/guides/gta-6-illegal-operations-all-gang-hideout-locations-loot-safe-codes), and [Insider Gaming pickup inventory](https://insider-gaming.com/all-illegal-operations-locations-key-valuables-in-gta-6/).

### Activities and robbery source guides

- [Game Checklists: first 17 basketball-hoop locations](https://game-checklists.com/gta6/basketball-hoops-checklist/) and [Push Square: detailed hoop finding guide](https://www.pushsquare.com/guides/gta-6-basketball-hoop-locations).

- [PowerPyx: 100% completion categories and thresholds](https://www.powerpyx.com/gta-6-100-completion-checklist-guide/)
- [PowerPyx: all 80 Challenge objectives](https://www.powerpyx.com/gta-6-all-challenges-guide/) and [PowerPyx: cheat unlock requirements](https://www.powerpyx.com/gta-6-all-cheats/)
- [AlcastHQ: eight Challenge groups, conditions, and milestone reward names](https://alcasthq.com/gta-6-challenges/)
- [PowerPyx: 39 Illegal Gang Operations](https://www.powerpyx.com/gta-6-all-illegal-gang-operations-locations/)
- [PowerPyx: 14 small banks and 8 large vaults](https://www.powerpyx.com/gta-6-all-bank-jobs-locations-bank-vaults/)
- [PowerPyx: Payday Loan jobs](https://www.powerpyx.com/gta-6-all-payday-loans-jobs-locations/)
- [PowerPyx: Smash and Grabs](https://www.powerpyx.com/gta-6-all-smash-n-grabs-locations/)
- [Game Checklists: 107-store directory, not 91 confirmed stick-ups](https://game-checklists.com/gta6/stick-ups-checklist/)
- [GTA6Activities: candidate store map](https://www.gta6activities.com/map)
- [PowerPyx: 42 drone time trials](https://www.powerpyx.com/gta-6-all-top-fun-drone-race-time-trial-locations/)
- [PowerPyx: shooting-range gold medals](https://www.powerpyx.com/gta-6-all-shooting-range-gold-medal-challenges/)
- [PowerPyx: race challenges](https://www.powerpyx.com/gta-6-all-race-challenge-locations-gold-medals/)
- [PowerPyx: arcade games](https://www.powerpyx.com/gta-6-all-video-game-locations/)
- [PowerPyx: pool opponents](https://www.powerpyx.com/gta-6-all-pool-opponent-locations/)
- [PowerPyx: golf courses](https://www.powerpyx.com/gta-6-all-golf-course-locations/)
- [PowerPyx: unique racing venues](https://www.powerpyx.com/gta-6-all-unique-race-locations-racing-activities/)

## Vehicle locations, deliveries, and properties

The vehicle sections describe the reported pickup area, the particular vehicle or livery, and any relevant story or access condition. An entry described as a **search area** should not be treated as a guaranteed parking spot.

### Vehicle hunts

**Randy's Wish List has 22 tracked requests.** Find delivery vehicles using reported streets, buildings, parking areas, and mission hints. Examples include:

- **Windsor Drop:** luxury residential driveways in northern Capri.
- **Ocelot Jugular:** outside BITE on Atlantic Avenue in Ocean Beach.
- **Jubilee:** Shore Drive beachfront parking in Ocean Beach; northern Capri is also reported.
- **Itali RSX:** a villa garage on Vespucci Island; an alternate Capri location is reported.
- **Dubsta:** check the **N1N Club** in Downtown, **N-X 94 Vice Car Repair** near VCIA, and hotel parking in Vice Beach or Ocean Beach.
- **Baller ST-D:** beige residential driveways in northern Capri.
- **Übermacht Cypher:** the Übermacht dealer in northern Southside or outside Cirque de Cuisine in Ocean Beach.
- **Grotti Carbonizzare:** beneath Aurum Condominiums in Tequesta, at Status Auto Imports, or in northern Capri public parking.
- **Pfister Neon:** the lakeside mansion north of Belville, Peacock Estates, or northern Capri luxury homes.
- **Truffade Thrax:** a reported final Randy job at the Dalton Island Club parking area, accessible by ferry from Waxman Island. Its availability is mission-dependent.

The game's **Map > Robberies** may offer more current hints for active jobs.

**Moses Drop-Offs has seven deliveries.** The guide covers the Chapter 2 story delivery and six further pickups. After delivering all seven, wait for Moses' text before collecting his **Emperor Sultan** at the Port Gellhorn shop. The reward has its own Standout Vehicle checkmark. [Collect Moses' Gift walkthrough](https://www.powerpyx.com/gta-6-collect-moses-gift-walkthrough/)

**Wyman's Classic Cars has four restoration projects and a fifth checkmark for the final '72 Deviant.** The four restoration entries give directions to 15 wreck parts. Guides identify this as Ultimate Edition content. **Updated access:** The Reynolds Kit Car's Wild Hog Point container needs the cut-off tool from Chapter 3 *Meet the Heders*. The four rebuilt cars cost $1,500 each after Wyman sends a BuckMe request; the final Deviant is a direct $1,000 offer (no fifth wreck search).

### Standout Vehicles

The **280-entry vehicle catalog** covers distinctive cars, motorcycles, aircraft, watercraft, liveries, and mission and stunt rewards. Entries give a pickup landmark, search area, or reward requirement where published.

Examples:

| Vehicle | Reported location |
| --- | --- |
| **Imponte Ruiner Turbo** | Surfside Drive, Key Lento; residential driveway by a wooden fence. |
| **Declasse Akane (anime wrap)** | Dolphin Drive, Key Lento; residential garage. |
| **Annis Yago's Elegy (anime wrap)** | Reward from Chapter 3 Yago finale **Brother to Brother**; keep in garage after the mission. This is a different anime-themed car from the Akane. |
| **Bravado Driveshaft Banshee** | Nova Auto Junkyard, Hamlet. |
| **Bravado Revenant Gauntlet** | Barn in Ambrosia. |
| **Maibatsu Penumbra FF** | Villa garage on Vespucci Island. |
| **Vanessa's Pink Pfister Growler** | Del Rio Blvd pink-bow garage near Auntie's Place, La Perle. Before Chapter 3 ends, unlock the laptop with `MuffinsQueen15`, **read WhatsUp**, and enter garage code `92665`; missable. |
| **Peacock Sovereign** | Garage behind a gas station in Capri. |
| **Street Art Squalo** | Boat trailer near the northeast end of Sunset Drive, Key Lento. |
| **BF Muckspreader Bandito** | Yellow container in a junkyard near Boone Speedway, south of Hamlet; use the Chapter 3 Cut-Off Tool. |
| **Maibatsu Cellerica Caddy** | Green branded golf cart outside Conference Hall B, Belville Cellerica campus, during Lucia's *Peach!* initiation. |
| **Vapid Driveshaft Hustler** | Dolphin Drive, near Jeb's Snack Bar in southeastern Key Lento. |
| **Invetero Spiderweb Coquette** | Party-house garage in southeastern Key Lento; reported keypad code `092319`. |
| **Seashark Sedna (Graffiti)** | Key Lento; the exact dock is unconfirmed. |
| **Pfister Hornet Growler** | Middle section on the second level of an Ocean Beach parking garage. |
| **Lapwing Blazing Mallard** | Bobcat Key airfield hangar. Look for the flame livery. |
| **Buckingham Love Fist Shamal** | A hangar at Vice City International Airport. |
| **Dundreary Sal's Chino** | Port Gellhorn reward for helping Sal fight the Kolts. |
| **Calder Mosaic Dibnah** | Dalton Island dock. Look for the mosaic-patterned boat. |
| **Maibatsu Camo Caddy** | Next to a hut in Buckskin Swamp. Look for camouflage and American flags. |
| **Vapid Peppermint Stanier** | Outside a storefront in La Perle. Look for the pearlescent finish and oversized wheels. |
| **Berserker Sanchez, Black Dragon Maverick and Black Dragon Mallard** | Reported rewards for Stunt Jumps, Under the Bridge and Knife Flights, respectively. |

**Acquisition tips:** The Street Art Squalo needs to be launched from its trailer. Bring the Chapter 3 Cut-Off Tool for the Muckspreader Bandito's shipping container. To see the Cellerica Caddy, progress Andy's Lucia-only missions through *Peach!* at the Belville campus.

**Additional named Standouts and alternate locations (v1.9.5):** The catalog now includes the Cellerica Shamal at VCIA; Raul's Tempesta in the Scoop-container story job; Ultimate Edition Typhoon Squalo, Preziosa Cheetah and Wyman's restored reward vehicles; Memorial Sanchez; Thicc Esskey; GTA+ Venturas BLVD Elegy; and the Malibu Zombie Bobber. The earlier generic Phoenix is now correctly identified as **Blazin' Phoenix (existing ID 176)**, preserving its checkmark. **Seashell Boomazoom** is listed as an identified variant with no reliably documented pickup yet; it is not assigned a fabricated location.

**Alternative pickup reports, one checkmark per vehicle:** Gold Rush Tempesta is reported outside **Cirque de Cuisine (Shore Drive, Ocean Beach)** or on the **upper covered parking deck of Crimson Palms Condominiums (Washington Beach)**; both reports call for the Chapter 3 Key Cloner, but may be conflicting descriptions of one car. Venomous Marshall has reported sightings at the **Boone Speedway car show (southwest Hamlet)** and in the **Land of Plenty barn (eastern Mount Kalaga)**; a third map marker lacks published directions. Guides mark two sites for **Erotic Journey** in Watson Bay and **Pink Licorice Astron** around Belville, without yet providing a dependable second address for either. The museum/VCPD recruitment display notes for Valiant Tempesta may identify the same location. Use WAiNK to confirm identity, and never create extra progress entries solely for additional pins.

**Edition and reward distinction:** Wyman's Classic Cars entries track salvage and restoration tasks; the named restored Standouts track the resulting vehicles. The garage entries for **Paradise Garage** and **GTA+ Crosstown Garage** track property access, while the corresponding unique vehicles are listed by name in Standout Vehicles. Ultimate Edition and GTA+ rewards require the appropriate entitlement, and some unlock only in later chapters.

**Anime vehicle notes:** The Akane on Dolphin Drive is reportedly unlocked and has no tracker; a mod-shop replacement livery may permanently remove its anime wrap. Yago's Elegy is not a free roadside pickup: finish his four-part Jason storyline, including the time-sensitive *Race Against Yago* hourglass mission, and keep the final mission car. Bumper to Bumper requires the Custom Street Racing Build and Nitrous Boost (paid by Yago).

The **pink Pfister Growler** has a checkmark under Standout Vehicles; it is not a separate garage collectible. The **Gold Rush Tempesta** has conflicting published spawn clues at Crimson Palms Condominiums in Washington Beach and Cirque de Cuisine on Shore Drive. Compare the **Gold Tempesta lead** with the Gold Rush WAiNK name and livery before checking them as two different vehicles.

### Garages and safehouses

**Garages** covers **15 purchase, story, and membership/edition entries**, including published prices, storage capacity, access conditions, and benefits: eight purchasable garages, four story rewards, and three edition or GTA+ benefits. **Houses / Cribs** lists **five story safehouses**, including the Chapter 5 **Pegassi Residences penthouse** on Bayside Dr, Tequesta Retreat. Its three-slot garage has its own entry under Garages. [Safehouse map pin](https://www.gtabase.com/gta-6/map/?feature=safehouses#safehouse-006)

### Vehicle and property sources

- [GTA Base - Muckspreader Bandito](https://www.gtabase.com/gta-6/vehicles/muckspreader-bandito), [GTA6Wiki - Street Art Squalo](https://gta6wiki.eu/vehicles/street-art-squalo/), [PowerPyx - Peach! initiation](https://www.powerpyx.com/gta-6-peach-join-the-cellerica-initiation-walkthrough/), and [GTA6 GPS - vehicle access notes](https://gta6gps.com/map/game/unique-vehicles).
- [GAMETA - mapped Standout Vehicles and named variants](https://gameta.gg/gta6/map/standout-vehicles/); [Ultimate Edition vehicle and reward timing](https://gameta.gg/gta6/guides/ultimate-edition-items/); [GTA Base - Standout locations](https://www.gtabase.com/gta-6/map/?feature=standout-vehicle).
- [GTA6Wiki - competing Gold Rush Tempesta pickup sites](https://gta6wiki.eu/vehicles/gold-rush-tempesta/); [GTA6Wiki - Venomous Marshall](https://gta6wiki.eu/vehicles/venomous-marshall/); [GTA Base - Crosstown GTA+ bundle](https://www.gtabase.com/gta-6/properties/gta-plus-crosstown-garage).
- [Malibu Zombie Bobber portal walkthrough](https://www.keengamer.com/articles/guides/how-to-open-the-vice-city-portal-in-gta-6-and-get-tommy-vercettis-shirt/); [GTA Base - Malibu Zombie Bobber](https://www.gtabase.com/gta-6/vehicles/malibu-zombie-bobber).
- [Akane anime-car pickup report](https://www.aprasi.com/blogs/trending-topics/gta-6-akane-anime-car) (single-guide security/wrap details; verify in-game).
- [GTA Base - Yago's Elegy](https://www.gtabase.com/gta-6/vehicles/yagos-elegy), [PowerPyx - Bumper to Bumper](https://www.powerpyx.com/gta-6-bumper-to-bumper-mentor-yago-walkthrough/), [PowerPyx - Race Against Yago](https://www.powerpyx.com/gta-6-race-against-yago-walkthrough/), and [PowerPyx - Brother to Brother](https://www.powerpyx.com/gta-6-brother-to-brother-back-up-yago-walkthrough/).
- [PC Gamer - Wyman Transgressor](https://www.pcgamer.com/games/grand-theft-auto/gta-6-transgressor-locations/), [Riata](https://www.pcgamer.com/games/grand-theft-auto/gta-6-riata-locations/), [Mamba GT](https://www.pcgamer.com/games/grand-theft-auto/gta-6-mamba-gt-locations/), and [Kit Car](https://www.pcgamer.com/games/grand-theft-auto/gta-6-kit-car-locations/); [GTA Base - GTA+ Crosstown Garage](https://www.gtabase.com/gta-6/properties/gta-plus-crosstown-garage).
- [Push Square - Moses vehicle drop-offs](https://www.pushsquare.com/guides/gta-6-moses-vehicle-drop-off-locations) and [PowerPyx - Moses walkthrough](https://www.powerpyx.com/gta-6-steal-vehicles-for-moses-walkthrough/).
- [Push Square - Randy's Wish List](https://www.pushsquare.com/guides/gta-6-randys-wish-list-vehicle-locations), [Radio Times - Randy's vehicle locations](https://www.radiotimes.com/technology/gaming/gta-6-randys-wishlist-cars/), [GTA Base - vehicle requests](https://www.gtabase.com/gta-6/map/?feature=vehicle-request), and [GTA6Map - Randy markers](https://gta6map.io/location/randy-wish-list).
- [AllGTA World - Randy parking leads](https://allgtaworld.com/guides/gta-6-randy-wish-list), [GTA6Values - Capri parking leads](https://www.gta6values.com/randys-wish-list/), and [Game Secret - Cirque de Cuisine sighting](https://game-secret.com/guides/grand-theft-auto-vi/all-randys-wish-list-vehicle-locations-in-gta-6/).
- [PowerPyx - trophies and vehicle-request context](https://www.powerpyx.com/gta-6-grand-theft-auto-vi-trophy-guide-roadmap/).
- [GTA Base - standout vehicles map](https://www.gtabase.com/gta-6/map/?feature=standout-vehicle) and [GTA6Map - standout locations](https://gta6map.io/location/standout-vehicle).
- [MetaForge - Standout pickups and access](https://metaforge.app/gta-6/gta-6-standout-vehicles-locations).
- [GAMETA - Berserker Sanchez](https://gameta.gg/gta6/vehicles/berserker-sanchez/), [Black Dragon Maverick](https://gameta.gg/gta6/vehicles/black-dragon-maverick/), and [Black Dragon Mallard](https://gameta.gg/gta6/vehicles/black-dragon-mallard/).
- [PowerPyx - Hamlet impound Phoenix](https://www.powerpyx.com/gta-6-unwanted-attention-go-see-cal-walkthrough/) and [GTA6 House - Halifax's '32 Hustler](https://gta6.house/library/vehicles/vapid-32-hustler).
- [The Games Wiki - Driveshaft Hustler](https://thegameswiki.com/gta6/wiki/driveshaft-hustler), [Spiderweb Coquette](https://thegameswiki.com/gta6/wiki/invetero-spiderweb-coquette), and [Wyman's cars](https://thegameswiki.com/gta6/wiki/wymans-cars).
- [GTA Base - garages](https://www.gtabase.com/gta-6/articles/how-to-unlock-garages-in-gta-6), [Leonida Waypoint - properties](https://leonidawaypoint.com/database/properties), and [Output Lag - safehouses](https://outputlag.com/guides/gta-6-safehouses/).

## PS5 and Xbox Cheats

Choose **PS5 Cheats** or **Xbox Cheats** to browse **61 reported effects** grouped by type. The catalog includes **19 published D-pad inputs per platform**, unlock notes, warnings, and **28 screenshot-derived sequences marked unverified**. Twenty-four screenshot sequences appear in the separate **Unverified codes** list, while four correspond to main-catalog effects. Xbox labels translated from PS5 screenshots are not independently captured Xbox codes.

Published guides describe entering cheats from the game's phone in free roam: **D-pad Up**, then **L1 + R1** on PS5 or **LB + RB** on Xbox, followed by the sequence. See the in-game **Progress > Cheats** area for availability and unlock information where applicable. Guides warn that using cheats may affect saving and trophies or achievements. **Save before enabling cheats.**

The app displays codes and instructions; it does not send controller inputs. Screenshot-only inputs and the single-source **Pinhole** entry are expressly marked as reports rather than confirmed sequences.

### Cheat sources

- [PowerPyx - GTA VI cheat list](https://www.powerpyx.com/gta-6-all-cheats/).
- [GamesRadar+ - cheat guide](https://www.gamesradar.com/games/grand-theft-auto/gta-6-cheats/).
- [PC Gamer - cheat list](https://www.pcgamer.com/games/grand-theft-auto/gta-6-cheats-list/).
- [TrueAchievements - reported codes](https://www.trueachievements.com/news/gta-6-cheat-codes).
- [Nerdism - PS5 and Xbox comparison](https://nerdism.com/gta-6-cheat-codes-live/).
- [Respec - inputs and reported unlocks](https://www.respec.net/guide/318-gta-6-cheats-codes-list-unlock-requirements-ps5-xbox) and [GTA6Database - cheat status](https://www.gta6database.com/cheats).
- [GTA6DB - code catalog](https://gta6db.io/cheats).

## Mock Loaders

The app ships **45 full-screen images**, with **30 GTA VI** scenes and **15 Dungeons & Dolphins** scenes. The gallery browses the enabled sets. Press **Left** or **Right** to switch images and **Back** to exit.

**Mock Loader Settings** offers separate toggles for the GTA VI and Dungeons & Dolphins sets. Use **Up/Down** to choose a set and **OK** to enable or disable it. The default gallery shows GTA VI. Enable Dungeons & Dolphins to add its 15 images. Disabling both sets displays **No sets enabled** in the gallery.

When the app launches, it shows a **random image from the enabled collection(s) for 0.5 seconds**, then opens the main menu. If both sets are disabled, the main menu appears without a splash. Image files are loaded individually for display.

## About the reference data

GTA VI Companion is an **unofficial fan reference** based on linked third-party guides, interactive maps, screenshots, and community discussions. These are **published reports, not official Rockstar data**. Codes, collectible positions, vehicle spawns, mission availability, prices, and exploit behavior can depend on story progress, character, time, or game patches. A published map pin or general search area does not establish an exact, repeatable spawn.

Where sources disagree or a position is not specific, entries use labels such as **reported**, **unconfirmed**, **variable**, and **conflicting**. Collection totals in this README describe entries available in the app and should not be interpreted as a claim that every in-game collectible is cataloged.

## Project and community

- [GTA VI Companion source repository](https://github.com/RogueMaster/GTA_VI_Companion)
- [Patreon](https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release)
