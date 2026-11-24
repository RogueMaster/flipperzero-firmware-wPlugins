# GTA VI Companion changelog

## v2.0 — Expanded Location References
- Improved all 11 Legendary Animal map labels, added labeled search landmarks, and expanded Randy vehicle sightings, mission maps and access notes. Existing progress IDs and NFC behavior are unchanged.
- Removed the legacy hoop checklist and duplicate hoop search leads; the 100 individual GTA6GPS hoops are the only active basketball collection. Added Caledonia Boulevard Garage location references for the GTA+ garage and its Venturas BLVD Elegy reward, including a clearly labeled nearby GTA6GPS landmark.
- Use the full 100-pin GTA6GPS basketball checklist as the primary hoop list, retaining the numbered guide and its saved checkmarks separately. The aggregate counts only one hoop collection.
- Reconciled published GTA6GPS basketball markers with shared-court candidates and separate unmatched location leads. Added reported vehicle and watch matches and labeled venue-only references; retained exact guide maps and existing progress IDs. Confirmed all five safehouse references.
- Added 24 individually mapped search locations and refined existing location labels and access notes. Moved misplaced README references into the Secret Missions, Collections, and Vehicle sections. Existing IDs and NFC behavior are preserved.
- Added 30 labeled GTA6GPS associations: 26 new optional records and 4 additional references for existing IDs. Expanded vehicle sightings, card pickups, basketball courts, flamingo wreck markers, street art and optional Easter eggs. Existing pickup maps and NFC behavior remain unchanged.
- Added 28 labeled GTA6GPS associations: 26 new optional search records and 2 additional references for existing IDs. Expanded vehicle sightings and directions, card pickups, hoop maps, meteorite leads and a Hamlet style location. Existing pickup maps and NFC behavior remain unchanged.
- Added 39 labeled GTA6GPS associations: 32 new optional search records and 7 additional references for existing IDs. Expanded vehicle sightings, card pickup clues, hoop leads, unidentified publications and optional Easter egg maps. All five murals now offer the shared reward garage as an alternate link; existing pickup maps and NFC behavior remain unchanged.
- Added 38 GTA6GPS item-link associations, 28 provisional location leads and 2 random-card pickups. Refined vehicle, dancer-card, hoop, street-art, flamingo and lab descriptions; matched additional landmarks without changing existing IDs, NFC navigation or encoding.
- Added 56 labeled location and guide associations, refined existing link labels, corrected numbered Burg facility directions, added 42 separate research leads and the named Maibatsu Donk Boomazoom Standout. Retained established collection IDs, existing links and map-first NFC navigation.
- Expanded Underwater Discoveries to 33 named sites; retained six wreck IDs, promoted the Underwater Hotel with its existing ID and preserved both unnumbered lab leads separately. Added optional Easter Eggs and 14-step Vice City Portal map checklists, Secret Missions map shortcuts, vehicle access notes and additional labeled map references. Updated Dispatch and progress-summary ranges without changing the NFC plugin or save format.
- Added 25 labeled GTA6GPS map associations for vehicle sightings, special weapon pickups, the Peewees reward restaurant and four provisional search leads. Preserved existing collection IDs, progress, and map-first NFC navigation. Conflicting weapon locations and the alternate Thrax sighting are tracked separately from established collection entries.

## v1.9.11 — Progress Capacity and NFC Catalog Optimization
- Replaced the two-button NFC URL picker and Wyman wreck carousel with a scrollable provider-labeled link list. Emulate the highest-priority map immediately, with GTA6GPS pins and category maps first. OK during NFC opens the alternate-link list when available; Back stops NFC and restores the originating checklist and focused row. Added optional partitioned alternate-location catalogs. Release selector buffers before loading NFC; retained the existing NTAG215 listener and NDEF encoding.
- Expanded collection progress storage to 8,192 bits using redundant `progress3.a` / `progress3.b` saves; import valid 4,096-bit and 2,048-bit saves without modifying them. Corrected the overall progress summary to include every active catalog ID. Added 12 individually mapped random-card pickup locations, seven unidentified style/watch leads and five reported flamingo search leads. Existing collection identities and checkmarks are unchanged.
- Partitioned item NFC links into fixed 256-ID TSV ranges; each lookup scans one file with category fallback retained. Added 35 individually mapped random-card pickup locations, without increasing the 480 named card identities. Retained all existing item IDs, NFC records and save migration support.
- Added 74 individual card NFC records and enriched 13 existing records with GTADB map links for Backyard Wrestling, Liberty City Penetrators, Vice City Club De Fútbol and Liberty City Wrath; retained existing IDs, prior links and alternate pickup reports.
- Expanded individual NFC references for Liberty City Swingers, Los Santos Corkers, Los Santos Panic, Vice City Narcos and America's Next Top Hooker; added 103 card link records (including targeted guide references for Taylor Blanc and Badge Teal), enriched 16 existing records, and documented alternate pickup clues without changing progress IDs.
- Completed individual references for all 480 cataloged card identities with 160 additional NFC records, including 157 individual map references and 3 targeted guide-only references. Reconciled the remaining six card sets, retained all prior links and IDs, and documented conflicting pickup locations without merging unresolved card identities.
- Allocated checklist and guide indexes at their exact sizes, packed checklist IDs and offsets into four bytes per row, and released temporary detail text after rendering. Added allocation guards for catalog buffers and complete progress records.
- Added 11 optional missions from Freddie, Dodos Rising, and Papa Audacity, bringing the checklist to 43. Added 44 individual references for missions, arcade titles, early Bret decks, weapon pickups, and the Crowex Octane 'Bobby Ambrose'. Corrected Billy's late mission chapters and arcade notes, and documented supplemental vehicle sightings.

