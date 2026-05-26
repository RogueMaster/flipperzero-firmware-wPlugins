# FlipRogue v1.2 Magic Tables

These tables match the current v1.2 implementation in `src/`.

Unknown potions use generated color labels such as `Red Potion`. Unknown
scrolls appear in inventory as `Scroll RUNE RUNE`, while pickup logs keep only
the runes, for example `Picked VEX MUR.` Unknown Charms use one generated rune
word such as `Charm VEX`. Mages know all scroll names immediately.

## Potions

Identical potions stack as `Name (N)` and consume one copy per use. Each potion
copy still counts toward the 20-item pack limit. Potions offer `Quaff`,
`Throw`, and `Drop`.

| ID | Known name | Quaff log | Quaff effect | Throw effect |
|---:|---|---|---|---|
| 1 | Healing Potion | `Warmth knits flesh.` | Heals 8 HP, capped at max HP. | Heals the target 8 HP. |
| 2 | Strength Potion | `Iron wakes in you.` | Permanent +1 STR. | Restores 1 target HP if below max. |
| 3 | Antidote Potion | `Venom loosens.` | Clears `POISONED`. | Clears `POISONED` and `SLOWED` from the target. |
| 4 | Fire Ward Potion | `Your skin cools.` | Clears `BURNING` and grants a short Ward timer that prevents new fire ignition. | Clears `BURNING` from the target and extinguishes terrain fire in a 3x3 area. |
| 5 | Quickness Potion | `Your feet spark.` | Clears `SLOWED`. | Clears `POISONED` and `SLOWED` from the target. |
| 6 | Venom Potion | `Bitter roots bloom.` | Applies `POISONED` for 12 turns. | Applies `POISONED` for 12 turns. |
| 7 | Flame Potion | `Fire blooms.` / `Fire hisses.` | 3x3 fire burst centered on the player. | 3x3 fire burst at the target tile; overlapping fire fields merge, refresh, and expand through normal fire spread math. |
| 8 | Frost Potion | `Time turns thick.` | Applies `SLOWED` for 10 turns. | Applies `SLOWED` to a target, extinguishes fire in 3x3, and freezes nearby water/puddles into ice. |
| 9 | Blindness Potion | `The dark leans in.` | Applies `BLIND` for 8 turns; FOV shrinks. | Applies `BLIND` for 8 turns. |
| 10 | Smoke Potion | `Smoke folds you.` | Applies `CONFUSED` for 8 turns and `BLIND` for 4 turns. | Applies `CONFUSED` for 8 turns and `BLIND` for 4 turns. |

Throwing most non-Flame potions at an empty tile identifies it, consumes it, and
logs `Potion shatters.` Fire Ward and Frost also affect empty terrain. Throwing
a potion while engulfed by a Cube is blocked with `Cube holds you.`

## Scrolls

Identical scrolls stack as `Name (N)`, consume one copy per read, drop one copy
per drop, and count each copy toward the 20-item pack limit. Targeting text
intentionally avoids revealing unknown scroll identity: wands say `Choose where
to ZAP.`, target scrolls say `Choose target.`

| ID | Known name | Log | Effect |
|---:|---|---|---|
| 1 | Identify Scroll | `One name settles.` / `Nothing happens.` | Identifies one carried unknown potion, scroll, or Charm. The UI can also target a specific unknown item. If nothing can be identified, the scroll is not spent or identified. |
| 2 | Enchant Weapon | `Weapon +.` / `Nothing happens.` | Raises the class weapon by +1, capped at +3. Ranger upgrades bow, Mage upgrades staff, Warrior upgrades sword. |
| 3 | Enchant Armor | `Armor +.` / `Nothing happens.` | Raises body armor by +1, capped at +3. |
| 4 | Fire Bloom | `Fire blooms.` / `Fire hisses.` | Targeted 3x3 fire burst. |
| 5 | Blink Scroll | `Blink.` / `Nothing happens.` | Moves the player toward the target ray. Empty targets are used directly; actors can be crossed but not landed on; walls, statues, closed grates, and unopened chests stop the blink at the last empty tile before them. Targeting yourself or having no empty landing tile does not spend or identify the scroll. |
| 6 | Mapping Scroll | `Map wakes.` | Marks the whole current floor explored and reveals hidden doors, traps, and hidden actors on that floor. |
| 7 | Fear Scroll | `Fear.` / `Nothing happens.` | Applies `AFRAID` to the targeted monster for 8 turns. Empty targets do not spend or identify the scroll. |
| 8 | Charge Scroll | `Wand charged.` / `Charge.` | Fills one carried wand to max. If no wand needs charging, gives the Mage staff 2 charges. |
| 9 | Phase Scroll | `World shifts.` / `Nothing happens.` | Moves the player by a 120-step bounded drift that usually prefers tiles farther from the start, with a little chaotic motion. The path can pass through monsters but lands on the last empty non-start tile; passing through a hidden door reveals it as an open door. |
| 10 | Reveal Secrets | `Secrets wake.` / `Nothing happens.` | Reveals hidden doors, traps, and hidden actors within radius 8 of the player. |
| 11 | Decurse Scroll | `Curse cracks.` / `Nothing happens.` | Clears one cursed Charm flag. |
| 12 | Call Scroll | `All hear you.` | Wakes all active monsters toward the player. |

