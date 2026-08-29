Dungeons & Dolphins SD-card catalogs

Copy this catalogs folder to:
/ext/apps_assets/dndolphins/catalogs/

Each file normally uses one option per line. Blank lines and lines beginning with # are ignored.
Dungeons & Dolphins streams the selected catalog directly and does not load a second overlay.
Append additional names or annotated records to the appropriate normal file. The app stores only the
selected name in the character save; long descriptions and restricted add-on content are not bundled.

Short OK on an item, spell, feature/feat, class, subclass, or Background opens its catalog.
Hold OK on that name to enter fully custom text instead.

Background names are kept separately in backgrounds.txt. The selected
background is stored in the active character's own ch_{id}_{name}_{level}.txt save.

Spell lines may optionally provide filtering metadata:
Spell Name|level|Class1,Class2

Annotated spells appear by default only when the spell's assigned source class can cast that
spell level and its class is listed. Plain spell names remain supported in the All view.
Spell lines may use Spell|Level|Class, Class metadata. Hold OK in the spell catalog to toggle Allowed/All.
Subclass lines use Subclass|Parent Class. The default view shows only the selected class; hold OK for All.
Item lines use Name|Category|Rarity|Source. Supported categories include Weapon, Armor, Gear, Tool,
Mount/Vehicle, Potion, Ring, Rod, Scroll, Staff, Wand, and Wondrous.