## v1.9.10 — GPS and Collection Expansion
- Reconciled all 100 basketball hoop references to the current guide numbering and completed all 100 individual map previews (89 added). Expanded the stick-up search checklist to 107 reported stores, including six individual store pins; added 19 reported Kitty Litter lab locations, individual Mamba and Kit Car wreck guides, five unidentified style/watch search leads and a probable shared Vice Beach drone venue pin. Retained all existing progress IDs and distinguished reported store leads from the 91 eligible stick-ups.
- Added 97 NFC item-to-location mappings across 34 distinct GTA6GPS pins: 16 meditation completions at nine sites, 39 drone trials at 13 venues and all 42 shooting gold challenges at 12 courses. Added course fee and weapon-class details, selected drone rewards, Bret hunting clues and the five-lab Burg Industries search route. Retained all existing progress IDs and guide references.
- Added 101 direct GTA6GPS stunt-jump pins and 8 named collectible-card pins. Retained shared guide links, progress IDs and existing collection counts. Added chapter availability and dive-batch guidance for six underwater wrecks and five Kenton sets.
- Added individual GTA6GPS NFC pins for all 40 Knife Flights and 56 Under the Bridge crossings. Expanded aircraft acquisition and stunt completion instructions, plus La Noche and Lujuriosa chest search clues. Preserved all progress IDs and earlier source references.
- Added 66 individual GTA6GPS NFC mappings: 52 Under the Bridge crossings (including all 17 Lummox crossings), all seven Pool Opponents and all seven Payday Loan Jobs. Added six individual GTA Walkthrough atlas links for Messages in Bottles, matched to shoreline clues. Expanded pool table access directions and payday venue details. Retained all progress IDs and existing links.
- Added 77 individual GTA6GPS NFC mappings: 62 additional Under the Bridge crossings and 15 named collectible cards. Expanded card pickup landmarks and retained alternate location reports. Flagged conflicting card identification for the Cottonmouth 44017 trailer safe. Preserved progress IDs and existing individual source links.
- Added 61 individual GTA6GPS NFC mappings for the remaining 32 Under the Bridge crossings and 29 Stunt Jumps. All 397 listed stunts now have individual map pins. Matched alternate pin names using the reported stunt details and corrected the Gellhorn Roadworks East double-backflip wording. Preserved all progress IDs and existing individual links.
- Added 24 individual GTA6GPS NFC mappings for named dancer cards, including alternate pickup reports for Amber, Vicki and Tracy. Added location-specific map guides for the remaining two meditation sessions and three Vice Beach drone trials, plus six basketball access and court clues. Preserved all progress IDs, existing links and collection counts.
- Added 59 individual GTA6GPS NFC mappings: all 35 Smash and Grabs, 12 race challenges, 10 unique race venues and two style magazines. Expanded venue locations, reported in-game proceeds and security, race medal targets and selected vehicle rating requirements. Preserved all progress IDs, existing links and collection counts.
- Added 24 individual NFC location references: 12 Gang Operations, three golf courses, seven luxury watch entries and two publication titles. Corroborated two existing magazine links. Documented probable generic map matches, shared watch venues, alternate magazine copies and golf progress tracking. Preserved all progress IDs, existing links and collection counts.
- Added nine GTA6 GPS item references and one Pop Star makeup guide reference. Identified and moved the reported Pop Star and Circles of Hell Skull search leads to the corresponding style and tattoo menus, preserving their IDs; indexed both in Regional Dispatch (923 stops). Expanded magazine, watch, arcade and hoop clues; noted uncertain style identities and alternate pickup reports. Corrected the Lucia style count in the README.
- Added 21 individual NFC item mappings: all eight large bank vaults, five playlist poster entries and eight unique-animal search areas. Distinguished the Ocean Beach vault branches, expanded playlist and wildlife directions, documented alternate Vault posters without adding progress IDs, and refined the Gibson Brothers and Druthers bank locations. All 22 bank entries now have individual GTA6GPS references.
- Added 14 NFC item references: four missing-person stages, three remaining legendary animals, five existing watch entries and two reported watch models (Ashure Chrono and Kaleidoscope). Added a shared victim-site map image to three investigation stages, a current GTA6GPS underwater lead pin, and the Casa Guardia alternate watch pin. Expanded access landmarks and alternate watch locations, corrected five invalid secondary URL fields, and extended progress tracking for the two new watch entries. Exact Vice Beach buildings and independent watch counter credit remain unconfirmed.
- Added 99 NFC item references for the remaining basketball hoops: eight probable GTA6GPS pin matches and 91 numbered GTA6MAP.IO guide links. Added eleven direct hoop map previews (including the existing Catalan Key entry) and corrected two invalid secondary URL fields. All 100 hoops now have location references; guide links use browser text fragments and are not direct GPS pins.
- Added item-specific named guide links for all 112 AHOL Street Art pieces, with the interactive category map as a secondary reference. Existing progress IDs and collection counts remain unchanged. The cumulative 1.9.10 release adds 880 NFC item records (1,515 total catalog records), including direct pins, targeted guide links and shared venue references.
- Added 26 NFC item records and upgraded 23 existing records: 16 named cards (two GTA6GPS pins and 14 individual map screenshots), three race venues, the Ambrosia trial, Exotic Animals Front, Belpoint Lighthouse and four offshore search leads. Added individual atlas pins to 22 numbered AHOL pieces and a probable GTA6GPS pin for the Key Lento garage hoop. Retained the named guide and existing map image references; category maps remain in the README. Four new search-lead IDs are separate from the underwater discovery counter. The cumulative release adds 906 NFC records (1,541 total); 23 existing records were upgraded in this pass.
- Added 24 NFC item records and upgraded 14 existing records: nine Standout Vehicles, all 13 remaining Randy requests, two Wyman entries, an additional Mamba lead and 13 dive-site map images. Added Wyman’s multi-wreck NFC selector for individual reported component markers and full guides. Added a dedicated Map button to Infinite Money Glitch for Apollo Aggregates off Emory Avenue, using the existing NFC plugin without modifying checkmarks. Retained earlier source links and progress IDs; four unnamed Kenton markers remain unresolved. The cumulative release adds 930 NFC item records (1,565 total), excluding the separate walkthrough map button.
- Added 59 individual NFC records: 40 named cards, four Standout Vehicles and all 15 No Flusher tags. Includes 39 map-image associations, 12 interactive map references and 8 additional item-specific tag guide links. Retained alternate spawns, existing progress IDs and prior URLs; documented uncertain card variant and street-marker matches. Cumulative v1.9.10 adds 989 NFC records (1,624 total).