## Wands

Wands are rare, hold current/max charges, and do not disappear at 0 charges.
Empty wands print `No charges.` and can still be dropped or recharged. Inventory
labels look like `Wand Fire (1/3)`. Blink and Reveal wands have 2 max charges;
all other wands have 3.

| ID | Name | Log | Effect |
|---:|---|---|---|
| 1 | Wand Fire | `Fire bolt.` | Projectile damage 2; surviving target burns for 8 turns. |
| 2 | Wand Frost | `Frost ray.` | Target gets `SLOWED` for 10 turns. |
| 3 | Wand Force | `Pushed.` / `Force snaps.` | Pushes the target 1 tile away from the player; blocked impact deals 1 burst damage. |
| 4 | Wand Blink | `Blink wand.` | Uses the same target-ray landing rules as Blink Scroll. Invalid targets still spend a charge. |
| 5 | Wand Venom | `Venom ray.` | Target gets `POISONED` for 8 turns. |
| 6 | Wand Fear | `Fear ray.` | Target gets `AFRAID` for 8 turns. |
| 7 | Wand Arc | `Arc.` | Projectile damage 2, then 1 burst splash around the target tile. |
| 8 | Wand Reveal | `Revealed.` / `Nothing.` | Reveals hidden doors, traps, and hidden actors within radius 2 of the target tile. |

Skeleton Archers have a 50% dodge chance against projectile and thrown damage,
but not against burst damage. Logs use the short name `S.Archer`; monster cards
still show `Skeleton Archer`.

## Throwables

| Item | Stack | Damage | Notes |
|---|---:|---:|---|
| Stones | up to 20 | 1-2 | One stack counts as one inventory item. Ranger `Dart Hand` adds +1 damage. |
| Darts | up to 20 | 3-5 | Ranger `Dart Hand` adds +1 damage. |

Throwables offer `Throw` and `Drop`. If the player is engulfed by a Cube,
throwing is blocked with `Cube holds you.`

## Class Perks

Perk choices appear at levels 3 and 6. The player can pick from five class-local
perks; learned perks are marked and cannot be picked again.

### Warrior

Warriors are built around reliable survival and close combat. They start with
the most HP, a shield, strong melee damage from STR, and perks that reward
holding ground when the dungeon presses in.

| Perk | Menu text | Effect |
|---|---|---|
| Iron Heart | `+20% max HP` | Immediately increases max HP by roughly 20%, minimum +1, and heals the same amount. |
| Shield Bash | `melee bash chance` | Each melee hit has a 10% chance to double that hit's damage and push the target 1 tile if it survives. |
| Riposte | `counter after hit` | After a monster hits the player in melee, 10% chance to deal 1 melee damage back. |
| Cleave | `kill splash` | When a Warrior kills a monster in melee, nearby monsters around the killed target take 1 burst damage. |
| Hold Line | `block when swarmed` | If at least two active monsters are adjacent, player block gets +1. |

### Ranger

Rangers play around distance, awareness, and controlled movement. Their high
DEX makes them excellent at finding hidden trouble, they see traps outright,
and their arrows turn straight corridors into tactical lanes.

| Perk | Menu text | Effect |
|---|---|---|
| Pin Shot | `shots push` | After a ranged shot hits a still-active target, 10% chance to push it 1 tile along the shot direction. |
| Hamstring | `shots slow` | After a ranged shot hits a still-active target, 10% chance to apply `SLOWED` for 6 turns. |
| Trap Eye | `better searching` | Immediately grants +1 DEX, improving search and trap detection. |
| Vantage | `+far shot dmg` | Ranger shots at distance 4 or farther deal +1 damage. |
| Dart Hand | `+throw dmg` | Thrown stones and darts deal +1 damage. |

