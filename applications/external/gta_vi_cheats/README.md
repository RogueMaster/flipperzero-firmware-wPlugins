<p>
  <h1 align="center"><b>Hello, I'm <a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a> 👋</b></h1>
</p>
<h1 align="center"><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1><br>

# GTA VI Companion

**An offline Grand Theft Auto VI Story Mode guide, location reference, and collection tracker for Flipper Zero.**

GTA VI Companion puts collectible checklists, vehicle locations, stunt challenges, property information, cheat inputs, safe and door combinations, and an illustrated loading-screen gallery in a handheld app. It works as a **reference and progress tracker**: it does not connect to the game, enter cheats on your behalf, unlock content, or change an in-game save.

Open **GTA VI Companion** from the Flipper Zero **Tools** menu. The guide data and images are packaged with the app; checklist progress and loader preferences are saved to the Flipper's SD card.

## Features

The home screen has 16 icons across two pages. The first page groups gameplay guides, collections, Dispatch, cheats, and quick help. The second page groups vehicle hunts, properties, and loading-screen controls.

| Menu | What you'll find |
| --- | --- |
| **Infinite Money Glitch** | A five-step walkthrough of the reported Apollo Aggregates loot-respawn method, including vehicle storage, three possible reset approaches, and cash-out locations. |
| **PS5 Cheats** | Reported PlayStation controller sequences, effects, unlock prerequisites, and unverified-input labels. |
| **Xbox Cheats** | Reported Xbox inputs and effects, including confidence notes for converted sequences. |
| **Collections** | Checklists and directions for collectible cards, WAiNK scans, art, treasure, flamingos, publications, styles, and stunt challenges. |
| **Safe & Door Codes** | 38 reported safe combinations, door codes, and puzzle passwords, with location clues and source-confidence labels. |
| **Secret Missions** | 16 checkable optional encounters, hidden weapon finds, cash routes, stat rewards, and Easter eggs, grouped by the earliest reported story chapter. Uncertain unlocks appear at the end. |
| **Randy's Wish List** | 19 vehicle requests with reported pickup clues, access conditions, and delivery progress. |
| **Moses Drop-Offs** | Seven reported vehicle delivery jobs and their location clues. |
| **Wyman's Classic Cars** | Five restoration projects covering 15 wreck parts and the final pickup. |
| **Standout Vehicles** | 105 unusual vehicles and variants, with parking, garage, dock, and landmark clues where published. |
| **Garages** | 15 garage entries with location, capacity, acquisition cost, or unlock requirements where known. |
| **Houses / Cribs** | Four story safehouses with locations, chapter access, and facilities. |
| **Dispatch** | Region-based browsing of 839 indexed pickups and activities, with progress tracking and a next-unchecked shortcut. |
| **Mock Loaders** | A gallery of 45 full-screen images: 30 GTA VI scenes and 15 Dungeons & Dolphins scenes. |
| **Mock Loader Settings** | Separate enable/disable controls for the GTA VI and Dungeons & Dolphins startup image sets. |
| **Quick guide / warnings** | Button controls, cheat entry, saving precautions, and achievement warnings. |

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
| **Collections** | Each cataloged collectible or discovery in its category, including cards, flamingos, WAiNK, artwork, treasures, and published location entries. |

**Right** toggles the selected entry from its list. **OK** opens its details, where the center **Check / Undo** action also toggles completion. An empty category marked *Locations pending* has no entries to check until location data is available. The count in each list header reflects checked entries, and returning to a list updates the checkmark state. All these lists share one persistent tracking system, so checking a bridge or vehicle through a different route does not create a second completion record.

Checklist progress is saved automatically on the SD card in **`/ext/apps_data/gta_vi_cheats/`**. Files `progress.a` and `progress.b` store collection and Dispatch checkmarks. Secret Missions have their own `secrets.a` and `secrets.b` files so mission status can be checked or unchecked without affecting other collections. Back up these files before reformatting or replacing the SD card.

For **Secret Missions**, press **OK** to read the walkthrough, **Right** to check or uncheck an entry in the list, or choose **Check / Uncheck** with **OK** in its detail view. The menu shows the number completed out of 16, and completion is restored when the app restarts.

### Region-aware Dispatch

Dispatch groups **839 location stops** into **Vice City**, **Leonida Keys**, **Grassrivers**, **Port Gellhorn**, **Ambrosia**, **Mount Kalaga**, **Other areas**, and **All regions**.