## v1.9.9 — Memory Optimization + NFC Emulation Fixes
- Added 41 individual GTA6GPS NFC item mappings (40 distinct pins): all 25 BASE jumps, five 1010 murals and the Cheetah reward garage, four Peewees billboards, two named mini-golf courses, the pink-car trunk investigation site, Lucia makeup and the shared Tarot tattoo magazine. Preserved all existing item IDs, links and map-image references. Corroborated the Belpoint flamingo route and retained 94 reported pickups and search leads.
- Added 88 individual GTA6GPS NFC map pins for 30 WAiNK landmarks, all 35 curiosities and all 23 sculptures. Expanded Cocaine Flamingos to 94 reported pickups and search leads with an individual wrecked-car map marker; added that lead and the existing dockside lead to Regional Dispatch (921 stops). Retained existing progress IDs and source links.
- Moved vehicle/property, cheat, safe code, secret mission, NFC link and Regional Dispatch catalogs to readable TSV assets. Build temporary row indexes on demand; edit catalog TSVs directly without conversion tools.
- Load collection row offsets and IDs on demand, cache four visible labels, and read full details only when needed. Retain existing progress IDs, checkmarks, source links and menu order.
- Allocate collection, secret mission, submenu and text views when needed; release inactive views, text and catalog buffers.
- Keep NFC and mock loading plugins on demand; release detail text and URL data before NFC initialization and restore the previous detail or link selector on return.
- Added opt-in RAM diagnostics for heap, memory-pool block size and stack watermark.
- Added a Right Emulate action to linked item details; moved Dispatch Next to Left and preserved OK checkmarks. Embedded both FALs in the Companion FAP and resolve them through app-relative asset paths.
- Fixed NFC URL emulation using the firmware's native NTAG215 generator and listener initialization, added magenta emulation LED feedback, and reduced temporary NFC memory allocations.
- Use native tag accessors and specific initialization diagnostics for the embedded NFC module.
- Expanded Cocaine Flamingos to 91 reported locations and search leads, with 73 individually linked NFC map locations. Added five Regional Dispatch entries and prioritized direct GTA6GPS and GTADB map pins over shared collection guides.
- Expanded Cocaine Flamingos to 93 reported locations and search leads, integrated all 42 currently listed GTA6GPS pickup-related flamingo map pins, added 53 individual NFC map-pin mappings, expanded 175 individual source references and updated 70 collection-level map references.
- Added 50 direct GTA6GPS NFC map links across 41 Standout Vehicles, five Meteorite Chunks and three Special Weapons, including alternate vehicle locations. Expanded source references for three existing Cocaine Flamingo entries.
- Expanded Regional Dispatch to 919 indexed stops.
- Added individual and alternate NFC map links for Standout Vehicles, Special Weapons, Meteorite Chunks, publications, garages, Wyman wrecks, Kenton dive sites, all seven book swaps and selected story safehouses. Prioritized exact pickup pins over shared maps.
- Kept Special Weapons first and alphabetized the remaining top-level collection menus without changing progress IDs or Dispatch source indexes.
- Added 44 individual GTA6GPS NFC source pins covering all seven Moses vehicle pickups, nine Randy vehicle requests, 26 gang operations, the Star Spangled 556 (listed as Stars and Stripes 556 on the map), and the alternate Key Lento helicopter-wreck meteorite location. Preserved existing collection IDs and map-image links.
- Added 59 individual GTA6GPS NFC map pins for all 14 Small Bank Jobs, 5 Special Weapons, one Delights Dancer card, 18 Jack of Hearts Dancer cards and 21 Nine1Nine Dancer cards. Retained existing progress IDs and direct map-image links.

