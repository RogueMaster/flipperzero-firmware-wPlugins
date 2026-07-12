# Flipcraft

Flipcraft is a tiny first-person voxel sandbox for the Flipper Zero — a
pocket "Minecraft" rendered in true 3D on the 128x64 1-bit screen. You walk
through a procedurally generated world, chop trees, dig for coal and iron,
smelt ingots and glass in a furnace, craft tools and weapons at a crafting
table, build with placed blocks, stash loot in chests, tame a wolf and try
to survive creepers and your own dynamite.

Worlds are generated from a text seed in four sizes, from 128x128 up to
1024x1024 blocks, with forests, deserts, ravines and ore veins; the same
seed produces the same terrain at any size. Everything persists on the SD
card — the world itself, your inventory, health, and the contents of every
chest and furnace — and any number of worlds can be created, renamed and
deleted from the menu.

## Features

- Real-time 3D first-person renderer, 17 block types
- Seed-based world generation: forests, deserts, ravines, coal and iron ore
- 3x3 crafting: pickaxes, axes, shovels and swords in wood / stone / iron
  tiers, plus shears, crafting table, furnace, chest and dynamite
- Furnace smelting: iron ingots and glass, fueled by coal or wood
- Creatures: sheep, tameable wolves and exploding creepers
- 8 hearts of health, fall damage, apples to heal, respawn on death
- Chest storage, world saves in `.fcw` files on the SD card

## Donation

| Service | Remark | QR Code | Link |
|---|---|---|---|
| <img src="https://cdn.simpleicons.org/boosty" alt="Boosty" width="14"/> **Boosty** | Support the project | <div align="center"><a href="https://boosty.to/apfxtech/donate"><img src="https://api.qrserver.com/v1/create-qr-code/?size=180x180&data=https%3A%2F%2Fboosty.to%2Fapfxtech%2Fdonate" alt="Boosty donation QR code" width="140"/></a></div> | [boosty.to/apfxtech/donate](https://boosty.to/apfxtech/donate) |
| <img src="https://raw.githubusercontent.com/gist/PonomareVlad/55c8708f11702b4df629ae61129a9895/raw/1657350724dab66f2ad68ea034c480a2df2a1dfd/YooMoney.svg" alt="YooMoney" width="14"/> **YooMoney** | RU payments only | <div align="center"><a href="https://yoomoney.ru/fundraise/1IV33POM6H4.260711"><img src="https://api.qrserver.com/v1/create-qr-code/?size=180x180&data=https%3A%2F%2Fyoomoney.ru%2Ffundraise%2F1IV33POM6H4.260711" alt="YooMoney donation QR code" width="140"/></a></div> | [yoomoney.ru/fundraise/1IV33POM6H4.260711](https://yoomoney.ru/fundraise/1IV33POM6H4.260711) |


## Screenshots

<p align="center">
  <img src=".catalog/screenshots/gameplay.png" width="49%"/>
  <img src=".catalog/screenshots/inventory.png" width="49%"/>
</p>
<p align="center">
  <img src=".catalog/screenshots/crafting.png" width="49%"/>
  <img src=".catalog/screenshots/crafting_full.png" width="49%"/>
</p>
<p align="center">
  <img src=".catalog/screenshots/furnace.png" width="49%"/>
  <img src=".catalog/screenshots/death.png" width="49%"/>
</p>
<p align="center">
  <img src=".catalog/screenshots/menu_main.png" width="49%"/>
  <img src=".catalog/screenshots/menu_worlds.png" width="49%"/>
</p>
<p align="center">
  <img src=".catalog/screenshots/world_seed.png" width="49%"/>
  <img src=".catalog/screenshots/world_size.png" width="49%"/>
</p>
<p align="center">
  <img src=".catalog/screenshots/world_generating.png" width="49%"/>
</p>

## Controls

### World

- `Up` / `Down`: move forward / backward
- `Left` / `Right`: turn left / right
- `Ok` + `Up` / `Down`: look up / down
- `Ok` + `Left` / `Right`: select previous / next hotbar slot
- `Ok` short press: place block, use the targeted station, or attack the creature in front of you
- `Ok` long press: mine or break the targeted block; on a wild wolf: tame it (costs 2 apples)
- `Back` short press: jump
- `Back` long press: open inventory

Aiming at creatures is forgiving: any creature in the middle half of the
screen counts as targeted, no pixel-perfect crosshair needed. Blocks still
require the exact crosshair.

### Menus

- `Up` / `Down` / `Left` / `Right`: move cursor
- `Ok`: pick up, place, or take output
- `Ok` + arrow: drop one held item per slot crossed
- `Back`: close menu

### System

- `Ok` + `Back`: quit (bug press first "OK")

## Creatures

Three species roam the world. Sheep are common; wolves and creepers are
equally rare. Every creature has 2 HP: any sword kills in one hit, bare
hands or tools in two. A hurt creature flashes inverted for about a second.
Creatures are not saved with the world.

- **Sheep** (passive). Grazes, flees when hit. Drops an apple (meat).
- **Wolf** (neutral). Leaves you alone until you hit it, then it bites back
  (1 heart per bite). Wolves hunt sheep and creepers on their own: against a
  creeper a wolf bites and springs away, but sometimes it lingers too long
  playing and the blast takes both - the duel is roughly 50/50. Drops an
  apple (meat).
- **Creeper** (hostile). Stalks you from up to 6 blocks. Next to its target
  it freezes, flashes and swells for ~2 seconds, then explodes: a 3x3x3
  crater, chests and furnaces are destroyed with their contents, and anyone
  within 2.5 blocks - you included - loses ~90% of max health. Run more
  than 4 blocks away to disarm it. Drops gunpowder on **any** death,
  exploded or slain.

### Taming

Hold `Ok` on a wild wolf while carrying at least 2 apples: the apples are
consumed and the wolf becomes yours. A tamed wolf never bites you, follows
you everywhere and guards you: it still pounces on sheep and creepers by
itself, but never strays further than 3 blocks from you.

## Dynamite

Craft: 1 sand + 1 gunpowder side by side (any order, horizontal or
vertical) = 1 dynamite. Stacks like any material.

- Place it like a normal block; it is harmless until lit.
- `Ok` short press on a placed dynamite lights the fuse: it flashes for
  about 3 seconds (falls if unsupported), then explodes exactly like a
  creeper.
- The explosion **ignites any adjacent dynamite** with a short random
  delay - line up charges for a chain reaction.
- `Ok` long press breaks an unlit dynamite back into the item.