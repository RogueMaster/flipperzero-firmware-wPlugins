# FlipRogue Project Notes

## Future Backlog After Module Extraction

The old `src/game_logic_parts/*.inc` layer is gone. These ideas are still parked
because they touch memory, generation, rendering, and balance at the same time;
they should land as deliberate slices rather than as opportunistic content.

### Hazard Simulation

`src/hazards.[ch]` can grow from instant bursts/traps into a small terrain-effect simulation:

- spreading fire and gas with per-cell tick counters, decay, and simple extinguish rules;
- an alternative state-machine model inspired by small cellular automata, but bounded and deterministic enough for Flipper RAM;
- reusable update hooks for hazards that run once per player turn rather than per-render frame;
- clear interactions with puddles, grass, traps, doors, and actor/player status effects.

Rendering needs a tiny-screen-first design before implementation. Options to prototype:

- ASCII glyphs for stable readability, such as `*`, `~`, `^`, or `"` depending on effect type;
- pixel-grid overlays made from pseudo-random symbols for unstable fire/gas shimmer;
- small sprite-like patterns, such as 3-4 horizontal wave rows over puddles, instead of single-character terrain.

Ice is a separate hazard candidate: stepping onto ice slides the player several cells in the chosen direction, stopping at the end of the ice run or a blocking tile. Optional trap placement can sit adjacent to the ice endpoint rather than being hard-coded into the ice tile.

### Static Memory And Search Scratch

Before adding spreading hazards or heavier monster behavior, audit heap allocation and recursion use. The expected hot spots are pathing/BFS, random reachable tile selection, teleport destination search, wandering target search, and any future propagation simulation.

Preferred direction:

- no runtime heap use in tactical loops;
- static arenas for reusable path/search scratch;
- intrusive queue/graph node storage for BFS;
- bitsets for visited/open/closed cells;
- one shared scratch contract reused by monsters, teleport, map generation checks, and hazard propagation;
- explicit failure behavior when scratch space is exhausted.

### Static Memory Audit 2026-05-23

Current audit status before implementing arenas:

- Gameplay code does not actively allocate heap in tactical loops. Plain `malloc/free` only appears in
  `src/fliprogue_app.c` for app lifetime objects: `AppContext`, `FrGame`, the input queue, mutex,
  viewport, GUI record, and notification record.
- No direct recursion was found in `src/*.c` by a direct self-call scan. Current pathing, wandering,
  teleport, search, and generation logic are iterative.
- `FrGame` is large by design: about `28160` bytes on the host compiler. Most of that is compact
  floor persistence: `FrFloorState` is about `1358` bytes and `floors[18]` is about `24444` bytes.
  The app keeps `FrGame` on heap; host tests often keep it on stack, which is fine for tests but not
  representative of Flipper runtime stack usage.
- Current pathing scratch is already static in `src/pathing.c`, not stack-local:
  `path_queue[64*32]` is about `4096` bytes, `path_seen[64*32]` about `2048` bytes, and
  `path_first_dir[64*32]` about `2048` bytes. This is roughly `8 KB` of `.bss` dedicated to BFS-ish
  work. It is stable, but not yet a shared scratch API.
- A frame-size warning pass with `-Wframe-larger-than=256` found two gameplay frames worth watching:
  `fr_generate_floor` around `752` bytes on host, and `fr_set_game_over` around `352` bytes on host.
  `fr_generate_floor` mainly carries room-generation locals such as `FrRoom rooms[7]`; `fr_set_game_over`
  carries several `FR_LOG_SIZE` temporary buffers. Both are below the current Flipper app stack size
  of `2 KB`, but they are good cleanup candidates before adding heavier systems.
- Several small static string buffers exist for labels and summaries, for example item labels,
  run summary, status/effects labels, and look hints. They avoid stack churn and heap use, but they are
  single-result helpers and should not be treated as reentrant APIs.

Future implementation target:

