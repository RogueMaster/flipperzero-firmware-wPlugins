# Flipper Oregon Trail

An Oregon Trail-inspired journey across 2,000 miles of frontier America — playable on a Flipper Zero in 128×64 monochrome pixels.

> **Educational project.** Oregon Trail is the intellectual property of Houghton Mifflin Harcourt (formerly MECC). This project is an independent, non-commercial educational endeavor inspired by the 1971 original created by Don Rawitsch, Bill Heinemann, and Paul Dillenberger, and the 1985 MECC Apple II edition developed under R. Philip Bouchard. No code, assets, or proprietary materials from any commercial release were used. All graphics, game logic, and sound sequences were written from scratch.

---

## Gameplay

You lead a two-player party westward from Independence, Missouri to Oregon City — 2,000 miles across six distinct geographic regions. Every decision costs time, food, or health. The trail does not forgive poor planning.

### Trail Controls

| Input | Action |
|---|---|
| **OK** | Advance one day |
| **Hold OK** | Fast-advance (1 day per 800ms) |
| **Right** | Go hunting |
| **Left** | Rest day (+0.5 HP, costs food) |
| **Down** | Map / Settings |
| **Back** | Exit confirmation |

### Map Screen

- **Page 1** — Visual trail map with landmark markers and current position
- **Page 2** — Miles traveled, miles to Oregon, next stop
- **Page 3** — Trail Settings: adjust Pace and Rations

### Pace & Rations

| Pace | Miles/day | Effect |
|---|---|---|
| Steady | 15 | Safe |
| Strenuous | 22 | -0.5 HP every 4 days |
| Grueling | 30 | -0.5 HP every 2 days |

| Rations | Lb/day/player | Effect |
|---|---|---|
| Filling | 5 | Healthy |
| Meager | 2 | Slow HP drain |
| Bare Bones | 1 | Fast HP drain |

> **Oxen fatigue:** 5 consecutive days at Grueling pace triggers a mandatory 2-day rest. You cannot buy your way out of it.

---

## The Hunt

Animals wander in from the right. Your stickman shooter is fixed at the left. The bullet fires horizontally — timing your shot as animals cross the fire line is the entire game.

| Species | Zone | Yield | Speed | Risk |
|---|---|---|---|---|
| Rabbit | All | 2–6 lb | Fast | None |
| Deer | Plains | 40–80 lb | Medium | None |
| Buffalo | Prairie/Plains | 100–200 lb | Slow | **Gored: -2 HP** |
| Elk | High Plains+ | 50–100 lb | Medium | None |
| Wolf | High Plains+ | 20–40 lb | Fast | **Mauled: -2 HP** |

**Wolf packs:** A wolf that enters the screen has a 20% chance of triggering a faster companion wolf 3–5 seconds later. The companion is 30% quicker than normal. Exiting the hunt while wolves are on screen costs -1 HP — you ran, and they gave chase.

**Buffalo/Wolf exits:** If a dangerous animal reaches the left edge without being shot, your party suffers the full -2 HP penalty. At low health, this is lethal.

---

## Rivers

Three crossings interrupt the journey. Depth is computed from base depth + seasonal modifier + rainfall. Snowmelt in May and June adds +1.5ft — the Snake River in May is genuinely terrifying.

| River | Miles | Base Depth |
|---|---|---|
| Big Blue River | 100 | 2.5 ft |
| South Platte | 250 | 3.8 ft |
| Snake River | 1,300 | 4.2 ft |

| Choice | Cost | Risk |
|---|---|---|
| Ford | Free | Depth/current dependent: food loss, HP loss, or both |
| Take Ferry | $10–$20 | Safe |
| Wait 1 Day | 1 day + food | Reduces depth by 0.5 ft |

---

## Forts

Four trading posts appear along the route. A splash screen warns you before entry — you can skip them entirely.

| Fort | Miles |
|---|---|
| Fort Kearney | 307 |
| Fort Laramie | 640 |
| Fort Bridger | 1,070 |
| Fort Boise | 1,438 |