Choose a region to browse its stops. Press **OK** to change a stop's checkmark, **Right** to jump to the next unchecked stop, or **Back** to choose another region. Checks are shared with the corresponding collection or vehicle-request list when that item is indexed in Dispatch.

**Other areas** includes places outside the six main regions and locations with uncertain region assignments. Alternate spawns can appear under more than one region. The Dispatch list does not include Standout Vehicles or every Randy's Wish List entry; those are accessible through their own menus.

## Infinite Money Glitch

The guide describes a **reported** Story Mode loot-respawn method at **Apollo Aggregates, east or northeast of Hamlet**. The reported landmark is a **red barn/warehouse with a van decorated with teddy bears**. The process depends on collecting loot, placing bags in a suitable vehicle, refreshing the area, and cashing out at a fence or lockup. It is not an instant-money cheat.

1. **Locate the warehouse.** Travel to the Apollo Aggregates site with a vehicle that has cargo storage. A separate game save is advisable.
2. **Collect valuables.** Search the vans, warehouse, office, and safe for cash, cocaine, and other reported loot.
3. **Fill vehicle storage.** Put collected bags in the vehicle before attempting to refresh the location.
4. **Refresh the loot.** Guides describe three alternative approaches: driving away and returning, switching Lucia → Jason → Lucia with Jason far enough away, or saving and reloading after stashing the bags. Results can differ by chapter and game update.
5. **Sell or deposit.** Repeat only if items respawn, then use an unlocked fence or **Brian's Lockup**, for which `7700` is a published keypad code. Payout and inventory behavior may vary across story transitions.

The menu explains these steps in greater detail. **Back up your game save** before experimenting with an exploit; its behavior is not guaranteed.

### Money-glitch sources