- introduce one `scratch.[ch]` or `search_scratch.[ch]` module with a fixed static arena for map-sized
  work;
- replace byte-per-cell visited arrays with bitsets where practical;
- expose narrow operations such as reset, push/pop queue node, mark/test cell, and optional
  first-direction storage;
- keep failure explicit: if scratch is already in use or capacity is exceeded, callers should degrade
  to a bounded local behavior rather than corrupting shared state;
- migrate pathing first, then random reachable tile / blink / teleport / roaming, and only then future
  fire/gas propagation.

### Ambushes And Economy

Small content slices to consider after the architecture is calmer:

- lurkers hidden in grass, very rare, about 2-3 meaningful appearances per run;
- mimics disguised as valuable items, also rare and biased toward treasure-looking pickups;
- chests with a compact choose-what-to-take menu, not automatic bulk pickup;
- primitive merchants with a tiny inventory and simple prices, preferably one clear screen rather than a full shop simulator.

## v1.2 Notes

v1.2 is a stability and release-polish update after the first public-ready
v1.1 line.

- Saved floor state is more compact: saved actors/items are packed, explored
  memory is stored as a 2x2 coarse bitset, tile deltas are capped at 48, and
  the pathfinding queue is smaller with a safe overflow failure.
- Coarse explored restore avoids marking extra wall tiles as remembered, so
  returning to an upper floor should not show doubled walls outside vision.
- Low-HP LED feedback now resets RGB before setting its warning color and uses
  a slower red/dark-red pulse instead of leaking the previous green state.
- The title screen, manifest, README, changelog, and public tables identify the
  app as v1.2.

## v1.1 Notes

v1.1 is the first public-ready release line. The game is still a pocket dungeon,
but the world now has enough persistence and texture to feel like a complete
run rather than a prototype.

- The title screen, manifest, and README identify the app as v1.1.
- High Scores are persistent on SD at `/ext/apps_data/fliprogue/hiscores.txt`.
  Orb runs sort above non-Orb runs, then gold decides the table.
- `Sound: On/Off` persists as `/ext/apps_data/fliprogue/settings.txt` with
  `sound=1` or `sound=0`.
- Build documentation now points to UFBT and keeps both direct `python3 build.py`
  commands and Makefile aliases.
- The public docs emphasize the current pocket RPG contract: room decorators,
  water/ice/fire, grates, chests/mimics, lurkers, old god statues, class perks,
  and compact SD persistence.
- Save/resume, merchants, gas, chasms, and heavier hazard simulation remain
  future work.

## v1.0 Notes

v1.0 is the dungeon-variety release. It keeps the pocket RPG contract while
adding room templates, decorator passes, flooded rooms, finite fire fields,
directional traps, grates, chests, rare ambush monsters, shrines, and special
floor biases.

- Room generation now has multiple templates, including a maze-ish route.
- Decorators can add flooded rooms, trapworks, chests, shrines, grates, grass
  ambushes, and small reward pockets.
- Deep water is slow to cross, extinguishes burning, blocks land monsters, and
  can contain eel packs.
- Fire has a bounded fixed-capacity terrain-effect layer and a UI-only sparse
  dot overlay.
- Arrow/fire launcher/snare traps use short logs and visible feedback.
- Chests offer a choose-one popup. Mimics and Lurkers are rare surprises, not
  routine tax.
- Visible grates open from same-floor keys or buttons and persist through the
  existing terrain-delta floor state.
- Deferred ideas remain deferred: gas, chasms, floating item water physics, and
  merchants.