**Store items:** Food (buy/sell), Ammo (buy/sell), Medicine (instant +2 HP, player-specific). Selling is always at a loss. Medicine greys out if the player is at full health. The market doesn't care about your situation.

**Wagon repairs** also cost money — hire a repairman (1 day, $15–$25) or fix it yourself (2–3 days, free).

---

## Health System

- Each player has 4.0 HP, displayed as fill blocks
- A half-filled block represents 0.5 HP
- Dead players are replaced by a skull; food consumption halves automatically
- Rest gives +0.5 HP per day
- One player can die and the surviving player continues — alone, with half the food bill

---

## Weather & Events

The event system uses a 6-zone × 12-month climate risk table inspired by MECC's original climate modeling approach. Risk values drive event probability — blizzards only appear in mountain zones, flash floods in the desert, early frost in the Cascades in October.

Weather worsens significantly after September. Leaving in May gives you the best odds of crossing South Pass before winter. Leaving in August is survivable. Leaving in October is a statement.

---

## Regional Backdrops

The trail landscape changes as you travel:

| Region | Miles | Backdrop |
|---|---|---|
| Prairie | 0–300 | Rolling grass, tufts |
| Plains | 300–640 | Low rolling hills |
| High Plains | 640–947 | Ridge silhouette |
| Mountains | 947–1,200 | Sharp peaks |
| Desert | 1,200–1,600 | Flat horizon, cacti |
| Forest | 1,600–2,000 | Pine silhouettes |

## Quick Install (no build required)

1. Download `oregon_trail.fap` from this repository
2. Copy it to your Flipper SD card at `apps/Games/oregon_trail.fap`
3. Launch from the Apps menu → Games

Compiled against Unleashed firmware. May require a firmware update if you see API version errors.

## Building

This is a Flipper Zero FAP (Flipper Application Package), built against the [Unleashed firmware](https://github.com/DarkFlippers/unleashed-firmware).

```bash
# Clone into your firmware's applications_user directory
cd flipperzero-firmware/applications_user
git clone https://github.com/jlaughter/flipper_oregon_trail

# Build from firmware root
./fbt fap_oregon_trail

# Build and deploy over USB
./fbt launch APPSRC=applications_user/flipper_oregon_trail
```

Requires Flipper firmware SDK. Tested against Unleashed firmware. ARM-none-eabi-gcc 12.3 recommended.

---

## Project Structure

flipper_oregon_trail/
├── application.fam       — FAP manifest
├── game_state.h          — Shared types: Screen enum, Player, Trail, GameState
├── oregon_trail.c        — Main app loop, all screen draw functions, input handling
├── hunt.c / hunt.h       — Hunting minigame: animal spawning, bullet physics, wolf packs
├── day.c / day.h         — Day advancement, food consumption, event system, weather table
├── river.c / river.h     — River crossing logic, seasonal depth, ford outcomes
├── event_draw.c / .h     — Scrollable event card renderer
├── sprites.c / sprites.h — Pixel art: wagon, stickman, animals, skull, tombstone, wolf
├── sound.c / sound.h     — Sound sequences via Flipper piezo buzzer
└── icon.png              — 10×10 FAP icon
└── oregon_trail.fap      — Compiled Application

---

## Inspiration & Attribution

This project exists because of the original work of:

- **Don Rawitsch, Bill Heinemann, and Paul Dillenberger** — creators of the 1971 original Oregon Trail, written in BASIC for the HP 2100 while student teachers in Minnesota
- **R. Philip Bouchard and the MECC development team** — creators of the definitive 1985 Apple II version, which introduced the graphics, hunting minigame, regional weather modeling, and landmark system that define the game in cultural memory
- **The retro computing community** — whose documentation of the original Apple II source, climate tables, and game mechanics made historically-grounded design choices possible

Oregon Trail is a trademark and intellectual property of Houghton Mifflin Harcourt. This project is not affiliated with, endorsed by, or derived from any commercial release. It is an independent educational project released under the MIT License.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

This license applies to the code in this repository only. It does not grant any rights to the Oregon Trail name, trademark, or any intellectual property of Houghton Mifflin Harcourt.

---

*Don't be a chew toy.* 🐺
