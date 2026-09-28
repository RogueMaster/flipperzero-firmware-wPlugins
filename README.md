# Roulette for Flipper Zero

A polished, single-screen revolver roulette game for the [Flipper Zero](https://flipperzero.one/), built natively against the official Flipper Application Package (FAP) framework. Roulette pairs a hand-drawn six-chamber cylinder with hardware-randomized chamber selection, tactile vibration, multi-tone audio, and synchronized LED feedback to deliver a tense, arcade-style game loop in roughly three hundred lines of C.

## Overview

Roulette models the classic six-shot revolver as a finite state machine running inside the Flipper's cooperative event loop. On each round, the application uses the Flipper's true hardware random number generator (`furi_hal_random`) to place a single bullet in one of six chambers, then advances the cylinder one position per trigger pull. Each pull either resolves to a *click* — incrementing the player's survival score — or a *bang*, which ends the round with a full-screen flash, a low C3 tone, the red status LED, and a sustained haptic pulse.

The game is rendered entirely with the Flipper's 128×64 monochrome canvas primitives. The cylinder is drawn parametrically from polar coordinates, with the active chamber filled as a solid disc and the remaining chambers stroked as outlines, giving the player a clear at-a-glance read of the rotation state.

## Features

- **Hardware-backed randomness.** Bullet placement is sourced from `furi_hal_random_get()`, the Flipper Zero's on-die TRNG, ensuring each spin is non-deterministic and unpredictable across reboots.
- **Five-screen UI machine.** Title, Ready, Click, Bang, and Spin screens are driven by a single `Screen` enum, keeping render logic flat and auditable.
- **Multimodal feedback.** Each game event triggers a tailored `NotificationSequence`: a sharp click plus vibro on a safe pull, a layered red-LED + low-tone + extended vibration on a bang, and a descending ratchet of notes on a re-spin.
- **Procedural cylinder rendering.** The revolver cylinder is computed live from chamber count and radius using `cos`/`sin`, so the visual is data-driven rather than asset-baked.
- **Score tracking within a round.** A running "Survived" counter is shown on the Ready screen and surfaced as the final score on the Bang screen.
- **Cooperative event-driven architecture.** Input and timer events are funneled through a single `FuriMessageQueue`, with shared state guarded by a `FuriMutex` — the canonical Flipper concurrency pattern.
- **Self-contained.** No SD-card assets, no external dependencies, no settings persistence. Drops in cleanly to any uFBT or firmware-tree build.

## Controls

| Screen   | Button           | Action                                  |
| -------- | ---------------- | --------------------------------------- |
| Title    | `OK`             | Spin the cylinder and begin a round     |
| Title    | `Back`           | Exit the application                    |
| Ready    | `OK`             | Pull the trigger on the current chamber |
| Ready    | `Left` / `Right` | Re-spin the cylinder                    |
| Ready    | `Back`           | Return to the title screen              |
| Anywhere | Hold `Back`      | Force-exit the application              |

Re-spinning re-rolls the bullet into a fresh chamber and **resets your Survived counter to zero** — it buys you a clean six-chamber cylinder at the cost of the streak you have built.

Transition screens (Click, Bang, Spin) are timed and auto-advance — input is intentionally ignored during the animation window so the player cannot mash through feedback.

## Building and installing

This project ships as a standalone FAP. Either of the two standard Flipper toolchains will build it without modification.

### With uFBT (recommended)

```bash
# From the repo root
ufbt
ufbt launch   # builds, uploads to a connected Flipper, and launches
```

The resulting `roulette.fap` will be placed under `dist/` and can be copied to `/ext/apps/Games/` on the SD card.

### Inside a firmware tree

Drop the project directory into `applications_user/` of an [official firmware](https://github.com/flipperdevices/flipperzero-firmware) checkout (or any compatible distribution) and build with the standard FBT target:

```bash
./fbt fap_roulette
```

## Project layout

```
.
├── application.fam      # FAP manifest (id, entry point, category, icon)
├── roulette.c           # Application entry point, state machine, and rendering
├── roulette_10px.png    # 10x10 1-bit app icon
├── changelog.md
├── LICENSE
└── README.md
```

The application exposes a single entry point, `roulette_app`, declared in `application.fam` and implemented in `roulette.c`. All state lives in a heap-allocated `RouletteState` struct, and is torn down deterministically before the entry point returns.

## Technical notes

- **Tick rate.** The render loop is driven by an 8 Hz periodic `FuriTimer`. Transition screens count down in tick units (`MSG_DISPLAY_TICKS = 20`), giving a 2.5-second window for the click screen, 5 seconds for the bang screen, and 1.25 seconds for the spin screen.
- **Stack budget.** The manifest reserves a 2 KB stack — comfortable for the application's shallow call graph and the canvas drawing primitives.
- **Mutex discipline.** State is acquired before mutation and before each render pass, then released before the view port is asked to redraw, avoiding contention with the GUI thread.
- **Resource lifecycle.** GUI, notification, message queue, view port, timer, and mutex resources are all released in the reverse order of acquisition on exit.

## License

Released under the [MIT License](LICENSE).
