<p>
  <h1 align="center"><b>Hello, I'm <a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a> 👋</b></h1>
</p>
<h1 align="center"><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Patreon' title='Patreon'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='Resources' title='Resources'></a></h1><br>

# GTA VI Companion

**An offline Grand Theft Auto VI Story Mode guide, location reference, and collection tracker for Flipper Zero.**

GTA VI Companion puts collectible checklists, vehicle locations, stunt challenges, optional missions, hidden weapons, bottle messages, property information, cheat inputs, safe and door combinations, and an illustrated loading-screen gallery in a handheld app. It works as a **reference and progress tracker**: it does not connect to the game, enter cheats on your behalf, unlock content, or change an in-game save.

Open **GTA VI Companion** from the Flipper Zero **Tools** menu. The guide data and images are packaged with the app; checklist progress and loader preferences are saved to the Flipper's SD card.

## Features

The home screen has 16 icons across two pages. Icons appear in this order, from left to right: **page 1** - Infinite Money Glitch, PS5 Cheats, Xbox Cheats, Collections, Safe & Door Codes, Secret Missions, Randy's Wish List, Moses Drop-Offs; **page 2** - Wyman's Classic Cars, Standout Vehicles, Garages, Houses / Cribs, Dispatch, Mock Loaders, Mock Loader Settings, Quick guide / warnings.

| Menu | What you'll find |
| --- | --- |
| **Infinite Money Glitch** | A cars-and-cocaine walkthrough using a registered 10-slot van and registered second car, warehouse respawn pickups, an explicitly unverified two-car Dunk drop-off claim, confirmed-balance checks, manual-save fallback, and the reported single-bag overfill method. |
| **PS5 Cheats** | Reported PlayStation controller sequences, effects, unlock prerequisites, and unverified-input labels. |
| **Xbox Cheats** | Reported Xbox inputs and effects, including confidence notes for converted sequences. |
| **Collections** | Individual checklists for collectibles, discoveries, WAiNK scans, special weapons, optional missions, challenges, stunts, and activities. |
| **Safe & Door Codes** | 71 reported safe combinations, door codes, and puzzle passwords, with location clues and source-confidence labels. |
| **Secret Missions** | 36 checkable optional encounters, hidden weapon finds, cash routes, stat rewards, and Easter eggs, grouped by the earliest reported story chapter. Uncertain unlocks appear at the end. |
| **Randy's Wish List** | 22 vehicle requests with reported pickup clues, access conditions, and delivery progress. |
| **Moses Drop-Offs** | Seven reported vehicle delivery jobs and their location clues. |
| **Wyman's Classic Cars** | Four restoration checklists with directions to 15 wreck parts, plus a checklist entry for claiming the final car. |
| **Standout Vehicles** | 299 trackable vehicle entries covering special liveries, cars, motorcycles, aircraft, watercraft, mission rewards, and alternate pickup locations. |
| **Garages** | 16 garage entries with location, capacity, acquisition cost, or unlock requirements where known. |
| **Houses / Cribs** | Five story safehouses with locations, chapter access, and facilities. |
| **Dispatch** | Region-based browsing of 969 indexed pickups and activities, with progress tracking and a next-unchecked shortcut. |
| **Mock Loaders** | Gallery of the enabled image sets. The app ships 30 GTA VI and 15 Dungeons & Dolphins images. |
| **Mock Loader Settings** | Enable or disable each image set in the gallery, startup rotation and NFC module transitions. |
| **Quick guide / warnings** | Selectable topic list: OK opens the chosen guide section; Back returns to the highlighted topic, then to the originating menu. Collection progress is a separate live summary. Includes controls, NFC maps, cheat warnings, missables, collection tips and wildlife/fishing references. |

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

### Installation and memory usage

The app loads its bundled guide data and images as needed to conserve memory. It does not require an internet connection for its built-in checklists or walkthroughs.

Install the Companion FAP with its bundled catalog, collection, image assets, and embedded plugins. The firmware extracts the assets on launch; plugins and gallery images load only when needed.

### NFC source and map links

Press **Right (Emulate)** in a linked collection, vehicle, property, or Dispatch detail to start emulating the best available link immediately. GTA6GPS location pins take priority, followed by GTA6GPS category maps, other interactive maps, map images and guides. When multiple distinct URLs are available, press **OK** on the emulation screen to stop NFC and open the scrollable Links list. Use **Up / Down** to browse and **OK** to emulate the selected URL. A single URL emulates directly without a selector. Each option identifies its provider and reference type or location. Bring an unlocked NFC-enabled phone near the Flipper to open the URL.

References may be individual interactive map pins, location-specific guides, map images, or shared category indexes. A text-fragment URL may highlight a named entry in a shared guide. A venue or search-area pin is not necessarily the exact pickup position. Follow the accompanying location and confidence notes.

Wyman’s restoration projects use the same Links list for individual wreck markers and component guides. The restoration retains its own checkmark. Opening Links from NFC highlights the currently emulated link.

The NFC plugin loads on demand, with a mock loading screen when enabled. The active URL appears on the emulation screen and scrolls if necessary. Press **Back** to stop emulation and return directly to the originating vehicle or collection checklist with the same item highlighted. Back from the Links list also returns to that checklist. Dispatch and the Infinite Money Glitch guide return to their detail screen instead. Hold **Back** during emulation to exit to the firmware.

The app emulates a temporary firmware-generated NTAG215 NDEF URI and blinks the magenta LED during emulation; it does not write a physical tag, download images, require a Flipper internet connection, or change collection progress. The phone needs internet access to load the source page or its map/images. All Collections and vehicle/property checklist details, including Regional Dispatch, offer a source link. If a pickup-specific reference is unavailable, the link opens a shared guide or category index; use the item directions to identify the target.

Checklist progress is saved automatically to the Flipper Zero SD card. See **Saved progress and backups** for file locations.

For **Secret Missions**, press **OK** to read the walkthrough, **Right** to check or uncheck an entry in the list, or choose **Check / Uncheck** with **OK** in its detail view. The menu shows the number completed out of 36, and completion is restored when the app restarts.

### Saved progress and backups

Collection and Dispatch checkmarks are saved in **`/ext/apps_data/gta_vi_cheats/`** using alternating `progress3.a` and `progress3.b` files (8,192 progress bits). If neither new-format slot is valid, the app imports the newest valid save from `progress2.a` / `progress2.b` (4,096 bits) or, if neither is valid, `progress.a` / `progress.b` (2,048 bits). Both older pairs remain untouched; new checkmarks are saved in the new format. Older app builds cannot read new-format checkmarks.

**Secret Missions** use `secrets.a` and `secrets.b`. **Mock Loader Settings** use `mock_loaders.a` and `mock_loaders.b`. Back up the app’s SD-card data directory before replacing firmware or formatting the card.

### Region-aware Dispatch

Dispatch groups **969 location stops** into **Vice City**, **Leonida Keys**, **Grassrivers**, **Port Gellhorn**, **Ambrosia**, **Mount Kalaga**, **Other areas**, and **All regions**.

Choose a region to open its first unchecked stop. Press **OK** to toggle its checkmark, **Left (Next)** to advance to the next unchecked stop, **Right (Emulate)** to open its location reference, or **Back** to choose another region. Checkmarks are shared with the corresponding collection or vehicle entry.

**Other areas** includes places outside the six main regions and locations with uncertain region assignments. Alternate spawns can appear under more than one region. Dispatch covers all seven Moses drop-offs and all 22 Randy requests. The home menu has the complete in-app vehicle lists and Standout catalog.

## Infinite Money Glitch

