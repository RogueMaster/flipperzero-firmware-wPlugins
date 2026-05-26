# How to Play FlipRogue

The Dungeons of Yonder are a dangerous place for a pocket-sized adventurer.
Every run is short: go down, take the Orb of Yonder, and climb back to daylight.
The screen is drawn with tiny symbols, but each symbol is just a thing in the
world: you, walls, doors, items, monsters, and danger.

## Goal

Start on floor 1 and descend to floor 18. Find and take the Orb of Yonder. The
Yonder Warden wakes when the Orb is taken, then hunts across floors. Return to
floor 1 and use the up stairs to win.

Gold is your score. Surviving with the Orb is better than dying rich, but dying
rich still leaves a nicer high score.

## Controls

| Input | Action |
|---|---|
| D-pad | Move, attack, or line-shot as Ranger/Mage |
| Hold D-pad | Repeat movement |
| OK | Rest, use stairs/buttons, or interact with nearby objects |
| OK long | Look mode |
| Back | Game menu |
| Left/Right in inventory | Switch item tabs |

Movement is 4-directional. Most attacks are also 4-directional. Bats are the
main exception: after a blink they can strike from a diagonal tile.

## Symbols

| Symbol | Meaning |
|---|---|
| `@` | You |
| block tile | Wall |
| `#` | Visible grate |
| `.` | Floor |
| `+` | Closed door |
| `-` | Open door |
| `<` / `>` | Up / down stairs |
| `!` | Potion |
| `?` | Scroll |
| `/` | Wand |
| `:` | Stones |
| `;` | Darts |
| `=` | Charm |
| `%` | Food |
| `*` | Gold |
| `"` | Tall grass |
| `H` | Chest |
| `T` | Old god statue |
| `K` | Key |
| `&` | Orb of Yonder |
| letters | Monsters |

Water, ice, and fire are drawn as tiny pixel patterns rather than as one text
glyph: shallow water is sparse, deep water is denser and animated, ice is
scratched, and fire flickers as a sparse overlay.

## Classes

| Class | Style |
|---|---|
| Warrior | Toughest start, shield, better at standing ground. |
| Ranger | Uses arrows for straight-line shots and sees traps early. |
| Mage | Uses staff charges for straight-line magic and knows scroll names. |

All classes gain level bonuses automatically. At levels 3 and 6 you also choose
one class perk.

## Turns and Combat

Bumping into a monster attacks it. Ranger and Mage shoot instead of walking when
they press toward a visible monster in a straight line and have the needed
resource.

Monsters act after your turn. Alert monsters remember where they saw you. Some
monsters sleep, wander, flee when badly hurt, or wake their pack when attacked.

Resting can recover HP slowly, but it also gives monsters time to move.

## Terrain

Doors open when stepped through and close when nobody is nearby. Tall grass
blocks sight until trampled and can release healing dew. Puddles and shallow
water edges extinguish burning without slowing you. Deep water extinguishes too,
but it is slow: one swim move gives the world two turns. Land monsters avoid
deep water, but eels live there and tend to gather in small packs. Frost magic
can freeze water into slippery ice; eels caught in freezing water do not enjoy
the experiment. Sand appears in small dusty patches and blocks fire spread.

Some reward pockets sit behind visible grates. A grate blocks movement until a
same-floor key is picked up or a same-floor button is pressed. Grates are not
secret: if you can see one, the dungeon is making a little promise.

Old god statues are solid. Bump one to make it answer with a short line; it may
reveal a nearby secret or patch you up a little.

Hidden doors and traps can be revealed by searching. Ranger is best at spotting
traps. Bumping into a hidden door reveals it.

## Inventory and Items

Your pack holds 20 item copies. Potions and scrolls stack as inventory rows, but
each copy still counts toward that limit. Stones and Darts stack as one slot.

| Item | Uses |
|---|---|
| Potions | Quaff, throw, or drop. Unknown potions use color labels. |
| Scrolls | Read or drop. Unknown scrolls use rune labels. |
| Wands | Zap or drop. Empty wands stay in the pack and can be recharged. |
| Stones/Darts | Throw or drop. Darts hit harder; Ranger can improve them. |
| Charms | Wear, take off, or drop. One Charm can be worn at a time. |
| Food | Eat from the menu to restore hunger and a little HP. |
| Chests | Bump or press OK next to `H`, then choose one item. If your pack is full, the chest stays. |

Identify Scrolls reveal one unknown potion, scroll, or Charm. Wearing a Charm
also reveals its name.

## Magic

Potions and scrolls can heal, enchant gear, reveal secrets, blink, phase, burn,
slow, poison, blind, confuse, scare, or call monsters. Bad magic is not just flavor:
blindness shrinks your view, slow gives monsters more pressure, confusion
scrambles movement, and fear makes monsters retreat.

Blink magic moves toward the chosen target and settles on the last safe empty
tile before a hard obstacle or occupied target. Phase magic lets the world pick
a strange route that usually drifts away from where you stood; it can pass
through monsters, but it will not leave you inside one.

Fire effects use a 3x3 burst. Fire deals 1-3 damage, can burn actors, trample grass, refresh
temporary fire fields, and hiss out against puddles, deep water, ice, sand, or
chests. Fire Ward prevents new burning for a while; Ash Bead keeps fire contact
to 1 damage and prevents new burning while worn. Frost can douse nearby fire and
freeze water.
Arrow traps shoot from a wall source. Fire traps throw an oil jar and burst
where it lands. Snare traps paralyze you for a few world ticks while the dungeon
keeps moving.

## Monsters

| Monster | What to Know |
|---|---|
| Rat | Weak but slippery. |
| Bat | Erratic, blink-strikes, sometimes heals on hit. |
| Snake | Can poison. |
| Kobold | Small pack pressure. |
| Goblin | Armed and worth extra gold. |
| Skeleton Archer | Shoots, kites, and dodges thrown/projectile hits. |
| Slime | Splits when it survives direct damage. |
| Wight | Hidden until revealed or until it attacks. |
| Wisp | Can blind. |
| Ogre | Heavy melee, can stun. |
| Cube | Can engulf you and block throwing. |
| Dragonling | Rare deep pack monster with short-range fire bursts. |
| Eel | Water-only pack pest. Swim carefully. |
| Mimic | A chest with opinions. Reveals and bites when bumped or opened. |
| Lurker | Hidden in grass or wet edges; search can reveal it. |
| Yonder Warden | Wakes after the Orb and keeps hunting. |

## Hunger

Runs start with food rations. Hunger eventually weakens you; starvation damages
you over time and blocks waiting. Food restores hunger and a little HP.

Carrying the Orb slows hunger and starvation clocks, giving the return trip room
to breathe without making it safe.
