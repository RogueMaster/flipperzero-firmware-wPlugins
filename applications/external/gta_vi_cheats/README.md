# GTA VI Companion

Unofficial, offline GTA VI Story Mode reference and collection tracker for Flipper Zero.
Open **GTA VI Companion** from **Tools**.

## Main menu

Use the **D-pad** to move between icons. The top line shows the selected item's
name. Press **OK** to open it and **Back** to return to the same icon.

| Menu option | Description |
| --- | --- |
| PS5 Cheats | Browse reported PS5 inputs, unlock requirements, and unverified codes. |
| Xbox Cheats | Browse reported Xbox inputs, unlock requirements, and unverified codes. |
| Collections | Find Story Mode collectibles and discoveries, view location clues, and track completed entries. |
| Randy's Wish List | Track Randy's vehicle deliveries and see reported spawn locations and times where available. |
| Moses Drop-Offs | Find Moses's Chapter 2 vehicle and follow-up targets, then check off each delivery. |
| Wyman's Classic Cars | Track Ultimate Edition restoration projects and find the wrecks and final pickup. |
| Standout Vehicles | Find documented spawn locations for distinctive vehicles and mark each one acquired. |
| Garages | Track acquired garages and look up their locations, prices or unlocks, and storage capacity. |
| Houses / Cribs | Track story safehouses and see their locations, chapter unlocks, and facilities. |
| Dispatch | Cycle through location tips for unfinished Randy and Moses deliveries and mark targets complete. |
| Quick guide / warnings | Review checklist controls, cheat entry steps, and warnings about saving and trophies or achievements. |
| Mock Loaders | Browse 15 full-screen Dungeons & Dolphins loading images. Press **Left** for the previous image, **Right** for the next, or **Back** for the menu. |

## Checklist controls

Press **OK** on a row to open its location and notes. Press **Right** in the list or **OK**
in the details to toggle its checkmark. **Back** from the details returns to the selected
row. The list header shows the number checked. The app saves progress at
`/ext/apps_data/gta_vi_cheats/` on the SD card as each checkmark changes.

In **Dispatch**, press **Right** for the next tip or **OK** to check or undo the current
target. Dispatch and the two delivery checklists share the same progress. Consult the
in-game mission marker if a reported spawn is unavailable.

## Collections

Choose a category or set to open its checklist. The app loads one list at a time from
the files bundled in the FAP.

| Collection | Included entries |
| --- | --- |
| Collectible Cards | All 18 set menus and 190 documented cards. 187 have text location clues. The in-game catalog contains 480. |
| WAiNK Locations | 31 landmarks, 35 curiosities, 23 sculptures, 15 mapped No Flusher tags, and the four Peewees Billboard counties. Published No Flusher totals differ. |
| AHOL Street Art | 11 type checklists covering the game's 112 pieces, with starter clues where documented. Check a type when you finish its pieces. |
| 1010 Murals | All five photo markers, with the landmark to frame from each marker. |
| Kenton's Treasures | The seven sets after the tutorial wreck, with dive areas and piece counts. |
| Underwater Discoveries | Six named wrecks from the current guides, including the tutorial wreck. The game tracks 33 discoveries. |
| Magazines, Books & Comics | 26 documented titles and one location clue for each. |
| Book Swaps | All seven Little Library stops, with nearby streets or landmarks. |
| Meditations | All 18 completions at 10 spots. Shared spots have separate Jason and Lucia rows. |
| Cocaine Flamingos | 22 documented locations out of the game's 125. |
| Unique Playlists | Both playlists and all five poster locations. |
| Discovered Styles | 15 documented entries across Jason's styles, Lucia's styles, tattoos, and luxury watches. |

The list counter tracks entries included in the app. When a confirmed game total
exceeds the included list, the details show the catalog coverage.

### Exploration sources