Press **Right (Map)** anywhere in the walkthrough to emulate the [Apollo Aggregates warehouse location off Emory Avenue](https://gta6gps.com/?p=pin-916938). Press Back to stop emulation and return to the walkthrough. This is a location reference, not a collection checkmark.

The guide covers a **cars-and-cocaine warehouse loop** using vehicle storage at **Apollo Aggregates**, the red warehouse east of Hamlet in South Vice-Dale County, beside the teddy-bear van. This is a reported Story Mode exploit rather than an official cheat.

1. **Register both vehicles before collecting.** Bring a **10-bag-slot van or delivery truck** and, for the experimental two-car swap, a **6-bag-slot car**. **Both must be owned/registered**, including the second car; register a stolen vehicle using the **Register** service at a Pay 'n' Spray or through Moses before storing cocaine. Do not rely on an unregistered stolen car retaining its cargo. Keep an untouched manual save. Chapter 2 is recommended to avoid Chapter 1 story-related money loss.
2. **Collect cocaine and cash.** Empty the teddy-bear van outside, the parked van inside, the office table, and the office safe. Retrieve the cash-filled stash bag from the central open van. Store filled bags in the 10-slot van and the second car. **Do not drive away the interior warehouse van**, which is used as a repeat pickup.
3. **Refresh the warehouse by switching characters.** Keep the collected cocaine stored in your vehicles, switch to the other playable character, then switch back and return to Apollo Aggregates. Recheck the indoor van and office safe for respawned cocaine; refill your vehicles and repeat as needed. If the loot does not reset, the **personal-vehicle manual save/reload** method remains a fallback; always use a separate save slot.
4. **Try the two-car Dunk's trailer swap (unverified).** Back the full 10-slot van up to Dunk's drop-off and unload it, move it away, then reverse the filled 6-slot car into place and unload. A community poster claims that repeated character pickup/drop-off after swapping vehicles lets the storage keep paying, citing **$13,200 per drop-off**. **Actual credited payments are unconfirmed**; reports warn that the counter can change without payment. Do not assume the amount is repeatable.
5. **Cash out and verify.** Check the real bank balance, pending sales and safehouse mailbox. If the two-car claim fails, use the repeatable warehouse pickup and sell full bags through a story-unlocked fence instead. Early-game buyers include Donnie or Dunk; **Tyrone** reportedly offers the strongest narcotics rate starting in Chapter 3. **Brian's Lockup** in Key Lento has a reported keypad code of `7700` and may pay after several in-game days. Fence demand, payout, and availability vary.

**Reliability:** Warehouse respawns may depend on the save, story progress, or game updates. The two-car Dunk payout is an unverified community claim. Check the actual bank balance and keep a separate manual save before attempting the exploit.

### Free registration, bag overfill and Max Focus

All three are in **Quick Guide**. The main Infinite Money Glitch screen also includes the bag-overfill method and retains the warehouse Map shortcut.

- **Free car registration:** Chapter 2 Moses tutorial grants one free registration/respray for an eligible tracker-free car with available storage. Use Find Another to preserve the waiver until the desired car is selected. The reported early Thrax phone-clipping route is a separate guide. No reproducible method for unlimited free registrations was established. Standard trackers become eligible at Pay n Spray early in Chapter 3; [PowerPyx confirms](https://www.powerpyx.com/gta-6-how-to-register-advanced-tracker-cars/) that Advanced tracker registration follows Randy's Wish List and Dale Mamba. [Moses registration guide](https://allthings.how/gta-6-how-to-register-a-car/) and [Chapter 2 Thrax walkthrough](https://finalboss.io/gta-6-truffade-thrax-starfish-island).
- **Overfill stash bags:** The November 24 B Man Xbox video, summarized by [ViceVaulted](https://www.vicevaulted.com/d/gta6-stash-bag-overfill-infinite-stash-glitch), reports using a full bag plus a nearly empty bag, collecting only from paired prompts, and resetting loot by character switching. One bag reportedly exceeded $100,000. Test actual fence payouts and BuckMe capacity. PS5 replication is unverified. A [smaller overfill report](https://www.reddit.com/r/GTA6_playbook/comments/1y4lei5/duffel_bag_trick_to_increase_from_9600_to_10800/) describes collecting two adjacent pickups quickly. The [stuck trunk counter](https://www.vicevaulted.com/d/gta6-trunk-stash-counter-stuck-glitch) is a separate experimental method with inconsistent payouts.
- **Max Focus glitch:** Repeat Market Range Beginner at Rodgers Ranch, fully quitting the result screen before starting again; check the Focus stat of each character. [PowerPyx stat guide](https://www.powerpyx.com/gta-6-how-to-increase-character-stats-health-cardio-focus/) corroborates the basic method; [ViceVaulted](https://www.vicevaulted.com/d/gta6-focus-shooting-range-quit-farm) reports the creator's estimate of 5% per exit. This is stat training, not the Unlimited Focus cheat.

## Secret Missions and Easter eggs

**Secret Missions** is a guide to 36 optional activities, hidden weapon pickups, cash opportunities, and Easter eggs. Entries are organized by the **earliest reported chapter or prerequisite**: Chapter 1 (with missable opportunities first), later Chapter 1 unlocks, Chapter 2 and its follow-ups, then locations with **unknown story requirements**. Within a chapter, exploration activities can usually be attempted in different orders. Check off activities as you finish them; you can uncheck any entry later. This is a discovery guide, **not** a required mission sequence. The menu uses `C1`, `C1+`, `C2`, `C2+`, and `C3` for reported story windows; `?` means the chapter requirement has not been established.

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
| Any chapter; Jason or Lucia (reported) | **Hyde Island Manhunt** | Visit Hyde Island or trigger the Vice City couch-loading encounter; survive and collect gear at boathouse | **Island Poacher FF-50** and reported $30,000 total; no confirmed suppressor |
| Port Gellhorn access, chapter uncertain | **Satan / Circles of Hell** | Open garage in northern Port Gellhorn from midnight to 4 AM, choose *Rock On* | Hellspawn Comshot 12, tattoo and shirt; **no verified health increase** |
| Vice City access, chapter uncertain | **Secret radio stations** | WAiNK poster scans at The Vault and four La Perle/Crosstown shops | Hidden stations (reported) |
| Vice City access, tower uncertain | **Literal Easter egg** | Nest atop a high broadcast tower | Short secret scene; no verified material reward |
| Any chapter; Jason or Lucia (reported) | **The Backrooms** | Orange container at a dig site on east bank of Lake Tallaloko, north of Millport. | Permanent max stamina increase for the active character; Mind Maze Left Leg Award tattoo unlocked for purchase. |
| Lucia, Chapter 2; potentially missable | **Arthur Morgan Hat** | Southern Port Gellhorn yard sale. | Genuine Cowboy Hat in wardrobe. |
| Chapter 2 registration glitch (reported); Chapter 3 No Flusher route | **Andrew Node's Thrax** | Ocean Beach headquarters; southern Starfish Island, Sabal Circle. | Tracker-free Thrax; $70,000 registration or reported free tutorial registration. |
| Return after the Chapter 1 fire; police zone clears around Chapter 3 | **Dolores the Snake** | Burned house, Key Lento. | Hidden encounter; no new reward reported. |
| Jason; chapter unknown | **Backyard Wrestling** | Backyard American Mania ring behind a trailer in Hamlet. | Luchador Mask, Adrenaline and other stat items; card identity unconfirmed. |
| Jason; after access to all substations | **Vice City Portal** | Badger R&D tower, Key Lento; Reactor 2, Turtle Head Power Station east of Hamlet. | Tommy Vercetti shirt and Malibu Zombie Bobber (see Standout Vehicles). |
| Boat and diving access; chapter unknown | **Burg Industries Mystery** | Yellow platform east of Key Lento; second station west of Mile Zero Air Station. | Reported meteorite pickups; five named facilities are in Underwater Discoveries. |
| Free roam; chapter unknown | **Ride Em Chihuahua** | Frontier Adventure Golf, south Waning Sands. | Animal log entry. |
| Diving access | **Kraken Shipwreck** | Seabed southeast of Gloriana Key. | Easter egg; no confirmed collectible counter or reward. |
| Free roam; chapter unknown | **Immortal Man** | Southern Key Lento, residential decks near Donnie's trailer. | No verified reward. |
| Free roam; chapter unknown | **Deep Roots Memorial** | Deep Roots Memorial Sanctuary northeast of Hamlet, between town and airport. | Developer memorial; no reward. |
| After police zone clears, around mid Chapter 3 | **Key Lento Rainy-Day Fund** | Jason's burned house, Key Lento. | Reported $5,000 regardless of earlier stash amount. |
| After Clonin Keys, Makin GS; either protagonist | **Capital Offense** | Glass-fronted showroom in south Vice City; blue Banshee on display. | Capital Offense trophy. |
| Lucia alone after Credit Check | **Bad Luck, I Guess** | Payday Loans store in Little Cuba from the opening mission. | Bad Luck, I Guess trophy. |
| Night; reported around 1:30 AM | **Lighthouse Ghost Ship** | St. Lucia Lighthouse, Little Hardin Island. | Cursed Knife (Special Weapons); possible ghost scene. |
| Boat access; chapter unknown | **SOS Island Phone** | Tiny island south of Bobcat Key, north of military air base, Leonida Bay. | Hidden video and two meteorites; no separate reward for the mystery. |
| IAA Black Site access; exact story gate unconfirmed | **Brainwashed** | North Mariana County, second President bunker. Hatch 085355. | Permanent Focus increase, Brainwashed Right Arm tattoo and Brainwasher 1922 pistol. |
| Free roam; story gate unknown | **Plumber Hats** | Ruins in southwest Moreau, Lummox County; reported near old paper mill. | Red J and green L hats for Jason and Lucia. |
| Free roam; story requirement unconfirmed | **Marbelle Hotel Fog** | NW 8th Street, Crosstown; rear entrance, roof note and ground-floor safe `955214`. | Crowex Ambassador Breakfast watch and reported $234. |
| Free roam; story gate unknown | **Haymarket Clown** | Haymarket building, Ocean Beach. | Easter egg only; no reward reported. |

The in-app entries contain the detailed steps and specific location clues. For Kyle's chemical delivery, see [the FarmFix negotiation and stamina reward](https://getvicekeys.com/guides/gta-6-collect-chemicals-for-kyle). For the Hamlet cash route, the total combines several items and fences; it is **not** a guaranteed $20,000 cash pickup. The separate **Infinite Money Glitch** menu covers the disputed respawn/reset method. Some activities have a spoiler-heavy outcome, and certain Chapter 1 opportunities cannot be revisited after the story advances.

Use **Collections → Easter Eggs** for mapped secret references and **Collections → Vice City Portal Route** for the ordered substation maps. Supported Secret Missions include a **Maps** shortcut to the related location detail. Back from NFC restores the map checklist; Back again restores the original Secret Missions list and focus.

**Source confidence:** These are third-party reports, not gameplay-tested by the companion. Names, spawns, unlock chapters, puzzle combinations and rewards can vary or differ between guides. Unknown requirements are explicitly labeled rather than inferred from their location.

### Optional Missions checklist

**Strangers & Freaks trophy:** The eight qualifying questlines are Merle, Freddie, Kirk, Kyle, Alvan, Yago, Papa Audacity and Pilot Pete. Finish each finale for trophy credit. Yasmani’s ATM tip expires after one in-game day; the Gold Blammer and small-bank tips may be missed by advancing to the Chapter 4 vault tutorial.

**Randy roster audit:** All 22 tracked names match the [GTA Town roster](https://gtatown.com/garage/randy-wish-list); the [November 27 core-board guide](https://www.gta6walk.com/side-missions/randys-wish-list/) distinguishes 15 board cars plus Zorrusso from later missions and crossed-out candidates. The 22 records are not a mandatory 22-car quota. [Delivery Day](https://www.powerpyx.com/gta-6-delivery-day-meet-randy-walkthrough/) awards Supply & Demand, followed by [Collect the Tempesta](https://www.powerpyx.com/gta-6-collect-the-tempesta-walkthrough/).

The **Collections → Optional Missions** checklist tracks **43 mission and side-activity entries**, including Freddie, Dodos Rising, Papa Audacity, Billy, Andy, Yasmani, Tyrone, Dunk, Cal, Eddie, Shayna, Yago, and the bank-vault tutorial. Entries describe reported chapter requirements, missable objectives, and rewards. This checklist is distinct from the Secret Missions home-menu guide.

| Mission strand | Reported access and location | Important notes |
| --- | --- | --- |
| **Retest Freddie's Defenses** | Freddie's compound north of Lake Leonida, Leonard County. Either character. | Reach Freddie's intercom on the first visit, then wait for his text to start the retest. Reported Strangers & Freaks credit and hurricane unlock; PowerPyx lists $10,000, while VGC reports $6,250 for leaving or the gold Dubsta for staying. [Walkthrough](https://www.powerpyx.com/gta-6-retest-freddies-defenses-walkthrough/) |
| **Dodos Rising** | Meet Noah near the airport or Rosebud outside the Catalan Key Oceanarium. | Complete *Bring Out the Chimp* and *Sisters of the Sea*, then use Lucia for *Twin Flames* in Watson Bay and *Dodo, Rise!* at Emerald Springs. [Mission index](https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/) |
| **Papa Audacity** | Chapter 3. Start *The Unknown Stuntman* at the park on the south end of Ocean Beach. | Six checklist entries cover the stunt tests, reported AudaCity Sandking reward, and the launch party in Tequesta. [First mission](https://www.powerpyx.com/gta-6-the-unknown-stuntman-walkthrough/) |
| **Get Shirley's Rum** | Chapter 1, Key Lento; buy Salty Squid rum at Three Sheets Liquor for Shirley. | **Missable before Tourist Trap.** Buy the rum before investigating strange bird noises: the reported shootout can close the shop. |
| **Ring the Bell!** | Chapter 1; Cal's invitation to the Rusty Anchor after *Day Release*. | Go for drinks and drive Cal home. The Rusty Anchor shirt and cap become **purchasable**, rather than free mission rewards. |
| **Steal the Scooter** | Chapter 1; Shirley's follow-up call, Peggy's driveway across the water in Key Lento. | Bring the scooter back to Shirley; reported unavailable after *Tourist Trap*. |
| **Pick Up a Gun** | Chapter 2; Lucia at Gellhorn Outfitters in Port Gellhorn after *Girls' Trip*. | Retrieve locker weapons or purchase a gun; no separate cash reward is reported. |
| **Kyle / McKittrick Farm** | Lucia; flyer near Starlet Motel, then McKittrick Farm; Chapter 2 start and Chapter 3 follow-ups. | Multi-stage crop-duster, chemical-recovery, and standoff strand; rewards vary by choices. At FarmFix, negotiate a reduced price or offer Kyle's plane as collateral; eat the offered strawberry for the reported max stamina increase. |
| **Bret Vogel Hunting** | Jason alone, northern Lummox County after *Coffee Break*. | Hunting follow-ups; the **Killtelligence SM-69** is reported as a weapon reward and has its own acquisition checkmark. |
| **Yago Limited Race** | Jason; Salton, Vice City, after *Bumper to Bumper*. | Watch for the **limited-time hourglass marker**; the reported race payout is not guaranteed. |
| **Pilot Pete's Flights** | Jason; Sundown Private Airfield in Chapter 4 after *Pool Day*. | Start *Passing the Torch*; **Pete's KZ-59** lies on the hangar backroom **table**, while safe `3718` is separate loot. Later flying jobs follow. |

These are **guide-reported story windows and rewards**. Check the in-game phone, map, and mission state for availability; do not assume an optional activity is still accessible after its chapter cutoff. Sources: [PowerPyx optional-missions list](https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/) and [Nexzy optional-missions guide](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/gta-6-optional-missions).

## Safe & Door Codes

An offline catalog of **71 published codes and passwords**. Choose **All codes**, **Mission safes**, **Operations**, **Doors / garages**, **Puzzles / other**, **Browse by area**, or **Random safe tips**. The area browser groups codes into Vice-Dale / Balsero, Keys / islands, Port Gellhorn, Swamps / Grassrivers, Mariana County, Inland / north, and Other areas. Each entry shows the published combination, area, landmark or access instructions, story context when known, and a confidence label. Codes appear at the start of list rows so they are visible on the Flipper's small display.

**Weapon access:** Black Eagle's Balsero warehouse office uses `789987`, and the safe inside uses `031462` to obtain the Kalinga .50. The Pelican Valley research outpost uses password `CLEAVES`, safe `0072`, and boathouse keypad `0212` for the Crabshack VF5. Published bunker clues connect the island door (`010317`) and safe (`2975`) to the IAA Black Site hatch (`085355`), reportedly holding the Cold War 556. The Marbelle Hotel safe (`955214`) is reported to contain a collectible watch. Royal Bay Marina uses a variable code: the last four digits of the phone number displayed on the warehouse sign unlock the office safe for Leroy Rook (Bronze). See [KeenGamer - card pickups](https://www.keengamer.com/articles/guides/all-collectible-cards-in-gta-6-locations-sets-prices-and-rewards/). These combinations are also listed in the relevant weapon details. The Black Eagle M306 TMR has a separate reported access route through the stacked boxes and fence.

| Category | Entries | Examples |
| --- | ---: | --- |
| **Mission safes** | **8** | Delights Cabaret `2008`; Donovan Banks & Co. `0104`. |
| **Illegal Operations** | **17** | Nova Auto Junkyard `7777`; Kingpin Bowling `0300` (conflicting report). |
| **Doors / garages** | **18** | Brian's Lockup `7700`; Key Lento Party House `092319`; Mount Kalaga Tower `69185` (conflicting report). |
| **Puzzles / other** | **28** | Pelican Valley research codes and two text passwords. |
| **Total** | **71** | Reported PINs, text passwords and variable robbery examples. |

At **Father Scrumpy's Red Apple Orchard**, enter `360782` at the farmhouse garage to claim the **Applesauce Bobcat**. At **Southside Storage**, enter `1010` after photographing all five **1010 Murals** to claim the **1010 Cheetah**. Both vehicles have entries in Standout Vehicles.

At **Kingpin Bowling** in Port Gellhorn, the **Gutterball Moreland 850 shotgun is on the northeast pool table**. The **northeast office safe** (reported `0300`, with `0330` in a conflicting guide) holds cash and the southeast room holds the **Crowex Octane 'Orca'** luxury watch. At the **Cottonmouth Lake trailer** on the southwest spit, collectible **Laki Maivia (Bronze)** is in the **safe beneath the kitchen microwave**, opened with `44017`. The highlighted bedroom-wall photos/postcards give the clue. The card and cash are collected together from the microwave safe. [Kingpin Bowling watch](https://gta6.house/library/items/crowex-octane-orca-luxury-watch)

**Vanessa's pink Pfister Growler:** Before **Chapter 3 ends**, inspect the memorial photo (**Muffins**), vanity sketch (**Queen**), and number **15** on Boobie Ike's laptop, enter `MuffinsQueen15`, and **read the WhatsUp message**. Then find the pink-bow garage on **Del Rio Blvd, La Perle**, through the run-down motel lot past Aline's Thrift Store near Auntie's Place; enter `92665` to claim the car. The garage keypad may not activate before reading the message. Access may be lost when Chapter 4 begins; the vehicle has its own existing Standout checklist entry, and the free Del Rio Boulevard Garage is separately tracked (5217). These are published walkthrough claims, not guaranteed access conditions.

**Leading zeros matter.** Published sources disagree on Kingpin Bowling (`0300`/`0330`), Saber Shipping Facility (`1122` remains disputed), and Mount Kalaga Tower (`69185`/`691865`). A newer guide attributes `1965` to Casa Guardia, not Saber Shipping Facility; `8046` is the Yard code. South Point Airfield `2341` is also disputed. Certain robbery safes may randomize their combinations; Avery Pawn has reported codes `6499`, `4768` and `4248`. Search the back office, whiteboard, computer or guards; press both sticks (`L3 + R3`) for Focus Mode, use the “Safe Code?” prompt when available, or check Notes on the keypad. The Cigar Import Front door at Palmis Liquor uses `1426`; the cash safe inside is separate. The alien rescue lab code `2314` is a reported example: count the groups of lines in your dream to obtain the applicable code.

**Confidence labels:** *Guide-tested* means the linked guide publisher reports testing it, not that the companion author tested it; *Multiple reports* means more than one published source; *Reported* means limited corroboration; *Variable* means the result may differ by session or replay; and *Conflicting* means sources disagree. A `?` beside a code signals uncertainty.

## Special Weapons

Under **Collections → Special Weapons**, browse **137 individual guide-reported weapon pickups and variants** covering rifles, pistols, SMGs, shotguns, launchers, a knife, and a speargun lead. Each row has a name, location or access clue, and acquisition notes. Several entries share a location but have independent checkmarks. Locations without adequate corroboration are labeled **reported** or **unconfirmed**, rather than shown as established spawns.

**Mark acquired:** select a weapon and press **Right** to check or uncheck it. Press **OK** for details and choose **Check/Undo**. The status reads **Acquired**, the list shows progress, and checkmarks are saved to the app's SD-card progress files. Acquiring one of a paired set does not mark the other.

Example searches include the **Assassin Nipper .38** on a Permit Keys wreck, the **Unpacked Comshot 12** inside a Key Lento house, the **Drug Mule KZ-59** near a sunken Riquezas Keys speedboat, and the **Deever Tech GLR40** plus **Cockroach Xtreme 556** at the Sunrise Motel in Tequesta. These are reported community discoveries, not guaranteed spawns.

The catalog includes **Kingfishers Akulus** (Sunrise RV Park), **Chromium W-10** (La Perle Trap House), **Zodiac Nipper .38** (Ocean Beach Lab), **Quarter Mile ES9** (Chatfield Storage on Waxman Island), **Killtelligence SM-69** (Bret Vogel's hunting strand), **Shark Knife** (a shark cage north of Saint Attaway Islands), **The Patriarch DRX** (submerged vehicle southwest of Kalaga Reservoir), **Deadringer 1992** and **Money Bag 1992** (Moreau River mobile-home area), **Cargo H&L .38** (yellow Scoop container at Port Vice City), **Couch Potato Mustang .357** (suspended couch off Tanglewood Lane), and **Pete's KZ-59 Assault Rifle** (Sundown Private Airfield hangar). These named variants are **guide-reported leads**. Some have broad search areas, conflicting variant names, or limited independent verification; not every entry is a confirmed free pickup.

**Pickup directions:** The **Couch Potato Mustang .357** is hidden in a hanging couch by a mobile home off Tanglewood Ln in Port Gellhorn. Shoot the supporting ropes, then sit on the fallen couch to claim the revolver. The *Bombazo!* publication also has a copy at the Formosa Ave apartments in Hamlet, inside the second ground-floor unit from the right (on the toilet).

**Keep rewards and codes distinct:** The Clunker 556 is in the smaller Brooks Boatworks building near the code map; safe `6823` is in the warehouse pool-table room. The Scrapper VF5C is beside the La Cascara Motel safe room, not inside safe `6969`. The San4San KZ-59 is by the TV in JanJak's adjoining office, and the Marabunta Grande KZ-59 is near the blue container at Nova Auto Junkyard. The Eris Fresh DSG is reported as an Ultimate Edition operation reward.  The **Gutterball Moreland 850** is on the northeast **pool table**, not in the Kingpin Bowling office safe. **Pete's KZ-59** is on the hangar backroom **table**, not in the `3718` safe. **Killtelligence SM-69** is described as a hunting-quest reward, not an Ammu-Nation purchase. Named shop-exclusive variants should not be mistaken for world pickups.

A weapon's detail includes any required combination when known. For example, the Kalinga .50 gives both the office and safe codes; the Crabshack VF5 gives the dock code and optional puzzle chain; and Russo's Pistol and the Cold War 556 list their bunker combinations. Individual acquisition checkmarks are independent even when weapons share a building or puzzle.

Unique weapons use yellow icons, are shared across both protagonists after acquisition, and have fixed configurations. Store-specific variants list the reported purchase price. Donnie's H&L .38 may be permanently missable in Don Gone. The Dazzler DQ-9 is also listed as DW-9; the Jack Sheepe H&L .38 edition requirement is disputed.

**Griffin Shooting Range:** Earn gold in every Griffin challenge to unlock the suppressed Pocketblast DSG for a reported $2,840. The Quarter Mile ES9 is in the Chatfield office desk case; the yard safe holds reported $9,000-$10,000 (code `5800`). The Purple Drank Emerus is in the second open waterfront container; open the exit with the security-cabin button. Reported registration $29,900; scan value $32,945-$33,000, with a separate $30,000 Randy offer reported.

**Additional weapon pickups:** Gun Nut SMS-01 (Key Lento), Gil's Kalinga .50 (Safe Word), Monty's 1992 (The Missing Link), Caminos Signature K17 (Ultimate Edition), and Mahi Nipper .38 (conflicting store/mine reports). Save before the two Chapter 3 missions to avoid missing their pickups.

**Additional reported variants:** Marabunta Grande Hobo (Hamlet Storage), Shack Attack W-14 (Richardson Hunting Supplies), Donnie's Knife (Big Pine farm) and Candy Suxxx Bat (Peacock Estates mansion). Russo's pistol is reported as the 1922; the alternate 1992 name remains noted.

**Weapon sublists:** World Finds & Leads, Operation Weapons, and Shops & Rewards each have their own checklist. The catalog includes reported variants and ordinary weapon leads; its 137 entries are not the game's reported 130 unique-variant counter. In the weapon locker, select the base gun and open Variants to reclaim acquired unique weapons. Personal Ultimate Edition weapons may be character-specific. The Convict Kalinga .50 is reported in a collapsed prison escape tunnel; the Mister Mangrove Laird 350 is awarded during Kirk's final hunt. Other reported weapons have uncertain access requirements, as indicated in their entries.

Named variant map references include GTA6GPS and alternate GTA6 Maps pins. Shared venues and conflicting retailer or acquisition reports remain labeled; a map reference alone does not establish a unique pickup or mission unlock.

**Weapon variants:** The checklist includes unique variants and ordinary weapon pickups. Some variant names, store restrictions and acquisition routes remain uncertain. The published weapon roster lists 130 variants, but only 94 are named in that guide. Verify the weapon name in your inventory before checking an entry.

## Endings

**Major story spoilers.** Under **Collections → Endings**, browse **13 finale routes and epilogue variations**, including **ten reported outcomes or scene variations** and **three speculative variants that have not been demonstrated reliably**. Each entry explains the relevant chapter, choices, preparation, and reported result. The final three entries are marked with `?` in their names so they cannot be mistaken for confirmed unlocks.

**Track endings seen:** Press **Right** on an ending to mark it **Seen** or uncheck it; press **OK** to open its requirements and use **Check / Undo**. The completed count and checkmarks are saved to the regular Collections progress files on the SD card. This tracker does not change the game or its save data.

Before the finale, make a **manual save**. Published guides identify four major influences: Jason and Lucia's relationship, each character's Criminal Profile, money deposited in **BuckMe Family Savings**, and the choices in **Protective Custody** and **The End of the Road**. Sources disagree on exact thresholds for some branches. The commonly cited best-outcome requirements are a **Hot Together** relationship, relatively clean profiles, **$2,000,000 in Family Savings**, and **Swim for It** at the bridge.

| Ending / variant | Reported way to see it |
| --- | --- |
| **The Swamp** | Wait for **Take Deal** at the officer's standoff in *Protective Custody*, then accept it. |
| **The Sacrifice** | Weak relationship; shoot Mendez, then choose **Stay Down** at the tower. |
| **Defend Jason** | Weak relationship; shoot Mendez, then choose **Defend Jason** at the tower. |
| **No Surrender** | Shoot the officer; at the bridge select **No Surrender**. |
| **No Surrender + Two Years Later** | Same bridge choice, with $2,000,000 in Family Savings; reported family epilogue. |
| **Armed Swim for It** | Violent Criminal Profile; choose **Swim for It** and let **Exit** expire. |
| **Armed Swim + Two Years Later** | Armed Swim route with full savings; a different post-credit family scene is reported. |
| **Swim for It - Empty Handed (Prison)** | Reach the bridge with **Hot Together**, clean profiles and less than $2,000,000 saved; choose **Swim for It** and press **Exit**. |
| **Happily Ever After** | Hot Together, both Professional, $2,000,000 Family Savings; choose **Swim for It** and press **Exit**. |
| **Jason Escapes with Rocco** | Break up at the opening talk in Protective Custody; both Professional, at least $500,000 saved; accept Take Deal. |
| **The Swamp + Savings (?)** | *Speculative:* Take Deal with full Family Savings; an additional ending scene is unverified. |
| **The Sacrifice + Savings (?)** | *Speculative:* Stay Down on the weak-relationship shoot route with full savings; not reliably documented. |
| **Defend Jason + Savings (?)** | *Speculative:* Defend Jason on the weak-relationship shoot route with full savings; not reliably documented. |

The **13 tracked entries** include epilogue variations and three unverified legacy predictions. Current GTABase coverage identifies eight principal endings, including the separately tracked Rocco branch. Only the two fatal bridge routes have separately documented savings epilogues; the three predicted savings variants remain unverified. Newer guides also report **Jason Escapes with Rocco**: break up at the start of Protective Custody, keep both profiles Professional, save at least $500,000, then accept Take Deal. Lucia surrenders, Jason escapes, and their later lives are shown separately. This route now has its own progress ID; the swamp route retains its original ID. Existing IDs are included as separate checklist entries and search leads. Save before entering Abe’s office; Postpone leaves time for preparation. After the credits, free roam reportedly resumes before the finale. Keep a separate manual save.

## Collections and exploration

Select **Collections** to browse categories and individual sets. Each entry provides the available location, landmark, chapter, or activity clue and a checkmark for tracking completion.

| Collection or activity | Content |
| --- | --- |
| **Collectible Cards** | **480 cards across 18 sets**, organized by card set with pickup clues. Random Card Pickups tracks 52 reported pickup locations; these are not additional card identities. All 36 markers in the [GTA6GPS yellow loot box directory](https://gta6gps.com/map/game/loot-boxes) are accessible across Random Card Pickups and Location Search Leads (Marbles). Contents vary, and the Hamlet marker may overlap another reported pickup. John Morgan (Dusty Pink) is reported on a floor-level poster at the back of the cream villa's garage on Casanova Island. All 480 named entries have individual map or targeted pickup guide references; some reported pickups have approximate or conflicting locations. Blake Balls (Bronze) and Anthony Herndon (Bronze) include targeted illustrated pickup guides where individual map links could not be reconciled. Individual GTA6GPS links are available where the card identity and location can be reconciled; variant-uncertain matches are labeled in the entry notes. All 90 entries across the three dancer sets (36 Jack of Hearts, 36 NINE1NINE and 18 Delights) include individual timestamped pickup walkthroughs in the NFC selector. The [Jack of Hearts walkthrough](https://www.youtube.com/watch?v=LTbw8B0DQHA) and [location guide](https://gta6vcity.com/en/guides/gta6-jack-of-hearts-cards/) supplement the individual maps. Additional screenshot-derived [GTA6Direct card maps](https://gta6direct.com/shared/collectibles/collectible-cards) and [Cavaliers Gaming pickup walkthroughs](https://www.cavaliersgaming.com/games/gta-6/collectible-cards/) supplement the named entries. Individual non-dancer pickup videos are also available; source color discrepancies are noted. The Dominant card includes an approximate community map and landmark guide. Wokester (Blue) and Side Effect (Light Blue) have named marker references; verify variant labels in-game. |
| **WAiNK** | **31 landmarks**, **35 curiosities**, **23 sculptures**, and **all 15 No Flusher tags**, with identifying landmarks and scan instructions where available. |
| **Peewees Billboards** | **4 roadside billboards** with approach directions, the correct face to scan, and instructions for claiming the Belville clothing reward. |
| **AHOL Street Art** | **112 art scans** organized by art type, including an option to mark a whole type complete. Each piece has an individual named guide reference and a numbered map or photographic location reference. Landmark maps are labeled separately. The [GTA6GPS street art directory](https://gta6gps.com/map/game/street-art) also provides a numbered Cone 246 map and probable alternate maps for Mattress 72 and Buckets 274. |
| **1010 Murals** | **5 photo locations** and framing clues. The reported completion reward is the 1010 Cheetah at Southside Storage, code `1010`. |
| **Kenton’s Treasures** | **7 named treasure sets**, with relic lists, dive-site directions, and individual search clues for the five named wreck sets. |
| **Underwater Discoveries** | **33 named discoveries**: six existing wrecks, five numbered Burg facilities, the Underwater Hotel and other seabed sites. Includes location-specific map references for all 33 discoveries, numbered guide maps, and separate legacy lab leads. Screenshot-derived positions are labeled as reported, not independently verified. |
| **Magazines, Books & Comics** | **27 reading entries**, including *Law & Litigation in Leonida*. That Hamlet copy has a published location and an unconfirmed contribution to the game's 27-title counter. |
| **Book Swaps** | **7 Little Library locations**. The eighth marker in one community directory is a second listing of Killer Prom in Key Lento, not an established eighth library. |
| **Meditations** | **18 character-specific completions** across **10 places**, with separate Jason and Lucia entries when applicable. |
| **Cocaine Flamingos** | **103 reported pickups and search leads**, grouped by geographic area with landmarks, access clues, and available map references. |
| **Unique Playlists** | **2 playlists** and **5 related poster locations**. |
| **Discovered Styles** | **51 entries**: 6 Jason magazine styles, 10 Lucia magazine styles, 17 reported magazine tattoos, and 18 luxury-watch pickups and leads. The Wandering Cowboy and Wandering Cowgirl share a reported Ambrosia church magazine; verify both unlocks. The [GTABase magazine directory](https://www.gtabase.com/gta-6/map/?feature=magazine) and [Tildrix magazine directory](https://maps.tildrix.dev/en/gta-6/magazines-books-and-comics) report the shared church location. The Hamlet buzz-cut legacy alias is retained under Location Search Leads; distinct watch credit is not established for every reported pickup. |
| **Optional Missions** | **43 mission and side-activity entries**, with character, earliest reported chapter, missability, and reward clues. Distinct from the Secret Missions home-menu guide. |
| **Messages in Bottles** | **6 numbered shoreline pickups** around Key Lento and the Shoeless Keys. These have their own checklist and do not count toward publications or Book Swaps. |
| **Missing Persons investigation** | **5 checkable investigation stages**: Starlet Motel poster and warehouse, three missing-tourist reports, and the optional warehouse/mine finale. The guide has **three victims, not five**. |
| **Meteorite Chunks** | **83 reported locations** across Keys, Coast & Islands and Mainland & Caves, with terrain, access, and nearby-landmark clues. |
| **Stunt Jumps** | **130 launch points** grouped by county, with reported launch directions and stunt goals. |
| **Under the Bridge** | **202 bridge crossings**, with individual spans and nearby landmarks. |
| **Knife Flights** | **40 aircraft gaps**. |
| **Base Jumps** | **25 jump starts** with published par-time notes where available. |
| **Location Search Leads** | **253 checkable leads** for map pins that may match an existing pickup or still need their unlock name or counter credit verified. Includes legacy lab leads, vehicle sightings, unmatched street art and lab markers, reported luxury-watch sites, six hunting-region references, optional roadside encounters and provisional flamingo map markers. Search leads include seven new provisional GPS markers (five style/watch, one flamingo, one underwater safe); these do not establish unique collectible credit. Search leads are not additional official collectible identities. |
| **Easter Eggs** | 24 reported optional discoveries and secret-route references, separate from Secret Missions completion. |
| **Vice City Portal Route** | 14 mapped steps: radio tower, 12 substations and Reactor 2. |
| **Endings** | **13 finale outcomes and proposed variations**: ten reported outcomes or epilogues and three speculative savings-based variants, each with story choices, suggested prerequisites, and confidence notes. |

**Peewees Chicken Challenge:** Find Mr Enigma’s sponsored Snapmatic post, scan the west faces in Vice-Dale and Mariana and the north faces in Leonard and Lummox. After all four scans, use the Chicken Challenge messages to solve BELVILLE, then visit the restaurant in Belville and select Collect Prize with the manager. Both characters receive the cap and shirt; wearing the set reportedly grants free Peewees meals. See the [billboard walkthrough](https://gta-walkthrough.com/gta-6-peewees-billboards/).

**Collectible card maps:** Named cards and color variants include individual or alternate NFC references from the [GTA6 Maps card directory](https://gta6-maps.net/categories/collectible-cards) and [GTA6GPS card directory](https://gta6gps.com/map/game/collectible-cards). These cover sports, club, dancer and comic card sets. Compare the reported pickup and card color in-game; unspecified colors, the Tusi/Junior Taufua naming discrepancy and conflicting locations remain provisional. Alternate sightings do not create separate collection credit. The [John Morgan marker](https://gta6gps.com/?p=pin-918402) does not identify a color.

**Luxury watches and styles:** [Gene’s encounter](https://gta6.house/library/items/novak-lauret-haute-grandeur-luxury-watch) reportedly awards Novak Lauret Haute Grandeur when the player covers for him; consult the [choice guide](https://gta6-help.com/en/gta-6/random-events/gene-naked-man-ocean-beach/). Unidentified watch locations near El Gabeto, Emilio Auto Center, Melody Theater and Fire Rescue Station 35 are search leads, not distinct watch models. The Rockridge and VR Gamer entries include alternate references. The [VR Gamer access guide](https://games.gg/grand-theft-auto-vi/guides/luxury-watch-locations-grand-theft-auto-6-known-finds/) reports Lucia-only access and a safe near the bathroom. The [GTA6GPS style directory](https://gta6gps.com/map/game/style-unlocks), [GTA6 Maps watch directory](https://gta6-maps.net/categories/luxury-watches) and [GTA Base watch map](https://www.gtabase.com/gta-6/map/?feature=luxury-watch) provide candidate locations. The [Kelly County airfield tarot magazine](https://gta6gps.com/?p=pin-918150) and [Hamlet bedroom hairstyle](https://gta6gps.com/?p=pin-918785) do not identify the unlocked designs.

**Street art and hunting:** Numbered artwork and landmark clues are supplemented by [Tildrix artwork maps](https://gtamap.tildrix.dev/en/gta-6/ahol-street-art) and [GTA6GPS street art](https://gta6gps.com/map/game/street-art). Tildrix uses Open GTA 6 Map (fbritoferreira) and GTADB contributors (CC BY 4.0). Named sites include Pawn Stash, Gabriel Wexler, The Shores Poolside Bar and Prestige Firearms; the last is a possible TV 240 match. [GTA Guide hunting](https://gta.guide/gta-6/guides/how-to-hunt) provides regional hunting and delivery maps, not guaranteed animal spawns. Later Target Deck species requirements remain unverified.

**Meteorite Chunks:** Search impact craters, coastal rocks, islands, caves, and underwater tunnels. The Selkie Hole tunnel route includes five reported fragments and requires scuba gear. The catalog distinguishes individual pickup clues from broader search areas. Each Selkie Hole chunk provides the five reported cave maps as shared references; marker order is not the numbered collection order. Individual [GTADB meteorite search markers](https://gtadb.net/articles/gta-6-meteorites-locations-guide/) provide approximate positions for island, coastal and mainland pickups; potential alternate matches are labeled separately.

**Unique Race Wins:** Under **Collections → Activities / Robberies → Unique Race Wins**, 40 named events have independent completion IDs, reported payouts, lap counts, field sizes, rating and entry details where available, and NFC source links. Individual course markers are used where matched; otherwise the link is labeled as a shared venue or nearby marker in the notes. The fifth Keep It PG race is the three-lap Rallycross Loop; the introduction is not counted as a separate win. See the [Rallycross Loop guide](https://getvicekeys.com/guides/gta-6-gellhorn-rallycross-loop-dirt-route). Verify first place in Progress > Activities > Racing before checking. Sources: [race guide](https://cutscenewire.com/games/gta-6/side-content/gta-6-all-40-unique-race-locations-and-how-to-unlock-them/), [race terms](https://thegameswiki.com/gta6/wiki/racing), [GTA6GPS](https://gta6gps.com/map/game/races), [Ambrosia venue](https://warppipes.com/en/gta-6/races/racing-ambrosia-raceway).

**Discovered Styles:** The Quick Guide includes the reported full rosters of 13 Jason and 15 Lucia magazine styles, with directions for the remaining 12 guide-only styles. Scan applicable magazines and inspect luxury-watch pickups. Crowex Submerge 'Ratface' is listed in Luxury Watches. The [GTA6 Maps watch directory](https://gta6-maps.net/categories/luxury-watches) adds individual and alternate site maps; repeated names do not establish separate style credit. The [Book Swaps map](https://gta6-maps.net/categories/book-swaps) lists Killer Prom separately from Book Swap 1; [the seven-library guide](https://cutscenewire.com/games/gta-6/collectibles/gta-6-all-book-swap-locations/) identifies only one Key Lento shelf. Entries identify the style or watch where known; unidentified pickups remain labeled as search leads. Approximate named magazine maps and reported pickup guides supplement exact markers where those are unavailable.

**Missing Persons (story spoilers):** Follow the Starlet Motel poster, warehouse clues, three missing-tourist investigations, and the optional warehouse/mine finale. The five checklist stages represent three victims and related investigation tasks. Individual in-game maps are linked for the warehouse and each victim site. Some players report both protagonists must be active for the final tip-line call; solo and paired approaches may lead to different finales. Save before the return.

**Publications:** Entries include alternate copies where reported, along with chapter restrictions for missable reading locations.

**Andrew Node's Thrax:** In Chapter 3, scan ten No Flusher tags (the ten Ocean Beach locations provide one convenient route), follow the group chat to headquarters, read the keypad note (`8554013` or `85540133` reported), and inspect the office for the Sabal Circle address. For early access, park a vehicle close to the closed garage, crouch into the gap facing the door and open your phone until you clip inside. Use the internal switch and drive out. During Moses' introductory Chapter 2 registration job, players also report registering this car for free. Save first: clipping may fail, drop you through the map or be patched. Normal registration is reported at $70,000. The 15-tag collection remains separate. See Standout Vehicles and Secret Missions.

**No Flusher scans:** Search the *ground-level pavement* with WAiNK rather than scanning walls. The 15 documented sites include street, intersection, and building clues. The in-game category contains 15 tags, all listed with their reported map landmarks here. Selected tags also include street-map screenshots and photographs of the actual markings. A [complete street-by-street route guide](https://gta6.evergamer.com/no-flusher-tag-and-peewees-billboard-locations) is available through NFC for additional tags; this is a shared guide, not an individual map pin. The Andrew Node’s Thrax entry includes alternate shop-based route references; their correspondence to the numbered tags is not established. See the [street-level location guide](https://thegameswiki.com/gta6/wiki/noflusher-tags), [PowerPyx WAiNK guidance](https://www.powerpyx.com/gta-6-all-no-flusher-tag-utility-markings-locations/), and [WAiNK scan guide](https://game-checklists.com/gta6/waink-scans-checklist/).

**Unique Playlist posters:** Four La Perle/Crosstown posters unlock *The Sniper of La Perle* under **On Demand**; a Downtown nightclub poster unlocks *The Vault* under **Radio**. Track all five poster locations individually.

**Kenton’s Treasures and underwater wrecks:** Use the named wreck routes and individual relic clues to search cargo holds, cabins, debris fields, and surrounding seabeds. Published completion guides call for five of the seven named sets. The *Nuestra Senora del Ocio* wreck in *Island Dreams* yields ten Gold Doubloons and is an underwater discovery, not a separate Kenton set. Unconfirmed offshore wrecks and the Misericordia marker appear under Location Search Leads. The five numbered Burg facilities count as discoveries; legacy unnumbered lab leads remain separately tracked. Facility 1 is west of Mile Zero, Facility 4 north of Mile Zero, Facility 2 east of Little Hardin Island, Facility 3 west of Wild Hog Point, and Facility 5 east of Key Lento. [Facility maps and dive guide](https://www.pushsquare.com/guides/gta-6-underwater-discoveries-locations-and-kentons-treasures). The alternate GTA6Zero pins are estimates derived from published in-game maps, not exact seabed coordinates. For the Huntress or Rugger Fortune, Push Square reports double-tapping Triangle for autopilot and holding Left to access the anchor control. GTABase reports using the detector from the weapon wheel: a green light indicates the direction of a find; stay near the seabed. [Detector guide](https://www.gtabase.com/gta-6/articles/how-to-find-treasures-underwater-for-kenton-in-gta-6).

**Additional watch locations:** The [November 25 watch roundup](https://www.vicecityhustle.com/guides/gta-6-luxury-watches) supplies alternate Ashore and Argonaut leads and a northern gated cabin village report for Ambassador Sprunk (unassigned; exact site and collection credit unknown). Its Watson Bay entry does not establish the Quantum model. Use the in-game wardrobe and counter to reconcile repeat reports.

**Cocaine flamingos:** The app includes 103 checklist records and 48 provisional search leads. These 151 records are not 151 unique pickups. Six GPS markers are included as provisional Buckskin, Lummox, Ambrosia and Mariana search leads. The 50-entry [GTADB guide](https://gtadb.net/articles/gta-6-cocaine-flamingos-locations/) maps to 50 existing records, including three separately reported figures at Meerwunder. [PowerPyx](https://www.powerpyx.com/gta-6-all-cocaine-flamingo-locations/) documents eight of these; [Push Square](https://www.pushsquare.com/guides/gta-6-cocaine-flamingo-locations) lists five Key Lento stops, with overlap still unresolved at Rusty Anchor. Shared map pins, likely aliases and multi-figure sites are distinguished in the notes. The [GTA6 GPS directory](https://gta6gps.com/map/game/flamingos) includes an identification/tutorial pin, not a collectible. [GTA Town](https://gtatown.com/collectibles/cocaine-flamingos) and [GTABase](https://www.gtabase.com/gta-6/map/?feature=cocaine-flamingos) offer additional partial coverage. In-game total: 125; 100 required for 100% completion; reported cheat unlock at 50. Destroy the black-and-white-beaked figures, not yellow-beaked decorations. Focus (both sticks) can highlight nearby targets; verify each counter increase in Progress > Collections > Exploration. Collecting the contents is optional. A [video advertised as all 125 locations](https://www.youtube.com/watch?v=tzCkxqF9tI0) is available, but its full route and individual credit remain unverified. See [MeriStation](https://as.com/meristation/guia_pagina/todos-los-flamencos-con-cocaina-en-gta-6-ubicaciones-y-mapa-interactivo-para-encontrarlos-f202611-n/) for Focus guidance and the MapGenie filter. The GTA Town map lists 54 markers; some markers cover multiple figures. Marker count is not a unique-pickup count. Not all 125 individual pickup locations have been established.

**Collection techniques:** Destroy cocaine flamingos with black-and-white beaks; black-and-yellow beaks are reported as decorative. Use WAiNK for style magazines, artwork, landmarks, and applicable scans. For underwater pickups, equip scuba gear and follow the wreck or cave directions.

**Messages in Bottles:** Follow the numbers on the six shoreline notes around Key Lento and the Shoeless Keys. Each entry provides a landmark and individual location reference; guide numbering may differ.

**Stunts:** The four stunt categories contain **397 listed challenges**. Published completion guides report that 300 count toward 100% completion. Mark each challenge only after completing it in-game. Bridge crossings share progress with their regional and stunt views.

**Alternate publication copies:** Named community map references include the Hamlet and Emerald Springs Law & Litigation in Leonida sightings, plus alternate book and magazine locations. Read the title to confirm the match; multiple copies share a single checkmark.
**Publication chapter cutoffs:** The five copies in Jason's Key Lento house become unavailable after Chapter 1, the two Starlet Motel copies after Chapter 2, and the two Bayside Apartment copies after Chapter 3. These are **missable copies**, not necessarily permanently missable titles; alternate copies may exist. Check these locations before advancing the story. See [PowerPyx's publication guide](https://www.powerpyx.com/gta-6-all-magazines-books-comics-locations/) and the [Key Lento route](https://www.pushsquare.com/guides/gta-6-key-lento-collectibles-and-secrets-walkthrough).

## Activities / Robberies

The **Collections → Activities / Robberies** group provides individual checklists with location clues, chapter unlocks, and completion requirements. The app tracks manually recorded progress; it cannot read in-game completion.

| Checklist | Tracked entries | Guide-reported completion rule |
| --- | ---: | --- |
| Gang Operations | 39 | Clear all 39 by taking every key valuable. Ultimate Edition-only bonus operation is excluded. |
| Small Bank Jobs | 14 | All 14; Chapter 3 *Credit Check* unlock. Take drawer and safe. |
| Large Bank Vaults | 8 | All 8 for the bank-job total; unlock after Chapter 4 *That's Where The Money Is...*. Take vault, drawer, and safe. |
| Payday Loan Jobs | 7 | All 7; Chapter 3 *Credit Check* unlock. Take teller cash and back-room safe. |
| Fence Sale Milestones | 4 | Manually mark when in-game cumulative sales reach $100k narcotics, $100k jewelry, $250k marked bills, and $200k vehicles. |
| Challenge Milestones | 19 | Two reported vehicle-cheat thresholds per challenge family (5/10 and 10/10), plus 50 challenges for a trophy, 70 for 100%, and all 80 for the P-996 Lazer cheat. These are **19 milestone checkboxes**, separate from the **80 individually checkable objectives** listed under Challenge Objectives. |
| Challenge Objectives | 80 | Each named Combat, Criminal, Exploration, Fitness, Shopping, Sports, Survival, and Vehicle objective has its own checkmark and requirement. Complete 70 for the published 100% target. |
| Bret's Target Decks | 15 (4 reported species lists, 11 numbered) | All 15 Jason-only hunting contracts have separate checkmarks. Species and hunt areas are provided for the first three; Deck 04 has a reported species list and Central West search region; later entries retain their numbers and link to regional hunting guides. Six optional hunting-region leads document four complete reported target lists and two partial lists without guessing their numerical order. |
| Legendary Animals | 11 | Nine Bret clue hunts and two rare creatures; separate from Target Decks and the eight-animal challenge counter. Individual habitat-map screenshots are available for all 11; the marked zones and possible spawn clusters are not guaranteed stationary locations. |
| Smash and Grabs | 35 | At least **34/35** for 100%; pawn shops after *Coffee Break*, jewelry after *Yasmani's Jewelry Jobs*. Check the back room. |
| Stick-Up Store Leads | 107 reported store leads | The game counts **91** eligible stick-ups, of which **50** are needed. Store positions may be approximate; check the in-game robbery-map filter. 100 leads have individually assigned GTA6GPS venue pins. Four additional numbered Balsero and Buckskin Swamp branches have individual alternate maps; the two Grassrivers records offer both branch maps until the correct branch is identified. The generic liquor store has a reported Little Cuba venue map. All 107 leads offer individual venue maps or paired candidate maps. Venue pins do not establish robbery eligibility. |
| Kitty Litter Labs | 20 reported lab sites | Follow Tyrone’s Chapter 3 storyline; locate the smoking lab vents and laundry vans. Individual search-area screenshots and timestamped walkthroughs supplement map pins and written directions. The 20-site map roster is represented, including North Crosstown Condo (5191); the North Crosstown garage match remains provisional. The Spin On This finale remains tracked in Optional Missions. New named GPS markers cover Zenon, Rockridge apartment, Slator Towing and Safia; the Slinger marker is a comparison lead only. |
| Drone Time Trials | 42 trials at 14 venues | All 42. Beat par on first two trials per venue; finish the third once. Venue-level maps are shared by the three courses; five venues also have individually illustrated map screenshots. |
| Shooting Gold Medals | 42 medals at two ranges | All 42 individual gold medals, across 12 courses. |
| Race Challenges | 13 | Gold in at least **10/13**; separate from unique races. |
| Arcade Video Games | 4 | Play all four; simply starting and exiting counts for the guide's 100% target. |
| Pool Opponents | 7 | Beat at least **5/7** different opponents. |
| Golf Courses | 5 total: 3 mini, 2 full | Play **2/3 mini golf** courses. The two regular golf courses are optional reference entries. |
| Unique Race Venues | 13 site leads covering 40 races | **30/40 unique races** needed for 100%; a checked venue means **visited**, not that its races are all won. Individual race wins are tracked separately in Collections > Activities / Robberies > Unique Race Wins. |
| Unique Race Wins | 40 named races | First-place finishes tracked separately; 30/40 needed. Includes the Keep It PG Rallycross Loop. |
| Basketball Hoops | 100 individual GTA6GPS maps | **90/100 hoops** required for the reported 100% threshold. Score at each distinct basket you check. |

**Basketball Hoops:** The checklist uses all 100 individual [GTA6GPS hoop markers](https://gta6gps.com/map/game/basketball-hoops), grouped by region and identified by their map pin. Each entry directly emulates its own map. Score at each distinct basket before checking it off. The former numbered guide and duplicate search leads are no longer tracked; only the GTA6GPS checklist contributes to hoop progress.

**Rare animal sale values:** Nine wildlife entries now include reported Pristine and Good payouts from the [November 25 GTA Zone hunting guide](https://gta-zone.com/news/gta-6-hunting-guide/). These are ordinary rare variants, not Snapmatic rewards.

**Bret’s Target Decks:** After Chapter 2 *Coffee Break*, meet Bret Vogel as Jason and complete *Veteran Hunted*. Track all 15 hunting contracts separately. The first three decks include reported species and habitat requirements: whitetail buck and doe; wild white tom turkey, eastern spotted skunk and wild boar; and cane toad, nutria and Nile monitor. Deck 04 reportedly requests Golden Fox, Eastern Diamondback Rattlesnake and Nine-Banded Armadillo; verify the active text. For decks 5–15, follow Bret’s texts and the purple **Map → Activities → Target Deck** markers. Location Search Leads includes six named regional hunting cards (Foothills, North Kelly, East Pelican Valley, Hamlet, Pelican River and North Bobcat Key); four have three reported targets and two have one documented target each. A hunting guide reports a Shrewsbury SMS-01 unlock after Deck 09. The deck number correspondence remains unresolved; the numbered contracts also provide general hunting-area maps and weapon/delivery guidance. The first three decks include individual hunting-area map references; these are not fixed animal spawn points. Mark a deck complete after Bret accepts the order. Check **Progress → Contacts → Bret Vogel** for the in-game total.

**Legendary Animals:** After *Veteran Hunted*, wait for Bret’s specific Snapmatic clue before searching for the nine legendary targets. Karkinos and Gang Panther have separate entries. Study and log each animal, then follow the delivery or photo prompt. Search-area maps describe roaming habitats, not guaranteed stationary spawns. Verify the eight-animal Legendary Hunter challenge separately. All 11 animals have individual third-party search-area references and visual identification guides. GTA6GPS now provides ten named animal habitat markers (all except Karkinos); older radar, dairy and golf-course markers remain separate landmarks. All markers are approximate.

**Challenge milestones:** The game lists **80 named Challenges across 8 families of 10**: Combat, Criminal, Exploration, Fitness, Shopping, Sports, Survival, and Vehicle. Their full objectives are under **Progress > Challenges**. The companion tracks **all 80 named objectives** individually in Challenge Objectives, plus the **5/10 and 10/10 milestones for each family** and overall **50/70/80** thresholds. Completing an objective does not automatically check its associated milestone; mark milestones after verifying the game's progress count. Published guides associate the family milestones with vehicle cheat unlocks. Cheat use remains gated behind story completion, and the app does not activate cheats. For the full named objectives, see the Challenge sources below.

The **22 Bank Jobs** comprise **14 small banks** and **8 large vaults**. Some banks share names and neighborhoods; refer to the entry notes to distinguish each vault. All 22 bank entries have individual GTA6GPS map links; the Ocean Beach Lombank branches are distinguished by their reported payout ranges. In-game filters: **Map > Robberies** for crime, or **Map > Activities** for racing/sports. Completion thresholds are guide-reported, not official app tracking.

**Related optional missions:** Billy, Andy/Cellerica, Tyrone, Yasmani, Dunk, Eddie Acosta, Shayna, and Yago have individual checklist steps under *Optional Missions*. Shayna's Lucia-only mission and Yago's timed race require prompt attention. Check the *That's Where The Money Is...* optional mission before attempting large vaults. These are not duplicated under *Secret Missions*.

**Bank and racing tips:** Payout ranges are guide estimates, not guaranteed rewards. At small banks, search the drawer and safe; at large vaults, take the vault loot, drawer, and safe, and prepare for a higher wanted level. The 13 Unique Race Venues entries cover access points and unlock requirements. Both Unique Race Venues and Unique Race Wins are under Collections > Activities / Robberies; checking a venue does not mark every race held there as complete, and winning a race does not automatically mark its venue visited.

**Gang Operation pickup tips:** The 39 operation entries identify reported rare-weapon and Standout Vehicle pickups alongside their related watches, within their checklists. Examples include the **Truth & Justice Caracara** at June Warehouse, **Smuggler Caracara** at Brooks Boatworks, **Firebug Dominator** at La Cascara Motel, **Searing Zombie Bobber** at Farm Fix Meth Lab, and **Royal Ruby Emerus** at Vespucci Mansion (small entrance garage; press OPEN left of the vehicle gate). The **Ashore** watch is also reported at Southside Trap House and Saber Shipping Facility; these are alternate acquisition sites, not separate model checkmarks. The **M7 Compound** is also called **Nova Auto Junkyard** in some guides. Safe combinations are guide-reported, and the Safe & Door Codes menu retains conflicting combinations where sources disagree. [PowerPyx operations list](https://www.powerpyx.com/gta-6-all-illegal-gang-operations-locations/), [operation key valuables and codes](https://leonidaplayers.com/guides/gta-6-illegal-operations-all-gang-hideout-locations-loot-safe-codes), [Palmer gold KZ-59](https://gta6-japan.com/en/guides/la-polilla-negra-kz-59-palmer-fishing-warehouse/), and [Insider Gaming pickup inventory](https://insider-gaming.com/all-illegal-operations-locations-key-valuables-in-gta-6/).

**Fast Focus leveling:** Visit Rodgers Ranch & Shooting Range in Kelly County, northeast of Port Gellhorn. Buy the $5 waiver and select Market Range Beginner. Bring a pistol or rent one (reported $10). Hit the six targets, then choose **Quit** and back out completely rather than selecting Restart. Start the challenge again and repeat until the purple eye stat reaches Level 4 in the Jason & Lucia character menu. Each exit reportedly grants lasting Focus, but gains can vary. Repeat separately for each protagonist once switching is available after Coffee Break (Chapter 2). This is stat training, not the Unlimited Focus cheat; no cheat activation is needed. The method may change with patches.

## Vehicle locations, deliveries, and properties

The vehicle sections describe the reported pickup area, the particular vehicle or livery, and any relevant story or access condition. An entry described as a **search area** should not be treated as a guaranteed parking spot.

### Vehicle hunts

**Randy's Wish List has 22 tracked requests.** Each tracked request includes a map, targeted location guide or illustrated spawn reference, including the final Dalton Island mission. Variable vehicle sightings and nearby landmarks are labeled separately from fixed mission pickups; the Cypher, Carbonizzare and Neon are reported crossed out on some boards, so verify the active request. The Leaf Links Mamba has a dedicated mission-area map. Find delivery vehicles using reported streets, buildings, parking areas, and mission hints. Examples include:

- **Windsor Drop:** Status Auto Imports on Caledonia Blvd in Bayside, or luxury residential driveways in northern Capri.
- **Ocelot Jugular:** outside BITE on Atlantic Avenue in Ocean Beach, or the Cellerica garage on Chastity Ave in Belville.
- **Jubilee:** Shore Drive beachfront parking in Ocean Beach; northern Capri is also reported.
- **Itali RSX:** a villa garage on Vespucci Island; an alternate Capri location is reported.
- **Dubsta:** check parking opposite **NINE1NINE** on NE 7th St in Downtown, **N-X 94 Vice Car Repair** near VCIA, and hotel parking in Vice Beach or Ocean Beach.
- **Baller ST-D:** beige residential driveways in northern Capri.
- **Übermacht Cypher:** the Übermacht dealer in northern Southside or outside Cirque de Cuisine in Ocean Beach.
- **Grotti Carbonizzare:** beneath Aurum Condominiums in Tequesta, at Status Auto Imports, or in northern Capri public parking.
- **Pfister Neon:** the lakeside mansion north of Belville, Peacock Estates, or northern Capri luxury homes.
- **Truffade Thrax:** a reported final Randy job at the Dalton Island Club parking area, accessible by ferry from Waxman Island. Its availability is mission-dependent.

Vehicle entries include individual interactive map pins or map screenshots of reported spawn areas. The game's **Map > Robberies** may offer more current hints for active jobs.

**Free registration:** During the Chapter 2 Take a Vehicle to Moses job, one eligible vehicle can be registered free at Watkins Auto Parts. Check the tracker and fee before accepting; Bison has six cargo slots, Dominator two, and the Montagne XL is available behind the shop. See [KeenGamer - registration choices](https://www.keengamer.com/articles/guides/gta-6-3-best-cars-to-register-for-free-at-mosess-garage/).

**Moses Drop-Offs has seven deliveries.** The guide covers the Chapter 2 story delivery and six further pickups. After delivering all seven, wait for Moses' text before collecting his **Emperor Sultan** at the Port Gellhorn shop. The reward has its own Standout Vehicle checkmark. [Collect Moses' Gift walkthrough](https://www.powerpyx.com/gta-6-collect-moses-gift-walkthrough/)

**Wyman's Classic Cars has four restoration projects and a fifth checkmark for the final '72 Deviant.** The four restoration entries give directions to 15 wreck parts, each with an individual pin or targeted component guide in the NFC Links list. The Mamba GT includes the pond wreck map; the Reynolds Kit Car includes five reported component markers, with two candidate identity matches labeled. Guides identify this as Ultimate Edition content. **Access:** The Reynolds Kit Car's Wild Hog Point container needs the cut-off tool from Chapter 3 *Meet the Heders*. The four rebuilt cars cost $1,500 each after Wyman sends a BuckMe request; the final Deviant is a direct $1,000 offer (no fifth wreck search). [Restored-car ownership guide](https://www.vifieldguide.com/guides/classic-cars/) reports $100 registration or garage storage; abandoned cars return to the scrapyard.

### Standout Vehicles

The **299-entry vehicle catalog** covers distinctive cars, motorcycles, aircraft, watercraft, liveries, and mission and stunt rewards. Entries give a pickup landmark, search area, or reward requirement where published. Named community map markers from the [Standout Vehicles directory](https://gta6-maps.net/categories/standout-vehicles) are available through NFC where matched to the catalog. These include reported aircraft locations for Kyle’s Mallard, the named Wyman restoration collection sites, and additional individual land and water vehicle markers. Mission and reward markers do not establish unrestricted free-roam access. Treat these as reported vehicle sightings; map sources may disagree on the exact region or pickup point, and access requirements should be checked in-game.

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

**Special vehicle rewards:** The catalog includes story, stunt, Ultimate Edition, and GTA+ vehicles alongside ordinary parked variants. GTA6 GPS gives a Vice-Dale County marker for the **Seashell Boomazoom** at X 315, Y 384. Exact parking and access details remain unconfirmed. [GTA6 GPS — unique vehicle sightings](https://gta6gps.com/map/game/unique-vehicles)

Additional named sightings include the [Anarchist Dominator](https://gta6gps.com/?p=pin-918643) on the top garage level, the [Rocket Slamvan](https://gta6gps.com/?p=pin-918558) near Watson Bay, the [King of Swing Youga](https://gta6gps.com/?p=pin-918470) in Mariana County, and the [Chapter 5 Ultimate Edition Cheetah](https://gta6gps.com/?p=pin-990003) in Pegassi Residences. Verify variants in WAiNK.

**Alternative pickup reports, one checkmark per vehicle:** Gold Rush Tempesta is reported outside **Cirque de Cuisine (Shore Drive, Ocean Beach)** or on the **upper covered parking deck of Crimson Palms Condominiums (Washington Beach)**; both reports call for the Chapter 3 Key Cloner, but may be conflicting descriptions of one car. Venomous Marshall has reported sightings at the **Boone Speedway car show (southwest Hamlet)** and in the **Land of Plenty barn (eastern Mount Kalaga)**; a third map marker lacks published directions. Guides mark two sites for **Erotic Journey** in Watson Bay and **Pink Licorice Astron** around Belville, without yet providing a dependable second address for either. The museum/VCPD recruitment display notes for Valiant Tempesta may identify the same location. Use WAiNK to confirm identity; alternate locations share the same vehicle checkmark.

**Edition and reward distinction:** Wyman's Classic Cars entries track salvage and restoration tasks; the named restored Standouts track the resulting vehicles. The garage entries for **Paradise Garage** and **GTA+ Crosstown Garage** track property access, while the corresponding unique vehicles are listed by name in Standout Vehicles. Ultimate Edition and GTA+ rewards require the appropriate entitlement, and some unlock only in later chapters.

**Anime vehicle notes:** The Akane on Dolphin Drive is reportedly unlocked and has no tracker; a mod-shop replacement livery may permanently remove its anime wrap. Yago's Elegy is not a free roadside pickup: finish his four-part Jason storyline, including the time-sensitive *Race Against Yago* hourglass mission, and keep the final mission car. Bumper to Bumper requires the Custom Street Racing Build and Nitrous Boost (paid by Yago).

### Garages and safehouses

Published Story Mode guides report no purchaseable houses or passive-income businesses; garages are purchased through the real-estate phone app when unlocked. **Garages** covers **16 purchase, story, hidden, and membership/edition entries**, including published prices, storage capacity, access conditions, and benefits: eight purchasable garages, four story rewards, one hidden garage, and three edition or GTA+ benefits. **Houses / Cribs** lists **five story safehouses**, including the Chapter 5 **Pegassi Residences penthouse** on Bayside Dr, Tequesta Retreat. Its three-slot garage has its own entry under Garages. Individual property map links identify the building or reported search area. All five listed safehouses have individual GTA6GPS map pins. The GTA+ Crosstown Garage is reported as Caledonia Boulevard Garage, beside Shunters body shop in Crosstown. Its dedicated [garage map](https://gta-6-map.com/place/caledonia-boulevard-garage) and [pickup guide](https://timesaver.gg/tools/gta-6/cars/free) are included. GTA6GPS provides a nearby Dutch Courage landmark, not a dedicated garage pin; confirm the garage icon in-game.

**Randy alternate vehicles:** [Screenshot-backed scan examples](https://gta6gps.com/map/game/randys-wish-list) show accepted models and variants outside the fixed mission pins. The displayed sale value depends on condition and is not the mission payout. Check the active request and scan before delivery.

## PS5 and Xbox Cheats

Choose **PS5 Cheats** or **Xbox Cheats** to browse **61 reported effects** grouped by type. The catalog includes **21 main-catalog D-pad inputs per platform (reported or converted)** (including Videotape and Dark Mirror), unlock notes, warnings, and **28 screenshot-derived sequences marked unverified**. Twenty-four screenshot sequences appear in the separate **Unverified codes** list, while four correspond to main-catalog effects. Xbox labels translated from PS5 screenshots are not independently captured Xbox codes.

Published guides describe entering cheats from the game's phone in free roam: **D-pad Up**, then **L1 + R1** on PS5 or **LB + RB** on Xbox, followed by the sequence. See the in-game **Progress > Cheats** area for availability and unlock information where applicable. Guides warn that using cheats may affect saving and trophies or achievements. **Save before enabling cheats.**

The app displays codes and instructions; it does not send controller inputs. Screenshot-only inputs and the single-source **Pinhole** entry are expressly marked as reports rather than confirmed sequences.

## Mock Loaders

The app includes **45 full-screen images**, with **30 GTA VI** scenes and **15 Dungeons & Dolphins** scenes. The gallery browses the enabled sets. Press **Left** or **Right** to switch images and **Back** to exit.

**Mock Loader Settings** offers separate toggles for the GTA VI and Dungeons & Dolphins sets. Use **Up/Down** to choose a set and **OK** to enable or disable it. The default gallery shows GTA VI. Enable Dungeons & Dolphins to add its 15 images. Disabling both sets displays **No sets enabled** in the gallery.

When the app launches, it shows a **random image from the enabled collection(s) for 0.5 seconds**, then opens the main menu. If both sets are disabled, the main menu appears without a splash. Image files are loaded individually for display. When opening or returning from GTA NFC Link, an animated hourglass overlays one randomly selected image from the enabled sets. Disabling both sets also skips the mock loading screen during module transitions.

## Map and guide coverage

The catalog contains 10,150 distinct item-to-URL references across 2,877 linked entries. These include individual map locations, shared venue maps and walkthroughs; they do not imply 10,150 distinct pickup sites. All 480 named cards, 397 stunts and 89 WAiNK landmarks, curiosities and sculptures have item-level references.

### Activity and collection details

- Shooting Range: all 42 medal entries include reported target scores or times.
- Drone Trials: 42 trial entries include published prize amounts and par times.
- Race Challenges: all 13 entries include medal targets; Unique Race Wins tracks 40 separate races.
- Wildlife: the Quick Guide lists 255 reported animal entries across eight groups, including identification and behavior clues for all 61 hunting animals and 60 birds.
- Book Swaps: all seven shelves have visual approach clues and reading controls; the publication guide includes alternate copies during Climbing the Cellerichain.
- Kenton’s Treasures: all 41 reported relics are listed across seven sets, separate from the 33 underwater discoveries.
- Standout Vehicles: available performance ratings, acquisition instructions, registration costs and tracker conditions are included where reported.
- Luxury watches, meteorites, flamingos and search leads contain unresolved identities; check the in-game collection counter before treating alternate sightings as new collectibles.

## Sources and reference accuracy

GTA VI Companion is an **unofficial fan reference** based on linked third-party guides, interactive maps, screenshots, and community discussions. These are **published reports, not official Rockstar data**. Codes, collectible positions, vehicle spawns, mission availability, prices, and exploit behavior can depend on story progress, character, time, or game patches. A published map pin or general search area does not establish an exact, repeatable spawn.

Where sources disagree or a position is not specific, entries use labels such as **reported**, **unconfirmed**, **variable**, and **conflicting**. Collection totals in this README describe entries available in the app and should not be interpreted as a claim that every in-game collectible is cataloged.

## Project and community

- [GTA VI Companion source repository](https://github.com/RogueMaster/GTA_VI_Companion)
- [Patreon](https://www.patreon.com/RogueMaster)
- [Extended source directory](more_sources.md) — additional maps, walkthroughs and guide references.
- [Changelog](changelog.md) — release history.
- [Catalog maintenance notes](extra.md) — internal reconciliation and source discrepancies.