- Early bats are back as single monsters on floors `1..6`, so the first third is not only rats. Actual bat packs remain constrained to the middle floors and are small, normally leader plus one or two extras.
- Consumable stacking now includes scrolls as well as potions. Potion and scroll stacks merge by subtype no matter where the existing row is in inventory, consume one copy per use, drop one copy per drop, and still count each copy toward the 20-item pack budget. Throwables keep their separate contract: up to 20 stones/darts count as one inventory slot.
- Ranger starts with 12 arrows and Mage starts with 7 staff charges. Early floors also place arrows more often; deeper floors make arrows rarer again.
- The title menu entries are centered, and the `Back quits` footer has right padding.
- Game menu stats are rendered higher and spaced as `S12 I10 D14`, away from `Fx:`.
- Virtual Gear rows use comma separators, e.g. `* Dagger, Bow` and `* Hood+1, Cloak, Boots+2`.

## v0.10 Notes

### Release Contract

v0.10 is the polish-and-behavior release that grew out of the v0.9.1 bug list. The rule for this release is that player-facing oddities become explicit contracts: if the tiny screen forces a compromise, document it, test the state transition, and keep the log text short.

### UI Windows

Help, High Scores, and inventory pages now share small page/scroll helpers in `src/ui_layout.h`. Window lists reserve a frame area and a separate footer band, so counters such as `1/2`, `Back`, and `OK open` do not draw over the frame. High Scores is Gold-sorted and visually named `High Scores`; v1.1 later made this table persistent on SD as `hiscores.txt`.

The in-game menu top row includes compact stats as `S12 I10 D14`. Effects remain on the separate `Fx:` line so a long effect list cannot collide with stats.

### Log And Death

Normal play keeps the braided log to the last two phrases. Game Over can show up to three recent phrases, but the death cause is still single and idempotent: after HP reaches 0, later monster hits, hunger ticks, poison, or burn ticks cannot append a second `Killed by ...` cause.

Combat logs distinguish hits and misses: `You hit Rat.`, `You miss Rat.`, and `Rat misses you.` are separate beats. Long monster names can have short log names; `Skeleton Archer` uses `S.Archer` in logs and Game Over while keeping the full name on the monster card.

### Mapping, Blink, And Secrets

`Blink Scroll` is targeted. Invalid or occupied target tiles log the generic `Nothing happens.`, do not spend the scroll, and do not identify/reveal it by accident. The same fail-closed rule applies to other failed scroll effects. `Phase Scroll` is the random-teleport family: it follows a bounded random reachable path and can reveal a hidden door crossed by that path before moving the player.

Secret doors are optional side spaces, not required connectivity. A found secret should usually open into a short corridor or reward room with a little loot and, rarely, a guard. Manual search checks a wider nearby area, while bumping directly into a hidden-door wall reveals it regardless of DEX.

Mapping explores the whole current floor and reveals hidden doors, traps, and hidden actors on that floor. Secret-door reveal emits one debounced event; the device UI turns that into one ascending two-beep cue, even if several doors were revealed at once.

### Monsters

Bats are intentionally the only animal allowed to create a diagonal attack, but only after a real blink-strike changes their position. If a bat does not blink, it uses ordinary cardinal melee rules. Early bats are allowed as singles; bat packs live in the middle third where they are annoying rather than run-ending.

Skeleton Archers prefer distance: if they have a clear straight line at range, they shoot with a visible projectile; if the player is adjacent, they have a 50% chance to step away instead of attacking. Their 50% dodge applies to projectile/throwable/bolt-style damage, not 3x3 bursts.

Slimes split only after they survive a direct non-burst hit. The original and clone divide the remaining HP, copy the same `pack_id`, and the clone searches first around the player and then around nearby slimes for a valid free cell. This keeps the Brogue-like "hit it and it multiplies" feel without background replication.

Any lone monster, or last living member of a pack, can rarely break and flee when below 30% HP. This uses the existing Fear effect rather than a separate morale system.

### Generation

Door placement is now restricted to true chokepoints: exactly two opposite walkable sides and exactly one side touching room floor. This prevents doors from appearing inside rooms or ending in zero-length door-to-wall stubs. Map generation is still procedural and seed-deterministic rather than template-based; compact floor state overlays actor/item/trap/terrain deltas on regenerated base geometry.

