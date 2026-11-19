# GTA VI Cheats v0.3 - Flipper Zero / RM 420

Unofficial, offline Story Mode cheat reference for PlayStation 5 and Xbox Series X|S.
Version 0.3 builds directly on the published RM 0.2 app. It retains `cheat_catalog.c` / `.h`, the
Flipper GUI, application ID, author, Tools category, and original `icon.png` (unchanged).

## Catalog and evidence - checked November 19, 2026

- 61 *reported* GTA VI cheat effects: Player 10, Combat 11, Vehicles 21, Environment 3, Filters 16.
- The existing 18 published PS5/Xbox D-pad inputs from v0.2 are preserved without changes.
- New single-source PS5 report: Drunk = `Up Left Right Up Up Left Right Up Left Right`.
  Source: Polygon as recorded at GTA6DB. NOT independently verified; Xbox input not populated.
- The other 42 effects have no published input recorded here. Their individual unlock
  requirements are nevertheless browsable, and a missing input displays `[?]`.
- All **28 button sequences** shown in the supplied image are now transcribed in C
  and the CSV audit. The **24 screenshot-only effects** appear under **Unverified codes
  (24)**, and each displays its controller sequence rather than just a name.
- Four effects match the 61-entry catalog: **Infinite Ammo**, **Raise Wanted Level**,
  **Spawn Sanchez** (Maibatsu Sanchez), **Change Weather** (Cycle Weather). Those are
  not duplicated in the Unverified list. Their screenshot inputs are shown as
  **image alternates** inside the corresponding main catalog details, alongside any
  existing GTA VI D-pad sequence. This does NOT validate the image's alternate input.
- PS screenshot tokens are preserved verbatim in the CSV. On Xbox the app maps PS
  physical button labels to Xbox equivalents (R1=RB, R2=RT, L1=LB, L2=LT,
  Triangle=Y, Circle=B, Square=X). These Xbox sequences are DERIVED; the image did
  not provide Xbox codes. The image contains combinations reminiscent of earlier GTA
  titles, so they must not be presented as console-tested GTA VI codes.
- Fixed sequence rendering: controller button numbers are no longer discarded
  (formerly R1 and R2 both appeared as R; L1 and L2 both appeared as L).

**Published** is not the same as console-tested or Rockstar-verified. This FAP has
not been used to validate the reported codes in-game.

## Usage

Choose your console, then pick **Published inputs**, **All GTA VI entries**,
a category (showing published/total), **Unverified codes (24)**, or **Quick guide**.
For main-catalog cheats, open **Progress > Cheats** to inspect unlock requirements.
The screenshot-derived sequences have not been verified for GTA VI and may have
incompatible input methods. Do not treat them as equivalents to published D-pad codes.
In free roam, open the phone with D-pad Up, then hold L1 + R1 (PS5) or LB + RB
(Xbox), and input the D-pad directions. Press reporting says active cheats stop
saving and trophies/achievements in that session. Save first. Story Mode only.

## Sources

- [PowerPyx GTA VI cheats](https://www.powerpyx.com/gta-6-all-cheats/)
- [GamesRadar+ GTA VI cheats](https://www.gamesradar.com/games/grand-theft-auto/gta-6-cheats/)
- [PC Gamer GTA VI cheats](https://www.pcgamer.com/games/grand-theft-auto/gta-6-cheats-list/)
- [GTA6DB live source log](https://gta6db.io/cheats) (single-source Polygon report)
- [PC Gamer GTA San Andreas cheats](https://www.pcgamer.com/games/grand-theft-auto/san-andreas-cheats/)

Source discrepancies from v0.2 are **preserved**:
* **Max Ammo**: PowerPyx/GamesRadar+ say 25 bronze medals; PC Gamer says 20.
* **Cycle Weather**: PowerPyx says finish story; GamesRadar+ says reveal map.
* **MTL Pumper**: PowerPyx does not specify unlock; other guides mention discovering it.

For discrepancies, check the unlock requirement in your in-game Progress menu.