## v1.9.8 — Weapon and Meteorite Expansion
- Expanded Special Weapons to 137 guide-reported pickups, variants and leads; added 20 named entries including Convict Kalinga .50, Mister Mangrove Laird 350 and additional character-associated weapons. Refined existing acquisition clues and mission missable warnings.
- Expanded Meteorite Chunks to 82 reported locations; added Capri fallen trunk, Wetman Bay carcass and Port Gellhorn bones. Added approximate map landmarks to existing locations and retained overlapping reports as alternate clues.
- Split Special Weapons into three sublists and Meteorite Chunks into two sublists using the existing asset loader; preserved existing checkmarks, progress IDs, menu order and save format.
- Expanded Cocaine Flamingos to 72 reported pickups, including Catalan Key, Southwind, Vice City, Port Gellhorn and Mount Kalaga; refined existing landmarks and alternate map reports. Added nine new and three previously unindexed flamingos to Regional Dispatch.
- Added two tentative Shoeless Keys flamingo map leads (74 reported pickups and leads total); refined seven existing search descriptions and indexed both new leads in Regional Dispatch.
- Expanded Cocaine Flamingos to 80 reported pickups and map leads, including six new tentative landmark-relative search areas; refined 18 existing descriptions using filmed routes and screenshot-based guides. Split the catalog into three regional sublists, retained all existing IDs and indexed the new leads in Regional Dispatch.
- Refined 13 Cocaine Flamingo search descriptions, including Dorado Pawn Shop, Riquezas building B-207, Southwind shore camp and Meerwunder. Retained 80 reported pickups and leads and all existing checkmarks.
- Added two separately mapped Key Lento shoreline search leads (82 reported pickups and leads total), with GTADB marker references and overlapping beach/yard clues. Added both to Regional Dispatch and preserved existing checkmarks.
- Added hold-OK NFC URL links for supported collection and Dispatch details, with Source/Map selection and temporary NDEF URL emulation. Preserved short-OK checkmarks and existing save IDs; Back stops NFC. Shared map indexes are distinguished from individual markers.
- Refined hold-OK NFC link selection: Source on Left when available; direct map image on Right, or Source 2 when no image is available. Retained interactive maps as source references and removed duplicate destinations.
- Hold OK now starts NFC URL emulation immediately when only one link option is available; two options retain the selector. Back stops NFC and returns to the item or selector accordingly.
- Added a fullscreen GTA-themed NFC emulation graphic with a bottom URL bar and automatic horizontal scrolling for long links.
- Expanded Cocaine Flamingos to 86 reported pickups and map leads with four separately indexed Key Lento and Watson Bay search points; distinct counter credit remains unconfirmed. Added all four to Regional Dispatch.
- Added shared source guide/index fallbacks for every Collections checklist and vehicle/property list, retaining individual URLs where available and preserving automatic single-link NFC emulation.
- Added individual GTA6 GPS NFC map pins to ten Standout Vehicles, eight Cocaine Flamingo entries, the La Noche wreck in both Kenton and Underwater Discoveries, and the SOS Island meteorite. Added a shared Wyman wreck map; retained individual guides where available and preserved progress IDs.
- Split NFC URL emulation into an on-demand GTA NFC Link FAL with fullscreen URL artwork and scrolling. Added a shared GTA loading FAL for randomized mock-loader transitions in both directions using the existing set preferences. Companion remains open; Back stops NFC and unloads the plugin, returning to the original item or link selector, and Hold Back exits to firmware.

## v1.9.7 — Collection and Secret Mission Updates
- Added nine guide-reported Cocaine Flamingo search areas (57 total), including Hamlet, Port Gellhorn, Buckskin Swamp and the coast south of Waning Sands; refined ten existing entries with street, building and alternate map clues. Updated Regional Dispatch.
- Added two reported meteorite sites (72 total) at Hyde Island and Featherstone; expanded existing Key Lento and Mount Kalaga directions.
- Added Island Poacher FF-50 as an individual Special Weapons pickup (64 total); expanded the existing Hyde Island Manhunt with alternate entry, retry, gear retrieval and reported reward.
- Added Royal Ruby Emerus gate-release instructions and clarified the Palmer warehouse gold KZ-59 pickup and safe clue.
- Refined the Backrooms access and navigation, clarified active-character stamina and tattoo reward, and expanded Kyle chemical delivery options and Bret deer hunting instructions.
- Preserved existing progress IDs, cheat inputs, menu order and save format.
- Added 42 guide-reported Special Weapons entries (106 total), including 23 store-specific variants, hidden pickups, edition rewards and the missable Don Gone revolver. Identified the existing bunker Type 9 as Conspiracist Type 9; expanded paired weapon locations.
- Expanded Gold Scarab Windsor specifications and two existing vehicle price/access descriptions; documented the shorter reported Attack Force alternative without replacing the primary code.
- Expanded Andrew Node's Thrax with the Chapter 3 No Flusher headquarters clue route, conflicting keypad reports, and mansion access notes; retained ID 212 and all 15 scan IDs. Added the early phone-clipping garage entry method, normal registration and reported free Chapter 2 Moses tutorial registration; noted patch sensitivity.
- Added 11 Legendary Animals entries covering nine Snapmatic hunts and two rare creatures; clarified the separate eight-animal challenge.
- Added Brainwasher 1922 (107 Special Weapons), Brainwashed, plumber hats, Haymarket clown and Andrew Node walkthroughs (35 Secret Missions); added three access/puzzle credentials and eight variable bank-code examples (63 Safe & Door Codes). Refined diving controls and Avery Pawn alternatives.
- Added five guide-reported Special Weapons (112 total), including Gun Nut SMS-01, two Chapter 3 missable pistols, Caminos Signature K17 and Mahi Nipper .38; added Royal Bay Marina variable safe clue (64 codes) and Marbelle Hotel fog walkthrough (36 Secret Missions). Expanded existing card, publication, bottle, watch and underwater directions; added Chapter 2 missable and free-registration tips.
- Expanded Chatfield 5800, Quarter Mile ES9, Purple Drank Emerus and spy-vessel pistol instructions; added Pocketblast DSG (113 Special Weapons), three mapped meteorite search areas (75 total), a four-Up Body Armor alternate report, and more flamingo, diving, shooting-range and hunting details.
- Added the reported Rodgers Ranch Market Range Beginner quit-and-repeat method for permanent Focus leveling, with instructions in Quick guide / warnings, Shooting Gold Medals and the Unlimited Focus cheat description.
- Added six map-reported Cocaine Flamingo locations (63 total), four meteorite search areas (79 total), and four named Special Weapons (117 total). Added Misericordia game-map coordinates, refined the first two Bret decks, and preserved existing progress IDs and published cheat inputs.