### Starvation

Starving no longer allows Wait. Pressing OK to rest while starving logs `Too starved to wait.` and does not spend a turn. Movement and other real actions can still trigger starvation damage.

## v0.9.0 Notes

### System Contracts First

v0.9 treats each new mechanic as a full slice: spawn/drop, inventory UI, core effect, floor-state persistence, tests, and docs. New content should not land as a label-only feature.

### Items And Magic

- Wands now store current/max charges, render labels like `Wand Fire (1/3)`, stay in inventory at `0`, and print `No charges.` when empty.
- Wand types are `Fire`, `Frost`, `Force`, `Blink`, `Venom`, `Fear`, `Arc`, and `Reveal`.
- Stones and Darts are stackable throwables up to 20 per stack. A throwable stack counts as one inventory item.
- Charms are one-slot trinkets with unknown one-word labels such as `Charm VEX`; wearing or Identify reveals them.
- `Fire Bloom`, Flame Potion, and Fire Trap share the same `fire_burst` primitive: 3x3 burst, actor/player damage, burning, grass trampling, and puddle hissing.
- Drop means "put on the ground." If the player tile already has an item, the game prints `Ground full.` and does not spend the item.

### Monsters And Packs

- Packs have `pack_id`; hitting one member wakes the whole pack with pursuit memory.
- Bat has a blink-strike: around distance 2 it teleports into the 3x3 ring around the player and attacks, including diagonal blink positions.
- Skeleton Archer has 50% dodge against projectile/throwable damage; burst damage still works.
- Cube can engulf the player. While engulfed, movement is blocked, throwables are blocked, other monsters cannot hit the player, and directional attacks damage the cube from inside.
- Slime can split on non-burst hits.
- Wisp, Ogre, Snake, Rat, Bat, and Wight have small hit-effect hooks.

### Perks

Perks now use five choices per class. The choice screen shows names only; OK opens a detail card; Back returns to the list; the player cannot leave without choosing.

Current names:

- Warrior: `Iron Heart`, `Shield Bash`, `Riposte`, `Cleave`, `Hold Line`.
- Ranger: `Pin Shot`, `Hamstring`, `Trap Eye`, `Vantage`, `Dart Hand`.
- Mage: `Overcharge`, `Ember Staff`, `Frost Rune`, `Quartz Mind`, `Veil`.

Auto-level bonuses still remain; perks are extra identity on top.

### Regression Tests

Host tests now cover new mechanics and combinations: pack aggro, bat blink-strike, skeleton projectile dodge versus burst damage, cube plus fire burst, blocked throwing while engulfed, throwables as one inventory slot, Charm identification/equip, blocked ground drops, and empty wand behavior.

## v0.8.0 Notes

### Monster Hit Effects

Monster identity now has a small shared hit-effect layer instead of only raw HP/damage numbers:

- `Rat` has a high dodge chance against player damage and logs `Rat dodges.`;
- `Snake` has a 5% chance to poison the player when it hits;
- `Bat` has more HP and can heal +1 when it hits, logged as `Bat sips.`;
- `Archer` is now `Skeleton Archer`, with the flavor `Clack. Nothing personal.`;
- `Wight` appears in later monster pools, starts hidden, can be revealed by search, becomes visible when it attacks or is hit, and has a 5% chance to inflict Fear on hit.

### Effects In Menu

The back menu now shows a compact `Fx:` line near the top, rather than adding another menu row and risking overlap on the 64px screen.

## v0.8 Brainstorm Notes

### Pocket RPG Vibe

The next systems should protect the pocket RPG feeling: tiny readable rules, one-screen tactical moments, and short flavor lines that make the run feel bigger than the implementation. Favor "fake it till you make it" mechanics: cheap state, visible impact, strong feedback. A 3x3 flash, knockback, one-tile fire, or a sharp logline can sell a grown-up roguelike mechanic without carrying the full simulation cost.