### Mage

Mages lean on knowledge and limited magical force. They know scroll names from
the start, fight with staff charges instead of arrows, and use perks to add
volatile effects or recover a little magic by resting.

| Perk | Menu text | Effect |
|---|---|---|
| Overcharge | `crit staff burst` | Each staff shot has a 10% chance to deal double damage and log `Overcharge.` |
| Ember Staff | `staff may burn` | After a staff shot hits a still-active target, 10% chance to apply `BURNING` for 5 turns. |
| Frost Rune | `staff may slow` | After a staff shot hits a still-active target, 10% chance to apply `SLOWED` for 6 turns. |
| Quartz Mind | `rest recharges` | Immediately grants +2 staff charges; resting can add +1 charge on every fourth turn, capped at 9. |
| Veil | `magic hides you` | Currently reserved by the perk flag; no active gameplay hook is implemented yet. |

## Charms

One Charm can be equipped at a time. Wearing a Charm identifies it; Identify
Scrolls can also identify Charms. Cinder, Glass, and Hungry Charms start cursed
and cannot be removed until a Decurse Scroll clears the curse.

| ID | Charm | Effect |
|---:|---|---|
| 1 | Dew Charm | Grass dew is twice as likely: 1-in-5 instead of 1-in-10. |
| 2 | Ash Bead | Fire-contact damage to the player is capped at 1, and `BURNING` can never be newly applied while worn. |
| 3 | Scout Loop | +25 hidden trap/search chance. |
| 4 | Fang Charm | 5% chance to scare a melee attacker for 5 turns. |
| 5 | Quartz Ring | Reserved charge-luck hook; currently no active effect. |
| 6 | Empty Locket | Hunger ticks slightly slower. |
| 7 | Cinder Charm | Cursed: every 10 turns hurts the player for 1-2 HP if above 1 HP, burns adjacent monsters for 1 burst damage, and logs `Cinder bites.` |
| 8 | Glass Charm | Cursed: sight can peek one tile past the first wall, closed door, or other sight-blocking terrain. |
| 9 | Hungry Charm | Cursed: hunger ticks slightly faster. |

## Monsters

Floor placement is randomized by 3-floor tiers. Dragonlings enter the final
floor tier as a rare replacement for part of the late Skeleton Archer slot and
can appear in packs of 1-4.

| ID | Glyph | Monster | HP | Damage | Placement | Traits |
|---:|:---:|---|---:|---:|---|---|
| 1 | `r` | Rat | 3 | 1 | Floors 1-3 pool; sometimes starts asleep. | 35% chance to dodge player melee, projectile, and thrown hits. |
| 2 | `b` | Bat | 4 | 1 | Floors 1-12 pool; can pack on floors 7-12; sometimes starts asleep. | Erratic movement, blink-strike near the player, and 10% chance to heal 1 HP on hit if wounded. |
| 3 | `s` | Snake | 3 | 2 | Floors 4-6 pool; sometimes starts asleep. | 5% chance to poison the player for 8 turns on hit. |
| 4 | `g` | Goblin | 6 | 3 | Floors 4-9 pool. | Always aggressive; drops 3 gold score on kill instead of 1. |
| 5 | `a` | Skeleton Archer | 5 | 2 | Floors 7-12 pool; rarer in floors 16-18. | Shoots along visible lines, can kite at melee range, and dodges projectile/thrown hits 50% of the time. |
| 6 | `S` | Slime | 10 | 2 | Floors 10-12 pool; often starts aggressive. | Splits after surviving direct non-burst, non-DOT damage, preserving pack identity. |
| 7 | `w` | Wisp | 3 | 2 | Floors 7-9 and 13-15 pools. | 15% chance to blind the player for 5 turns on hit. |
| 8 | `k` | Kobold | 5 | 2 | Floors 1-6 pool; can form packs. | Pack pressure; otherwise plain melee. |
| 9 | `o` | Ogre | 14 | 5 | Floors 13-18 pool. | 15% chance to stun the player for 2 turns on hit. |
| 10 | `C` | Cube | 10 | 2 | Floors 13-18 pool. | Full-health Cube has a 20% adjacent-turn chance to engulf the player, pinning movement and blocking throws. |
| 11 | `D` | Dragonling | 22 | 6 | Rare floors 16-18 pool; pack size 1-4. | 25% chance to fire-burst the player from range 3 on a straight line. |
| 12 | `W` | Yonder Warden | 199 | 14 | Spawns after taking the Orb of Yonder. | Always hunts, remembers forever, follows floors, moves every other turn when close and every turn when far. |
| 13 | `G` | Wight | 8 | 3 | Floors 10-18 pool. | Starts hidden; 5% chance to inflict Fear for 6 turns on hit. |
| 14 | `e` | Eel | 5 | 3 | Flooded pools; 50% of deep water bodies, packs of 3-4. | Water-only pressure monster; uses deep water and shallow borders. |
| 15 | `M` | Mimic | 14 | 6 | Rare chest mimic. | Reveals on bump/open, removes the fake chest, and attacks immediately. Flavor: `Box with opinions.` |
| 16 | `L` | Lurker | 6 | 3 | Rare grass or wet-edge decorator. | Starts hidden; search can reveal it. Flavor: `Waited all day. For this.` |