## v1.9.6 — Vehicles and Collection Updates
- Added four reported safe/door combinations (Chatfield 5800, Biker Hideout 1982, Stormy 7869, Big Day Motel 4242); documented Cottonmouth 44017 versus shortened 4017 and Palmer 1886/1887 discrepancies, with existing collection IDs and progress unchanged. Enriched eight Flamingo routes, Bret hunting conditions, and linked gang-operation, weapon and card locations.
- Enriched existing collectible-card, publication, meditation, meteorite and Challenge entries with detailed pickup routes, alternate-copy handling and Master shooting-range requirements.
- Corrected Collections progress total to include Piccia Furia (ID 28), which was already tracked but omitted from the summary count; existing checkmarks and IDs are unchanged.
- Added room-by-room Luxury Watch and gang-operation access routes, and cross-linked publication/card pickups at shared locations.
- Improved Selkie Hole and Belville meteorite directions, and clarified Del Ocio's detector search and ten-doubloon clearing requirement without adding speculative wrecks.
- Added collection-wide completion totals and percentage to the end of Quick guide / warnings, using saved checklist progress.
- Added a checklist of all **80 named Challenge objectives**, grouped by game category with individual goals; retained the existing 19 challenge milestones and their saved checkmarks.
- Added the first three location-documented **Bret Target Decks** with target species, hunting areas and carcass-quality tips; retained Bret's existing optional-mission entry and documented the remaining unmapped decks as incomplete research.
- Corrected the No Flusher Tags collection total to **15**, matching the 15 existing WAiNK scan records without duplicating scans.
- Added Lucia's **Buzz Cut** magazine-style pickup in Hamlet to the existing Lucia styles catalog, using its own progress ID.
- Expanded the Bret Vogel mission's in-app guidance and updated the README to distinguish fully enumerated objectives from partially mapped discoveries.
- Added all remaining twelve numbered Bret Target Deck checklists (15 total), preserving the first three named contracts and avoiding unsupported animal/spawn claims.
- Added separate Watson Bay Cocaine Flamingos in the table-top room and candlelit east-side building (43 mapped pickups total).
- Expanded all six existing Underwater Discoveries with wreck-specific relic or doubloon search directions, without changing wreck IDs or adding unverified discoveries.
- Located all six Cinco Torres treasures and completed the individual La Noche, Santa Sierra and Lujuriosa search notes under the original Kenton sets.
- Added Del Ocio search areas and clarified that its ten doubloons may be recovered in fewer digs; the story wreck remains separate from Kenton's seven sets.
- Clarified timed, weapon, location and completion conditions for selected entries in the existing 80 Challenge objectives; retained all 19 milestone checkmarks.
- Added 19 individually tracked Special Weapons variants, including the San4San and Marabunta Grande KZ-59 rifles, bringing the catalog to **62**; expanded gang-operation pickup directions and distinguished weapons from nearby cash safes.
- Added Cigar Import Front door code `1426` (**51** code entries), expanded safe landmarks and Pelican Valley loot details, and clarified variable Avery Pawn and alien-lab codes plus robbery Focus Mode instructions.
- Updated collection completion totals and README counts while retaining existing checklist IDs.
- Added 15 Secret Missions and Easter egg walkthroughs (31 total), including the Backrooms, Backyard Wrestling, full Vice City Portal switch order, Burg Industries and the Key Lento rainy-day fund.
- Added two reported underwater discoveries, two meteorite pickups and the Belpoint lighthouse flamingo; added safe code 80085 and expanded Kenton and Bret directions.
- Added reported resale and registration prices to 20 Standout Vehicles and an alternate Revenant Gauntlet search area.
- Added Videotape and Dark Mirror filter inputs, corroborated by PC Gamer and GamesRadar+ (21 published inputs); retained Pinhole and screenshot reports.
- Added Cursed Knife (63 special weapons), lighthouse ghost and SOS Island walkthroughs, two hidden trophies, Tenebrae Lux puzzle instructions, and expanded Backrooms, wrestling, portal and Bret hunting directions.
- Added the Royal Sanchez Standout (281 vehicle entries); expanded container routes for the Cuban Hermes, Blue Drake Penumbra and Caliente Picador.
- Added four reported Cocaine Flamingo sites (48 total), ten meteorite search locations (70 total), alternate shoreline/lawn clues and additional Burg Industries directions; expanded Regional Dispatch and collection totals.
- Clarified Bret carcass delivery and the reported Fair Game panther encounter; documented three conflicting Dexerto controller sequences without replacing the corroborated inputs.