### Progression And Perks

Rework progression as a whole rather than adding one-off perks. Auto-level bonuses can become a little smaller, while perks carry more identity and visible combat impact. Each perk tier should include at least one simple fallback pick such as `+20% max HP` for players who do not want a new active/conditional behavior.

Perks should be previewable before commitment. At level 3 and 6, selecting a perk should open a short description card with `OK choose` and `Back`; the player must choose one before returning to play, but can inspect all options first.

Candidate perk language:

- Warrior: surprise/force hit that can crit for x2 and knock an enemy back 1 tile if space exists.
- Ranger: shot perk with a 5-10% chance to knock the target back 1 tile; applies only to ranged shots, not dagger bumps.
- Ranger: rare pin/paralysis shot as an alternative, probably short duration to avoid locking the whole game.
- Mage: rare x2 charged crit or a stronger visual charge burst; on hit, show a short 3x3 ASCII flare around the target.
- Any class: plain survivability pick such as max HP +20%.

### Magic Direction

Magic should feel like a handful of strong pocket tools, not many invisible stat edits. Prefer effects that change the board for a few turns and are obvious in the log/render:

- Fire: burns actors and grass, can spread in a tiny bounded way, extinguished by puddles.
- Cold/slow: cheap timer-based slowdown like current slowed actors/player; no pathfinding changes needed.
- Air/force: push or knockback along a line if the destination tile is walkable.
- Mark/hex: visible target marker and +1 damage taken, useful because numbers stay small.
- Poison/paralysis: use sparingly, because monster HP is low; if used, make it short and readable.

Current design problem to solve: potions and scrolls do not yet feel impactful. This may be because the game is still easy, the effects are too hidden, the animations are too small, and, most importantly, the effects are not strongly tied into the core loop of positioning, terrain, pursuit, escape, and risk. The v0.8 magic pass should judge every effect by the question: "Would I change my next three moves because I have or fear this effect?"

### Wands, Scrolls, Potions

Wands should become rare, low-charge, high-impact tools. Move toward 8 wand types, but with low spawn weight and few charges so they feel like emergency buttons rather than inventory clutter. Display charges directly in inventory labels.

Potions and scrolls should keep the unknown-item vibe, but effects need short literary feedback instead of flat strings. A bad drink should say what happened, not just `Bad drink.`. Scrolls can be the bigger board/magic effects; potions can be self/throw effects; wands can be precise line tools.

### Gear And Trinkets

The Gear tab should show equipped baseline gear even before full itemization: bullet-style lines for armor/weapon/shield/bow/staff with `+N`. These are not removable inventory objects. World gear drops can be small upgrade pickups with a clear message when useful or not useful.

Current UI contract: Gear always draws two virtual equipped class lines before real inventory gear rows:
Warrior gets sword/shield plus helm/armor/boots; Ranger gets dagger/bow plus hood/cloak/boots; Mage gets dagger/staff plus hood/robe/sandals. These rows are display-only and are upgraded by gear pickups, not dropped or unequipped.

Consider one trinket/necklace slot as a compact RPG identity layer. This should be one equipped slot, not a full jewelry system. Examples: slow hunger, +1 search, small fire resistance, +1 ranged damage, or one extra wand charge on pickup.

### Throwables

Add stackable stones and darts as one inventory row per stack, up to 20 count. Stones deal about 1-2 damage; darts deal about 3-5. Item actions: `Throw` and `Drop`. This gives Warrior and resource-starved characters a small tactical ranged option without turning everyone into a Ranger.

### Monsters And Packs

Add monster variety through behavior hooks with tiny state:

- splitters that duplicate around themselves after being hit, with reduced HP;
- pack animals that share a short "player seen here" target when one member sees or is hit by the player;
- pack aggro must also trigger on player damage: hitting one pack member is treated as challenging the whole pack, so all members get `memory = 20` and a target, including normally lazy animals such as rats, bats, and snakes;
- fire/ice/acid-flavored monsters only if their effect is visible and cheap;
- idle wandering can exist, but damage must always wake survivors into pursuit.
- wandering is split into three cheap categories. Animal monsters can start asleep and wake from nearby player steps. Most idle monsters tremble around an anchor tile within one step. A small `FR_ACTOR_ROAMS` slice, roughly 10%, picks a random reachable target by bounded random-walk and heads there until close enough to retarget.
- packs reuse the same roam target: when a roaming pack member picks a target, active awake members in that pack copy it. This gives the pack a shared direction without storing a separate pack object or running expensive pathfinding for every actor.

Implementation preference: add a tiny `pack_id` byte to `FrActor` and assign it during pack spawning. This is more honest than guessing by monster type/radius and still cheap in memory. Avoid expensive per-monster BFS every turn. Use the existing remembered-target pursuit, occasional shared target, and simple one-step effects.

## v0.7.4 Notes

### Title Layout

The title screen keeps four menu rows, including `Sound: On/Off`, but the version label is now shorter and higher so it does not overlap `New game`.

### Ranged Resource Gate

Ranger and Mage projectile animation is only armed when the player can actually pay the shot cost. With zero arrows or zero charges, no projectile animation is shown; if the line target is not adjacent, the player attempts ordinary movement instead of firing a phantom shot.

### Damage Wakes Actors

Any monster that survives player damage now remembers the player's current position for a short pursuit burst, even if that monster was one of the simple animals that did not roll persistent chase behavior at spawn time. This keeps ranged attacks from feeling like they hit a statue.

## v0.7.3 Notes

### XP Table

Level progression now uses an explicit total-XP table instead of `level * 3`. Current thresholds are:

| Level | Total XP |
|---:|---:|
| 2 | 10 |
| 3 | 26 |
| 4 | 50 |
| 5 | 80 |
| 6 | 115 |

The target is that level 6 usually appears late, roughly around floor 15 for a run that fights a healthy share of monsters, without requiring a perfect full-clear.

### Perks

Levels 3 and 6 still grant their automatic class bonuses, and additionally create a pending perk choice. v0.9 expands this from three to five class-local perks with a description card before commitment.

Current v0.9 perk lists supersede the original v0.7.3 three-perk prototype.

### Inventory Actions

Inventory no longer auto-uses most items on OK. Selecting an item opens a small action menu:

- potions: `Quaff`, `Throw`, `Drop`;
- scrolls: `Read`, `Drop`;
- wands: `Zap`, `Drop`.

Food remains outside the inventory and is eaten from the game menu.

### Title Sound Toggle

The title menu has `Sound: On/Off`. When that row is selected, Left/Right toggles it and the row shows `< >`. v1.1 persists the flag to SD as `settings.txt`; LED status feedback remains separate.

## v0.7.2 Notes

### Compact Floor State

Floors no longer keep full `64x32` tile arrays for all 18 depths. That was too expensive for Flipper RAM and could push the app into out-of-memory or stack-corruption territory.

The current contract is:

- the base floor layout is regenerated deterministically from `run_seed` plus floor number;
- per-floor state stores the stairs coordinates, explored bitset, terrain/hidden-door deltas, actors, items, and traps;
- entering an already-seen floor rebuilds the deterministic base and then overlays that compact state;
- killed monsters, moved Warden state, picked-up items, sprung/seen traps, opened/trampled/revealed terrain, and explored cells persist across stairs.

Host tests cover this as a full round-trip: mutate terrain, explored cells, hidden-door reveal, actors, items, and traps; leave the floor; regenerate it; apply the compact state; then compare the restored runtime `tiles`, `actors`, `items`, and `traps` to the original runtime state.

### Stack Budget

Pathfinding scratch buffers are static rather than local stack arrays. This keeps BFS-style pathing away from the Flipper app stack, which is only `2 KB` in `application.fam`.