- [VICE — Apollo Aggregates loot method](https://www.vice.com/en/article/gta-6-money-glitch/) and [character switching](https://www.vice.com/en/article/how-to-switch-characters-in-gta-6/).
- [Insider Gaming — leave-area reset](https://insider-gaming.com/gta-6-infinite-money-glitch/).
- [KeenGamer — save/reload method](https://www.keengamer.com/articles/guides/gta-6-unlimited-money-glitch-guide/).
- [Guías GTA 6 — Brian's Lockup method](https://guiasgta6.com/en/guides/gta-6-unlimited-money-glitch-brians-lockup/).
- [Reddit r/gtaglitches — community discussion](https://www.reddit.com/r/gtaglitches/comments/1y1gou5/gta_6_unlimited_money_red_barn_apollo_aggregates/).

## Secret Missions and Easter eggs

**Secret Missions** is a guide to 16 optional activities, hidden weapon pickups, cash opportunities, and Easter eggs. Entries are organized by the **earliest reported chapter or prerequisite**: Chapter 1 (with missable opportunities first), later Chapter 1 unlocks, Chapter 2 and its follow-ups, then locations with **unknown story requirements**. Within a chapter, exploration activities can usually be attempted in different orders. Check off activities as you finish them; you can uncheck any entry later. This is a discovery guide, **not** a required mission sequence. The menu uses `C1`, `C1+`, `C2`, and `C2+` for reported story windows; `?` means the chapter requirement has not been established.

| Earliest access | Hidden activity | Location / how to start | Outcome |
| --- | --- | --- | --- |
| Chapter 1 | **Three Sheets talking parrot** | Behind Three Sheets Liquor in Key Lento during Shirley's rum errand | Hidden cash near generator and in safe |
| Chapter 1 only | **Cal's lost drugs** | Follow Cal's texts before chapter end | Sellable packages; missable |
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

- [The Big Lead — Hyde Island Manhunt and Island Poacher FF-50](https://www.thebiglead.com/saw-style-killer-gta-6/); [Cultured Vultures — Hyde Island mission walkthrough](https://culturedvultures.com/gta-6-how-to-find-the-manhunt-mission/); [GTA Base — Island Poacher variant](https://www.gtabase.com/gta-6/weapons/island-poacher-ff-50).
- [LeonidaHub — Apollo warehouse and parrot cash](https://leonidahub.org/en/news/easy-early-money-in-gta-6-the-apollo-aggregates-warehouse-and-the-parrot-that-knows-where-the-cash-is); [GTA6CommunityWiki — Chapter 1 $20K route](https://gta6communitywiki.com/tips/early-money-route); [GTA6Direct — warehouse valuables](https://gta6direct.com/free-loot).
- [GTA6.free — Permit Keys, outpost and free weapons](https://gta6.free/weapons/gta-6-unique-weapon-locations); [GTA6 Vice City Hub — secret weapon leads](https://www.gtavicecityhub.com/secrets).
- [GamesRadar+ — The Final Flush and permanent stamina](https://www.gamesradar.com/games/grand-theft-auto/gta-6-final-flush-increase-maximum-stamina/); [PowerPyx — optional mission chapter windows](https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/); [Nexzy — missable optional missions](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/gta-6-optional-missions).
- [The Big Lead — UFO dream, airfield keypad and stat rewards](https://www.thebiglead.com/gta-6-keypad-door-code-south-point-airfield/); [Vice City DB — alien rescue evidence](https://vicecitydb.org/easter-eggs/alien-rescue-dream-quest).
- [Indiatimes — Satan ritual and reported shotgun reward](https://www.indiatimes.com/game-guides/how-to-summon-and-kill-satan-in-gta-6-satanist-locations-hell-mission-beelzebub-dave-encounter-and-exclusive-shotgun-reward-explained/articleshow/136066345.html); [GTA Scoop — additional secrets and hidden radio stations](https://gtascoop.com/guides/gta-6-easter-eggs-and-secrets/).

**Source confidence:** These are third-party launch-period reports, not gameplay-tested by the companion. Names, spawns, unlock chapters, puzzle combinations and rewards can vary or be corrected by newer guides. Unknown requirements are explicitly labeled rather than inferred from their location.

## Safe & Door Codes

An offline catalog of **38 published codes and passwords**. Choose **All codes**, **Mission safes**, **Operations**, **Doors / garages**, **Puzzles / other**, or **Random safe tips**. Each entry shows the published combination, area, landmark or access instructions, story context when known, and a confidence label. Codes appear at the start of list rows so they are visible on the Flipper's small display.

| Category | Entries | Examples |
| --- | ---: | --- |
| **Mission safes** | **8** | Delights Cabaret `2008`; Donovan Banks & Co. `0104`. |
| **Illegal Operations** | **14** | Nova Auto Junkyard `7777`; Kingpin Bowling `0300` (conflicting report). |
| **Doors / garages** | **10** | Brian's Lockup `7700`; Key Lento Party House `092319`; Mount Kalaga Tower `69185` (conflicting report). |
| **Puzzles / other** | **6** | Pelican Valley research codes and two text passwords. |
| **Total** | **38** | Four- to six-digit PINs and text passwords. |

**Leading zeros matter.** Published sources disagree on Kingpin Bowling (`0300`/`0330`), Saber Shipping Facility (`1122`/`1965`), and Mount Kalaga Tower (`69185`/`691865`). South Point Airfield `2341` is also disputed. Certain robbery safes may randomize their combinations; if a number fails, look for an in-game note, computer, or other clue.

**Confidence labels:** *Guide-tested* means the linked guide publisher reports testing it, not that the companion author tested it; *Multiple reports* means more than one published source; *Reported* means limited corroboration; *Variable* means the result may differ by session or replay; and *Conflicting* means sources disagree. A `?` beside a code signals uncertainty.

### Safe and door code sources

- [VICE — Illegal Operations safe codes](https://www.vice.com/en/article/all-gta-6-safe-codes-illegal-operations/).
- [GamesRadar+ — safe codes](https://www.gamesradar.com/games/grand-theft-auto/gta-6-safe-codes/).
- [GTA Guide — safes, doors, and garages](https://gta.guide/gta-6/guides/safe-door-garage-codes).
- [GTA 6 Player Tools — mission and puzzle codes](https://gta6playertools.com/locations/safe-codes).
- [GTA6Heists — robbery safe clues](https://www.gta6heists.com/robberies/safe-codes).
- [Radio Times — door codes](https://www.radiotimes.com/technology/gaming/gta-6-door-codes-list).
- [ToGame — safe-code catalog](https://togame.io/b/gta-6-safe-codes/).
- [GTA Base — safe-location map](https://www.gtabase.com/gta-6/map/?feature=safe).

## Collections and exploration

Select **Collections** to browse categories and individual sets. Each entry provides the available location, landmark, chapter, or activity clue and a checkmark for tracking completion.

| Collection or activity | Content |
| --- | --- |
| **Collectible Cards** | **480 cards across 18 sets**, organized by card set with pickup clues. The John Morgan (Dusty Pink) card has a reported Casanova Island area; its exact pickup is unconfirmed. |
| **WAiNK** | **31 landmarks**, **35 curiosities**, **23 sculptures**, and **15 mapped No Flusher tags**, with identifying landmarks and scan instructions where available. |
| **Peewees Billboards** | **4 roadside billboards** with highway, interchange, and nearby-business clues, plus the reported Belville reward puzzle. |
| **AHOL Street Art** | **112 art scans** organized by art type, including an option to mark a whole type complete. |
| **1010 Murals** | **5 photo locations** and framing clues. |
| **Kenton's Treasures** | **7 treasure sets** after the tutorial wreck, with diving areas and piece counts. |
| **Underwater Discoveries** | **6 named dive or underwater locations**. |
| **Magazines, Books & Comics** | **26 publication entries**, including alternate copies and availability notes where published. |
| **Book Swaps** | **7 Little Library locations**. |
| **Meditations** | **18 character-specific completions** across **10 places**, with separate Jason and Lucia entries when applicable. |
| **Cocaine Flamingos** | **30 reported pickups**, including street, building, and landmark directions for locations in Capri, Key Lento, and other areas. |
| **Unique Playlists** | **2 playlists** and **5 related poster locations**. |
| **Discovered Styles** | **21 clothing and cosmetic discoveries**, including watches, the Backyard Wrestling Belt, and Luchadora tattoos. |
| **Stunt Jumps** | **130 launch points** grouped by county, with reported launch directions and stunt goals. |
| **Under the Bridge** | **202 bridge crossings**, with individual spans and nearby landmarks. |
| **Knife Flights** | **40 aircraft gaps**. |
| **Base Jumps** | **25 jump starts** with published par-time notes where available. |

**Stunts:** Stunt Jumps, Under the Bridge, Knife Flights, and Base Jumps account for **397 listed challenges**. Published completion guides report that **300** count toward the game's 100% requirement. Mark each stunt in the companion after completing it in-game.

### Collection and stunt sources

- [PowerPyx — GTA VI collectibles and Progress database](https://www.powerpyx.com/gta-6-collectible-guide-progress-database/) and [100% completion checklist](https://www.powerpyx.com/gta-6-100-completion-checklist-guide/).
- [GTA Game Intel — interactive map](https://gtagameintel.com/en/interactive-map/) and [collection markers](https://gtagameintel.com/en/interactive-map/?location=community-1090704).
- [GTA Base — stunt jump map](https://www.gtabase.com/gta-6/map/?feature=stunt-jumps), [WAiNK landmarks](https://www.gtabase.com/gta-6/map/?feature=waink-landmark), and [AHOL Street Art map](https://www.gtabase.com/gta-6/map/?feature=ahol-street-art).
- [Game Checklists — collectible card sets](https://game-checklists.com/gta6/card-sets-checklist/), [Cocaine Flamingos](https://game-checklists.com/gta6/cocaine-flamingos-checklist/), [Discovered Styles](https://game-checklists.com/gta6/styles-checklist/), [underwater discoveries](https://game-checklists.com/gta6/underwater-discoveries-checklist/), and [publications](https://game-checklists.com/gta6/magazines-books-comics-checklist/).
- [GTA6 Life — collectible cards map](https://gta6-life.com/en/tools/interactive-map/collectibles/) and [No Flusher tags](https://gta6-life.com/en/guides/gta-6-graffiti-tags/); [Vice City DB — collectible cards](https://vicecitydb.org/collectibles/collectible-cards).
- [GTA Six Map — WAiNK curiosities](https://www.gtasixmap.com/guides/waink-curiosities) and [sculptures](https://www.gtasixmap.com/guides/waink-sculptures); [The Games Wiki — curiosities](https://thegameswiki.com/gta6/wiki/curiosities), [sculptures](https://thegameswiki.com/gta6/wiki/sculptures), and [AHOL Street Art](https://thegameswiki.com/gta6/wiki/ahol-street-art).
- [GTA 6 Everything — Peewees billboards](https://www.gta6everything.com/activities/peewees-billboards), [Push Square — billboard locations and prize puzzle](https://www.pushsquare.com/guides/gta-6-peewees-billboard-locations-and-grand-prize-puzzle-solution), and [The Games Wiki — billboard locations](https://thegameswiki.com/gta6/wiki/peewee-billboards).
- [Nexzyapp — Kenton's Treasures](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures).
- [Vice City DB — Discovered Styles](https://vicecitydb.org/collectibles/discovered-styles), [GTA6 House — Marbelle watch](https://gta6.house/library/items/crowex-ambassador-breakfast-collectibles-watch), and [PowerPyx — Kingpin Bowling watch](https://www.powerpyx.com/gta-6-hit-kingpin-bowling-walkthrough/).

## Vehicle locations, deliveries, and properties

The vehicle sections describe the reported pickup area, the particular vehicle or livery, and any relevant story or access condition. An entry described as a **search area** should not be treated as a guaranteed parking spot.

### Vehicle hunts

**Randy's Wish List — 19 requests.** Find delivery vehicles using reported streets, buildings, parking areas, and mission hints. Examples include:

- **Windsor Drop:** luxury residential driveways in northern Capri.
- **Ocelot Jugular:** outside BITE on Atlantic Avenue in Ocean Beach.
- **Jubilee:** Shore Drive beachfront parking in Ocean Beach; northern Capri is also reported.
- **Itali RSX:** a villa garage on Vespucci Island; an alternate Capri location is reported.
- **Dubsta:** a reported Vice Beach parking area, without a confirmed specific lot.
- **Baller ST-D:** beige residential driveways in northern Capri.
- **Truffade Thrax:** a reported final Randy job at the Dalton Island Club parking area, accessible by ferry from Waxman Island. Its availability is mission-dependent.

The game's **Map > Robberies** may offer more current hints for active jobs.

**Moses Drop-Offs — 7 vehicles.** The guide covers the Chapter 2 story delivery and six further vehicle pickups, with drop-off instructions and activity clues.

**Wyman's Classic Cars — 5 restoration projects.** Find the reported 15 wreck parts and follow the final `'72 Deviant` pickup instructions. The collection is identified by guides as Ultimate Edition content.

### Standout Vehicles

The **105-entry vehicle catalog** covers distinctive cars, motorcycles, watercraft, liveries, and other unusual spawns. Each entry provides an available area, street, driveway, garage, yard, dock, or landmark clue.

Examples:

| Vehicle | Reported location |
| --- | --- |
| **Imponte Ruiner Turbo** | Surfside Drive, Key Lento; residential driveway by a wooden fence. |
| **Declasse Akane (anime wrap)** | Dolphin Drive, Key Lento; residential garage. |
| **Bravado Driveshaft Banshee** | Nova Auto Junkyard, Hamlet. |
| **Bravado Revenant Gauntlet** | Barn in Ambrosia. |
| **Maibatsu Penumbra FF** | Villa garage on Vespucci Island. |
| **Peacock Sovereign** | Garage behind a gas station in Capri. |
| **Street Art Squalo** | Boat trailer near the northeast end of Sunset Drive, Key Lento. |
| **Vapid Driveshaft Hustler** | Dolphin Drive, near Jeb's Snack Bar in southeastern Key Lento. |
| **Invetero Spiderweb Coquette** | Party-house garage in southeastern Key Lento; reported keypad code `092319`. |
| **Seashark Sedna (Graffiti)** | Key Lento; the exact dock is unconfirmed. |

The **Gold Rush Tempesta** has conflicting published spawn clues: Crimson Palms Condominiums in Washington Beach and Cirque de Cuisine on Shore Drive. The catalog describes uncertain locations as reported alternatives rather than guaranteed spawns.

### Garages and safehouses

**Garages** covers **15 purchase, story, and membership/edition entries**, including published prices, storage capacity, access conditions, and benefits: eight purchasable garages, four story rewards, and three edition or GTA+ benefits. **Houses / Cribs** lists **4 story safehouses**, with their general locations, story unlocks, and facilities.

### Vehicle and property sources

- [Push Square — Moses vehicle drop-offs](https://www.pushsquare.com/guides/gta-6-moses-vehicle-drop-off-locations) and [PowerPyx — Moses walkthrough](https://www.powerpyx.com/gta-6-steal-vehicles-for-moses-walkthrough/).
- [Push Square — Randy's Wish List](https://www.pushsquare.com/guides/gta-6-randys-wish-list-vehicle-locations), [Radio Times — Randy's vehicle locations](https://www.radiotimes.com/technology/gaming/gta-6-randys-wishlist-cars/), [GTA Base — vehicle requests](https://www.gtabase.com/gta-6/map/?feature=vehicle-request), and [GTA6Map — Randy markers](https://gta6map.io/location/randy-wish-list).
- [PowerPyx — trophies and vehicle-request context](https://www.powerpyx.com/gta-6-grand-theft-auto-vi-trophy-guide-roadmap/).
- [GTA Base — standout vehicles map](https://www.gtabase.com/gta-6/map/?feature=standout-vehicle) and [GTA6Map — standout locations](https://gta6map.io/location/standout-vehicle).
- [The Games Wiki — Driveshaft Hustler](https://thegameswiki.com/gta6/wiki/driveshaft-hustler), [Spiderweb Coquette](https://thegameswiki.com/gta6/wiki/invetero-spiderweb-coquette), and [Wyman's cars](https://thegameswiki.com/gta6/wiki/wymans-cars).
- [GTA Base — garages](https://www.gtabase.com/gta-6/articles/how-to-unlock-garages-in-gta-6), [Leonida Waypoint — properties](https://leonidawaypoint.com/database/properties), and [Output Lag — safehouses](https://outputlag.com/guides/gta-6-safehouses/).

## PS5 and Xbox Cheats

Choose **PS5 Cheats** or **Xbox Cheats** to browse **61 reported effects** grouped by type. The catalog includes **19 published D-pad inputs per platform**, unlock notes, warnings, and **28 screenshot-derived sequences marked unverified**. Twenty-four screenshot sequences appear in the separate **Unverified codes** list, while four correspond to main-catalog effects. Xbox labels translated from PS5 screenshots are not independently captured Xbox codes.

Published guides describe entering cheats from the game's phone in free roam: **D-pad Up**, then **L1 + R1** on PS5 or **LB + RB** on Xbox, followed by the sequence. See the in-game **Progress > Cheats** area for availability and unlock information where applicable. Guides warn that using cheats may affect saving and trophies or achievements. **Save before enabling cheats.**

The app displays codes and instructions; it does not send controller inputs. Screenshot-only inputs and the single-source **Pinhole** entry are expressly marked as reports rather than confirmed sequences.

### Cheat sources

- [PowerPyx — GTA VI cheat list](https://www.powerpyx.com/gta-6-all-cheats/).
- [GamesRadar+ — cheat guide](https://www.gamesradar.com/games/grand-theft-auto/gta-6-cheats/).
- [PC Gamer — cheat list](https://www.pcgamer.com/games/grand-theft-auto/gta-6-cheats-list/).
- [TrueAchievements — reported codes](https://www.trueachievements.com/news/gta-6-cheat-codes).
- [Nerdism — PS5 and Xbox comparison](https://nerdism.com/gta-6-cheat-codes-live/).
- [Respec — inputs and reported unlocks](https://www.respec.net/guide/318-gta-6-cheats-codes-list-unlock-requirements-ps5-xbox) and [GTA6Database — cheat status](https://www.gta6database.com/cheats).
- [GTA6DB — code catalog](https://gta6db.io/cheats).

## Mock Loaders

The **Mock Loaders** gallery displays **45 full-screen images**: **30 GTA VI** scenes and **15 Dungeons & Dolphins** scenes. In the gallery, press **Left** or **Right** to browse images and **Back** to exit.

**Mock Loader Settings** offers separate toggles for the GTA VI and Dungeons & Dolphins sets. Use **Up/Down** to choose a set and **OK** to enable or disable it. By default the GTA VI collection is enabled and Dungeons & Dolphins is disabled.

When the app launches, it shows a **random image from the enabled collection(s) for 0.5 seconds**, then opens the main menu. If both sets are disabled, the main menu appears without a splash. Image files are loaded individually for display.

## About the reference data

GTA VI Companion is an **unofficial fan reference** based on linked third-party guides, interactive maps, screenshots, and community discussions. These are **published reports, not official Rockstar data**. Codes, collectible positions, vehicle spawns, mission availability, prices, and exploit behavior can depend on story progress, character, time, or game patches. A published map pin or general search area does not establish an exact, repeatable spawn.

Where sources disagree or a position is not specific, entries use labels such as **reported**, **unconfirmed**, **variable**, and **conflicting**. Collection totals in this README describe entries available in the app and should not be interpreted as a claim that every in-game collectible is cataloged.

## Project and community

- [GTA VI Companion source repository](https://github.com/RogueMaster/GTA_VI_Companion)
- [Patreon](https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release)