## v1.9.5 — Fences
- Added seven individually checkable Fences under Collections, immediately before Endings, sorted by chapter unlock: Brian, Donnie, Moses, Dunk, Tyrone, Yasmani and Randy.
- Added unlock missions, selling locations, goods accepted, story-related availability and delayed/direct payment details.
- Identified Tyrone as the best reported narcotics buyer and second-best jewelry/marked-bills buyer; Yasmani as best for jewelry and marked bills and second-best for narcotics.
- Distinguished Moses' untracked vehicle purchases from Randy's premium Wish List jobs, without inventing comparable payout percentages.
- Added a Fences section to the README and retained saved checklist IDs and the Endings-last menu order.
- Expanded the existing Annis Pixel Elegy entry with its container-entry route; added the Maibatsu Sunset Sanchez as a distinct Standout Vehicle.
- Added four named luxury watches with hideout pickup directions; expanded the existing Ashore and Ambassador entries with additional reported locations.
- Added 15 geographically distinct, guide-reported Meteorite Chunk locations to the existing meteorite checklist, with separate progress markers.
- Added the Goose Key Mile Zero shoreline flamingo and expanded six existing Gang Operations with specific watch and key-valuable directions.
- Expanded existing Gang Operations with guide-reported unique weapons, rare vehicles, watch cross-references and safe-access clues; no duplicate operation IDs or new menus.
- Enriched existing Standout Vehicle entries with reported unlock, lock/alarm, associated hideout and transport details, without creating extra vehicle checkmarks.
- Clarified existing Orca and Ashore watch locations and cross-linked four Underwater Discoveries to their existing Kenton treasure routes, preserving the original saved IDs.
- Preserved existing collection IDs, previous release notes, and menu ordering.
- Added **Annis Yago's Elegy** as one distinct Street Racer Standout (ID 3603), acquired during the Chapter 3 Yago finale; not a duplicate Akane, Pixel, Drifter or Chameleon Elegy. Standout count: 262.
- Expanded four existing Yago mission entries with unlock sequence, Jason-only requirements, race time limit, shop build/nitrous instructions, reward, and finale location; retained all existing mission IDs.
- Refined all five Wyman restoration entries with map landmarks and payment flow; clarified Chapter 3 cut-off tool gate for the Reynolds Kit Car and corrected final Deviant payment from $1,500 to $1,000.
- Improved the existing anime-wrap Akane location/security and livery-preservation warning; enriched the existing GTA+ Crosstown garage with bundled Venturas BLVD Elegy, weapon locker and fencing access.
- Updated README vehicle totals, guide and research links.
- Identified the existing Chapter 1 impound Phoenix as **Blazin' Phoenix** without changing its saved progress ID (176).
- Added 15 individually named Standout vehicles/rewards absent from the existing 262-entry catalog (now 277), including story, Ultimate Edition, GTA+, and rare bikes/aircraft; marked undocumented pickup sites rather than inventing directions.
- Expanded existing Gold Rush Tempesta, Venomous Marshall, Erotic Journey, Pink Licorice Astron and Valiant Tempesta records with reported alternate pickup clues and access caveats.
- Cross-linked Wyman restored vehicles to their existing salvage tasks and the GTA+ / Paradise garages to their respective unique vehicles.
- Updated the Infinite Money Glitch screen to prioritize the Apollo Aggregates **cars-and-cocaine loop**: registered 10-slot van, **registered 6-slot car**, warehouse loot/vehicle storage, and character-switch warehouse refresh.
- Documented the two-vehicle swap at Dunk's trailer as an **unverified community report**, including the alleged $13,200 payout, actual payment verification, and possible visual-only duplication.
- Retained the owned-vehicle manual save/reload fallback, fence unlock/rate guidance, backup-save warnings and Chapter 1 money-loss caution; revised the README and sources without changing other release history.
- Added three missing named Standout Vehicles to the existing catalog: Street Art Squalo (Key Lento), Muckspreader Bandito (Hamlet), and Cellerica Caddy (Belville); total now 280.
- Expanded eight existing vehicle entries with precise acquisition, access, alarm and tracker information, including the Fruit Outlet Walton, Mountain Yosemite, Speedy Boomazoom, Westbound Azure, Brimstone Golfer, Rumpo, Taci Ragno and Searing Zombie Bobber.
- Improved the existing Andy Peach! mission with the Cellerica Caddy pickup, added a Hamlet alternate copy for Bombazo!, and detailed the Couch Potato Mustang .357 hanging-couch pickup.
- Retained earlier car and collection descriptions, alternate locations, saved progress IDs, Fences, Endings-last ordering, and all earlier changelog entries.