## v0.7.1 Notes

### Hunger And The Orb

The normal hunger clock advances every 2 player turns. After the player takes the Orb of Yonder, hunger and starvation damage advance every 10 and 50 turns respectively. The intent is that the return trip still has pressure, but the player is not doomed by already-cleared floors with little food left.

Food spawns are depth-weighted: floors `1..6` always place a ration, floors `7..12` place one at 60%, and deeper floors at 30%. Runs still start with 3 rations.

### Floor Persistence

The run keeps per-floor state for each of the 18 floors. Moving up/down saves the current floor and reloads the destination floor if it already exists. This means cleared rooms stay cleared, picked-up items stay gone, discovered/trampled terrain remains changed, and the Warden can continue existing away from the player's current floor.

### Pursuit

Actors that pursue now path toward their remembered target with a small BFS step instead of only greedy cardinal movement. This fixes corridor corners and door approaches. Some simple animals are allowed to be non-pursuers, with a small chance to be persistent anyway; humanoids, constructs, wisps, ogres, and the Warden pursue reliably.

The Warden is special: when it is on another floor, it walks toward the stair that brings it closer to the player's floor. When far from the player on the same floor, it moves every turn; when close, it falls back to every other turn.

### Potions

Identical potions stack in one inventory row and consume one count at a time. The stack does not bypass the pack limit: 20 healing potions are still 20 carried items. Unknown potions open a `Quaff`/`Throw` choice. Once known, helpful potions quaff immediately and harmful potions jump straight to target mode for throwing.

### Grass Dew

Trampling tall grass turns it into trampled grass. One grass tile in ten can release dew; dew is picked up immediately and heals +1 HP, capped at max HP. The device UI shows this as a short bubble around the grass tile before it disappears.

### Search Pressure

DEX now matters outside combat. Moving or resting searches adjacent tiles. Hidden doors can become closed doors, and hidden traps can become visible traps. Ranger has the strongest trap awareness, while high-DEX classes get better passive searching.

### Hold Movement

Holding a direction repeats movement after a short delay. Held bump-attacks are intentionally stopped after the first hit until the button is released, so auto-walk does not turn into accidental auto-combat.

### Feedback Budget

Device feedback should stay sparse: a single vibration for hits, a short level-up cue, and a low-HP warning when crossing below 30%. LED is green above low HP, red below it, yellow for starving/burning, and blue for poison/slow warnings. Avoid adding feedback to routine walking or menu navigation.

### Death Status

Game Over shows one cause-oriented status: `KILLED`, `STRV`, `BURN`, or `POIS`. Hunger labels are hidden on the Game Over HUD so they do not overlap or imply the wrong cause of death. In-game hunger labels use `HNGR` and `STRV` to keep the status row short.

## v0.6.1 Notes

### Stairs Contract

Every generated floor must have exactly one `stairs up` tile. Floors `1..17` must also have exactly one `stairs down` tile. Floor `18` has no down stairs and instead contains the Orb of Yonder.

This is covered by host tests across several seeds, including coordinate/bounds checks for both stair types.

### Braided Logline

The bottom log intentionally keeps the last two short messages from the same turn together when there is room. This can produce little micro-stories like:

```text
Door creaks. Rat hits you.
```

or, on bad turns:

```text
Door creaks. Killed by Rat.
```

This is a feature, not a bug. The Flipper screen is too small for a full message history, but combining two terse beats often makes the action feel more physical and funny than replacing each beat immediately. Keep log text short, readable, and compatible with this concatenation style.

Implementation detail: `fr_log()` appends a new sentence to `game->log`, then trims the visible braid to the last two punctuated phrases so longer turns do not run off the screen.

### Help Window Layout

Help screens reserve a separate footer band for `Back` / `OK open` hints. The help frame should not extend into that footer band, and the visible row count should leave bottom padding inside the frame.