- [Story Mode collection totals and links to individual guides](https://www.powerpyx.com/gta-6-collectible-guide-progress-database/)
- [Card set checklists and mapped variants](https://game-checklists.com/gta6/card-sets-checklist/)
- [Collectible marker locations, including cards, murals, books and magazines](https://gtagameintel.com/en/interactive-map/?location=community-1090704)
- [WAiNK landmarks](https://www.gtabase.com/gta-6/map/?feature=waink-landmark), [curiosities](https://www.gtasixmap.com/guides/waink-curiosities), and [sculptures](https://www.gtasixmap.com/guides/waink-sculptures)
- [No Flusher tags and published count differences](https://gta6-life.com/en/guides/gta-6-graffiti-tags/)
- [Peewees Billboard counties](https://www.gta6everything.com/activities/peewees-billboards)
- [AHOL street art starter locations](https://finalboss.io/gta-6-ahol-street-art-murals)
- [Kenton's sets and treasure counts](https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures)
- [Underwater discoveries](https://game-checklists.com/gta6/underwater-discoveries-checklist/)
- [Cocaine Flamingo locations](https://game-checklists.com/gta6/cocaine-flamingos-checklist/)
- [Discovered Styles locations](https://game-checklists.com/gta6/styles-checklist/)

Keep the numeric IDs in `assets/collections/*.tsv` attached to their entries
when editing or sorting the data. The checkmarks use those IDs.

## Vehicle and property checklists

| Checklist | Entries | Details |
| --- | ---: | --- |
| Randy's Wish List | 18 | Named delivery targets, with published locations and in-game times where available. The in-game Robberies list supplies current tips. |
| Moses Drop-Offs | 7 | Includes the Chapter 2 story target and six follow-up deliveries. |
| Wyman's Classic Cars | 5 | Ultimate Edition. Locations for all 15 wrecks and the final '72 Deviant pickup. |
| Standout Vehicles | 102 | Named vehicles from a 103-pin map snapshot. One vehicle has two documented locations. Entries with a published area only are labeled. |
| Garages | 15 | Eight purchasable garages, four story rewards, and three edition or GTA+ benefits. Open a garage for the price or unlock, district, and capacity. |
| Houses / Cribs | 4 | Story safehouses with their chapter unlock, location, and facilities. Check off each home as you reach it. |

### Vehicle and property sources

- [Moses locations and screenshots](https://www.pushsquare.com/guides/gta-6-moses-vehicle-drop-off-locations)
- [Moses follow-up deliveries](https://www.powerpyx.com/gta-6-steal-vehicles-for-moses-walkthrough/)
- [Randy's Wish List and Supply & Demand](https://www.powerpyx.com/gta-6-grand-theft-auto-vi-trophy-guide-roadmap/)
- [Randy spawn reports](https://www.pushsquare.com/guides/gta-6-randys-wish-list-vehicle-locations)
- [More Randy spawn reports](https://www.radiotimes.com/technology/gaming/gta-6-randys-wishlist-cars/)
- [Randy's map pins](https://gta6map.io/location/randy-wish-list)
- [Rockstar interview on vehicle tips](https://www.techradar.com/gaming/gta-6-senior-design-director-confirms-even-when-missions-show-a-level-of-completion-locations-can-still-be-hit-again-and-again-tackling-them-in-different-ways-each-time)
- [Wyman's restoration projects](https://thegameswiki.com/gta6/wiki/wymans-cars)
- [Standout map pins](https://www.gtabase.com/gta-6/map/?feature=standout-vehicle)
- [Standout landmarks](https://gta6map.io/location/standout-vehicle)
- [Garage prices, capacity and unlocks](https://www.gtabase.com/gta-6/articles/how-to-unlock-garages-in-gta-6)
- [Garage and safehouse locations](https://leonidawaypoint.com/database/properties)
- [Story safehouses and facilities](https://outputlag.com/guides/gta-6-safehouses/)

## Cheat usage

Choose **PS5 Cheats** or **Xbox Cheats**, then pick **Published inputs**, **All GTA VI entries**,
a cheat category, **Unverified codes (24)**, or **Quick guide / warnings**.
For main-catalog cheats, open **Progress > Cheats** in game to inspect unlock
requirements.

In free roam, open the phone with D-pad Up, then hold L1 + R1 (PS5) or LB + RB
(Xbox), and input the D-pad directions. Game guides report that active cheats
disable saving and trophies or achievements for the session. Save first.
Cheats are for Story Mode.

## Cheat sources

- [PowerPyx GTA VI cheats](https://www.powerpyx.com/gta-6-all-cheats/)
- [GamesRadar+ GTA VI cheats](https://www.gamesradar.com/games/grand-theft-auto/gta-6-cheats/)
- [PC Gamer GTA VI cheats](https://www.pcgamer.com/games/grand-theft-auto/gta-6-cheats-list/)
- [TrueAchievements GTA VI cheats](https://www.trueachievements.com/news/gta-6-cheat-codes)
- [GTA6DB live source log](https://gta6db.io/cheats) (historical source log)
- [PC Gamer GTA San Andreas cheats](https://www.pcgamer.com/games/grand-theft-auto/san-andreas-cheats/)