## v1.9.4 - Expanded Locations and Collectibles
- Added 26 separately checkable, guide-reported Meteorite Chunk locations; marked the total and completion status as unconfirmed.
- Added a five-stage Missing Persons walkthrough, distinguishing the warehouse and mine finale from the three reported missing tourists.
- Added 22 basketball hoop guide-number locations with ball pickup instructions, bringing the documented location records from 51 to 73; marked legacy crosswalk conflicts for continued review.
- Expanded the collection progress bitmap from 2,048 to 4,096 IDs. Added redundant `progress2.a` / `progress2.b` saves and read-only migration from `progress.a` / `progress.b` to retain existing checkmarks.
- Updated the README with new checklists, diving/quest references, safe migration instructions, and unresolved WAiNK/flamingo research. Kept home menu, existing IDs, and separate Secret Missions saves unchanged.
- Added 55 individually checkable Standout Vehicle entries with reported locations, including rare cars, motorcycles, an aircraft, and mission rewards.
- Added vehicle acquisition notes for shared spawns, story prerequisites, garage access, and Key Cloner requirements.
- Added two reported garage combinations: Red Apple Orchard (`360782`) for the Applesauce Bobcat, and Southside Storage (`1010`) for the mural-reward Cheetah.
- Added 46 additional Standout Vehicles, bringing the catalog to **260 vehicles**.
- Expanded vehicle locations with additional garages, residential properties, racetracks, storage containers, mission rewards, and unique spawn points.
- Improved vehicle acquisition instructions, including chapter requirements, Key Cloner access, special conditions, and alternate spawn locations.
- Added 12 Meteorite Chunk locations, bringing the catalog to **43 locations**.
- Added 5 Cocaine Flamingo locations, bringing the catalog to **40 locations**.
- Added 3 Luxury Watch locations, bringing the catalog to **9 locations**.
- Added 17 Location Search Leads for additional discoveries, including the Underwater Hotel and locations requiring further verification.
- Expanded Kenton's Treasures with improved underwater directions for four shipwreck locations.
- Updated Dispatch references and location details for additional collectible and vehicle discoveries.
- Improved collection descriptions with additional landmarks, access instructions, and search areas.
- Distinguished uncertain and potentially duplicate locations from established collectible entries.
- Updated the README to reflect expanded collections, vehicle information, and location guidance.

## v1.9.3
- Added 15 new collections
- Expanded on 4 existing collections

## v1.9.2
- Added 12 Special Weapons
- Added 8 Optional Missions
- Added 6 Messages in Bottles
- Expanded on publications collection, cards, safe codes & special weapons

## v1.9.1
- Expanded on safe codes and stand out car info

## v1.9 — Endings checklist
- Added Collections > Endings with 12 independently checkable finale routes and epilogue variants, including descriptions of choices, preparation, and reported consequences.
- Clearly distinguish nine reported outcomes/variants from three proposed but unverified savings-based epilogues; include a story-spoiler warning in the guide.
- Endings use persistent collection checkmarks and stable IDs, without changing the save format, Dispatch, Secret Missions, or the home-screen controller.
- Documented ending prerequisites and source references in the README, with an explicit explanation of differing published ending counts.

## v1.8 - Special Weapons and Code Navigation
- Added two Black Eagle warehouse combinations (789987 office, 031462 safe) with Kalinga .50 access instructions.
- Added four reported codes for the Marbelle Hotel and linked Presidential Bunker / IAA Black Site discoveries.
- Safe & Door Codes can now be browsed by area or by type; code text preserves leading zeros.
- Special Weapons now has 31 independently checkable pickups, including codes in the detail for every known locked weapon route.
- Kept Secret Missions, Dispatch, existing collection IDs, and SD progress data formats unchanged.

## v1.7 — Secret Missions and Completion Tracking
- Confirmed the existing shared checklist supports individual completed/uncompleted marks for Under the Bridge crossings, all stunt categories, Randy/Moses/Wyman/Standout vehicles, and populated Collections, using Right in the list or Check/Undo from details.
- Clarified those controls in the in-app Quick guide and README; preserved shared progress files and original IDs.
- Secret Missions now includes independently saved completion checkmarks (Right from list, OK Check/Uncheck from details) and a completion count.
- Mission progress uses separate redundant SD save files keyed to fixed mission IDs, unaffected by chronological menu sorting.
- Grouped the 16 home icons into gameplay, collection/vehicle, reference, and display tools; Infinite Money Glitch remains first.
- Ordered Secret Missions by reported story access, with Chapter 1 missables prioritized and uncertain chapter requirements last.
- Aligned the README feature list and secret walkthrough index with the actual menus.
- Added a **Secret Missions** home-menu icon and a chronological, story-progress-ordered guide to 16 hidden activities, weapon pickups, cash routes, and Easter eggs.
- Each secret includes earliest reported availability, specific location clues, steps, rewards, caveats, and named sources.
- Start-of-game and missable Chapter 1 finds are listed before Chapter 2 events; uncertain chapter gates are flagged rather than guessed.
- Added Hyde Island's Island Poacher FF-50 Manhunt guide. Silencer/attachment not established by the sources.
- Added reported Satan ritual, Hellspawn shotgun reward, and an explicit note that an extra-health reward is not substantiated.
- Added the UFO-to-alien-rescue route, which published walkthroughs associate with permanent Health/Stamina/Focus increases.
- Added The Final Flush stamina reward, hidden radios, a tower egg, Chapter 1 cash routes and weapons.
- Secret Mission checkmarks use independent save slots. Other checklists continue using the existing shared progress system, including bridges, stunts, vehicles, Collections and Dispatch.

## v1.6 — Location Enrichment
- Added reported landmarks for six Randy targets that previously lacked locations; added the final Truffade Thrax mission as new Randy ID 34, without changing existing IDs.
- Improved 12 Standout Vehicle location descriptions and retained uncertainty/conflict labels for uncorroborated claims.
- Added three previously absent Standout Vehicle variants (IDs 230-232): Driveshaft Hustler, Spiderweb Coquette and Seashark Sedna (Graffiti). Different variants and multiple spawn markers were not counted as new vehicles.
- Improved existing Capri flamingo directions and marked ambiguous map-only reports without changing the 30 known item IDs.
- Replaced four county-only Peewees billboard records with reported highways, intersections and landmarks; added the reported final prize route to Peewees in Belville; preserved their existing checklist IDs.
- Preserved existing Dispatch targets and all stored progress IDs. New Randy ID 34 is not yet a Dispatch stop; do not claim full new-stop dispatch parity until its mapping is integrated.