Lone or last-pack monsters below 30% HP have a 15% chance to break and flee for
6 turns. The Yonder Warden never breaks.

## Other Effects

| Source | Log | Effect |
|---|---|---|
| Fire burst | `Fire blooms.` / `Fire hisses.` | 3x3 burst, deals 1-3 fire damage to player/monsters, applies burning for 5 turns unless resisted, tramples grass, hisses against puddles/water/sand/ice/chests/shrines, and refreshes finite fire fields. |
| Arrow trap | `Arrow trap.` | Hidden step trap with a wall source; fires a projectile line and damages the player. |
| Fire trap | `Oil jar bursts.` | Hidden step trap with a wall source; fires a projectile, applies a 3x3 burst, and ignites finite fire terrain cells. |
| Poison trap | `Poison trap.` | Player gets `POISONED` for 12 turns. |
| Snare trap | `Snare locks.` | Player gets `STUNNED` paralysis for 3 visible world ticks; the world keeps advancing with short UI delays. |
| Puddle | `Splash.` | Clears player `BURNING`. Generated puddles spread into small random patches of 1-5 adjacent cells. Frost can turn puddles into ice. |
| Deep water | `Deep water.` | Clears burning and makes one player swim move advance the world twice. Land monsters avoid it; eels do not. |
| Ice | `Ice.` / `Ice slide.` | Walkable frozen water. Entering ice has a 30% chance to slide the mover along the entry direction to the last connected ice cell. Eels die when their water freezes. |
| Chest | `Chest opens.` / `Pack full.` / `Empty chest.` | Blocking choose-one object until opened. Opened chests stay visible but become inert and non-blocking. Full pack leaves the chest unopened. Chests do not burn. |
| Sand | `Sand whispers.` | Sparse dust patch; blocks fire spread. |
| Mimic chest | `Oh no. Mimic!` | Reveals a Mimic, removes the fake chest, skips the chest menu, and attacks once immediately. |
| Grate key/button | `Grate opens.` | Opens all same-floor grates and persists as terrain deltas. |
| Old god statue | old-god log line | Solid, non-burning object. Bumping it can reveal nearby secrets or heal 1 HP; otherwise gives a short lore beat. |
| Food ration | `You eat.` | Consumes 1 ration, restores +180 hunger and +3 HP. |
| Grass dew | `Dew +1 HP.` | Trampling tall grass can auto-pick dew for +1 HP. |
| Orb of Yonder | `Orb wakes.` | Spawns the Yonder Warden and slows hunger/starvation clocks 5x. |

## Terrain and Decorators

| Feature | Presentation | Notes |
|---|---|---|
| Walls | Small wall sprites | Walls no longer rely on `#` as their primary visual. |
| Grate | `#` / grate sprite | Visible gated reward pocket; blocks movement/pathing until same-floor key or button opens it. |
| Puddle / shallow water | Sparse static dot pattern | Extinguishes burning and borders deep water pools. |
| Deep water | Dense slow animated dot pattern | Swim cost is two world turns; eels may pack inside. |
| Fire field | Sparse animated dot overlay | Fixed-capacity finite terrain effect; no dedicated fire terrain glyph. |
| Ice | Diagonal scratch sprite / `_` fallback | Frost-created terrain; walkable, visible, and slippery. |
| Sand | Sparse dust sprite | Generated in small patches and blocks fire spread. |
| Shrine | Statue sprite / `T` fallback | Solid old god statue with short bump interaction text. |
| Chest | Chest sprite / `H` fallback | Blocking choose-one item popup; opened chest stays visible but inert; rare Mimic variant skips the menu and attacks. |

Deferred ideas remain out of scope for v1.2: gas propagation, chasms/falling,
floating item water physics, and merchants.