## v1.5
- Reordered safe-list labels to show combination first so long location names do not hide it.
- Added **Safe & Door Codes** immediately after Infinite Money Glitch on the icon home menu, with its own vault icon.
- Cataloged 38 published safe, mission, operation, keypad and puzzle combinations (including 4-, 5-, and 6-digit codes and two passwords). Locations, clues, chapters, and evidence-status details are browsable entirely offline.
- Added category filters and a code-first menu row for quick lookup; Back returns to the previous filter and retains the selected row.
- Flagged variable, less-supported and contradictory reports rather than presenting every combination as certain. Specifically documents Kingpin Bowling (0300 vs 0330 in sources) and Saber Shipping Facility (1122 vs 1965).
- Added a guide to randomized robbery safes, leading zeroes, employee clues and alternate opening methods. Updated the Quick Guide to mention the catalog.

## v1.4
- Added an Infinite Money Glitch guide to the first home-menu slot, with a dollar-sign icon and five steps for the reported Story Mode warehouse reset.

## v1.3
- Added checklists for all 397 stunts, including 130 Stunt Jumps, 202 Under the Bridge crossings, 40 Knife Flights, and 25 Base Jumps. Jumps and bridges are grouped by county. Entries include location clues, saved checkmarks, and reported bonus goals or par times. The game requires any 300 stunts for 100% completion.
- Added John Morgan (Dusty Pink), completing the 480-card catalog across 18 sets. Casanova Island is the reported area. The exact pickup spot is unconfirmed.
- Added flamingo locations in Capri and Key Lento and the Backyard Wrestling Belt and Luchadora tattoos in Hamlet. The app now lists 30 flamingo locations and 21 Discovered Styles. Dispatch now covers 839 stops.

## v1.2
- Expanded the card checklists to 479 of 480 cards with reported location clues. Added all 112 individual AHOL Street Art locations with progress linked between pieces and their types.
- Expanded coverage to 28 flamingo locations and 19 Discovered Styles. Added alternate pickup spots and chapter availability for reading items where reported, and refined WAiNK location clues.
- Added the single-source Pinhole cheat under Filters with a report label. The **Published inputs** menu still lists 19 corroborated inputs per platform.
- Expanded location-based Dispatch to 835 stops.

## v1.1
- Added region selection in Dispatch, with All regions and six named regions plus Other areas. Dispatch now covers 425 vehicle deliveries and collection stops, with checkmarks shared across menus.
- Filled the last three missing location clues among the 190 cards then listed. Refined WAiNK clues and added the Crowex Ambassador 'Breakfast' watch, bringing Discovered Styles coverage to 16 entries.

## v1.0
- Expanded the GTA VI loader set to 30 images. With the 15 Dungeons & Dolphins images, Mock Loaders includes 45 full-screen images.
- Added a randomized startup splash that displays an image from the enabled sets for half a second. Disabling both sets opens the main menu directly.
- Switched the displayed version to the manifest's compiled `FAP_VERSION` and removed legacy checklist save support in favor of the current format.

## v0.5
- Added Collections with 18 card set menus and 190 documented cards, plus WAiNK locations, AHOL street art types, 1010 Murals, Kenton's Treasures, underwater discoveries, reading items, book swaps, meditations, flamingos, playlists, and Discovered Styles.
- Replaced the main menu list with icons and a text line showing the selected item.
- Added the Mock Loaders gallery with 15 Dungeons & Dolphins images and eight GTA VI images. Mock Loader Settings saves an independent switch for each set. GTA VI starts enabled by default.

## v0.4
- Renamed GTA VI Cheats to GTA VI Companion and kept the PS5 and Xbox cheat menus.
- Added Randy's Wish List, Moses Drop-Offs, Wyman's Classic Cars, and Standout Vehicles, with location details and one-press checkmarks saved to the SD card.
- Added checklists for 15 garages and four story safehouses. Details include location, price or unlock, and storage capacity or facilities where available.
- Added Dispatch to cycle through unfinished Randy and Moses deliveries with known locations and share progress with their checklists.

## v0.3.1
- Added the Xbox D-pad sequence for Drunk and updated the PS5 entry with corroborating reports. This brought the published catalog to 19 inputs per platform.

## v0.3
- Expanded the catalog to 61 reported cheat effects with unlock requirements. Added the initial PS5 report for Drunk and displayed unavailable inputs as `[?]`.
- Preserved all 28 screenshot sequences as unverified. Listed 24 under Unverified codes and placed the four overlapping sequences in their matching cheat details.
- Fixed button labels in sequences so inputs such as R1 and R2 stay distinct.

## v0.2
- Added 18 published Story Mode D-pad sequences per platform, with effects, unlock requirements, sources, and cheat categories.
- Added the Quick guide / warnings screen with entry instructions and a note about saving and achievements. Moved the catalog into dedicated source files during the v0.2 updates.

## v0.1
- Introduced GTA VI Cheats as an offline Flipper Zero reference with separate PS5 and Xbox Series X|S screens. The initial build shipped before the published cheat catalog was added.
