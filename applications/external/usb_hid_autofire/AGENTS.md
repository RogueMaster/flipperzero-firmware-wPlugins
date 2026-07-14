# AGENTS.md

## Purpose

This file captures current repository architecture, runtime contracts, and active maintenance priorities. Completed feature history belongs in `CHANGELOG.md`.

## Project Snapshot

- Name: `usb_hid_autofire`
- Platform: Flipper Zero external app (`.fap`)
- Primary behavior: emulate USB HID and generate repeated selected events (`Mouse Left/Right`, `Key Enter`, `Key Space`)
- Entry point: `usb_hid_autofire_app` in `usb_hid_autofire.c`
- App manifest: `application.fam`
- App category: `USB`
- Current version: `0.7` (`version.h`)

## Current Module Layout

- `usb_hid_autofire.c`: app lifecycle/orchestration, event loop, centralized cleanup
- `usb_hid_autofire_i.h`: shared state, enums, constants, internal module contracts
- `usb_hid_autofire_controller.c`: input/controller transitions, delay/mode/preset logic, confirmation flow
- `usb_hid_autofire_hid.c`: timer callbacks, HID press/release engine, click scheduler, CPS tracking
- `usb_hid_autofire_ui.c`: canvas rendering for main/help screens
- `usb_hid_autofire_settings.c`: settings load/save/flush with debounced write scheduling

## Critical Invariants

- Always restore previous USB configuration before app exit.
- Never leave any fired control pressed on exit or failure.
- Keep `Back` exit reliable from main screen regardless of autofire state.
- Keep UI/input responsive while autofire is active.

## Runtime Flow

1. Allocate queue, viewport, click timer, UI refresh timer, settings debounce timer.
2. Switch USB config to HID mouse mode (unless screenshot macro is enabled).
3. Load settings from `APP_DATA_PATH(".settings")` via Flipper Format.
4. Feed input and timer callbacks into one event queue.
5. Main loop blocks on queue and handles:
   - click phase ticks (`press`/`release` + timer rearm),
   - periodic UI refresh ticks for real-time CPS (`250 ms`),
   - settings save debounce ticks,
   - key events for mode/delay/preset/help/toggle/exit.
6. On stop/exit, stop timers, release active control, flush settings, restore USB config, free resources.

## Control Model

- `OK` short (main): toggle active/inactive
- `OK` long (main): cycle presets (`Slow` -> `Medium` -> `Fast`)
- `OK` confirm: apply pending high-CPS preset when dialog is shown
- `Up` short/hold-repeat: cycle mode backward
- `Down` short/hold-repeat: cycle mode forward
- `Left` short/hold-repeat: decrease delay with acceleration
- `Right` short/hold-repeat: increase delay with acceleration
- `Back` long (main): open help screen
- `Back` short (help): close help screen
- `Back` short (main): exit app

## Timing And Limits

- Delay is clamped to `5..10000 ms`.
- Click cycle uses half-delay scheduling (`delay / 2` each phase), with defensive `>=1 ms` timer floor.
- UI shows real-time CPS from observed release timing, so displayed CPS reflects runtime lag.
- Presets:
  - `Slow`: `250 ms`
  - `Medium`: `120 ms`
  - `Fast`: `70 ms` (confirmation required)

## Persistence

Settings are stored in `APP_DATA_PATH(".settings")` with debounced writes (`500 ms`):

- `delay_ms`
- `mode`
- `preset`
- `startup_policy`
- `last_active`

Current UX always forces startup policy to paused on launch. The persisted `startup_policy` value is saved for future flexibility but is not currently applied at startup.

## Active Priorities

### P2 (Maintainability)

1. Add internal comments only where timing/state transitions are non-obvious.

### P3 (Quality + Release)

1. Add tests for controller state transitions and timing math.
2. Validate behavior across multiple Flipper firmware versions in CI.
3. Expand docs with troubleshooting and host OS behavior notes.

## Build And Verification

- Build: `./fbt fap_usb_hid_autofire`
- Build and launch: `./fbt launch_app APPSRC=usb_hid_autofire`

Manual smoke checklist:

- App opens and renders status/version.
- `OK` toggles active/inactive.
- `Up/Down` cycle fired mode.
- `Left/Right` adjust delay and clamp at `5..10000 ms`.
- `OK` long cycles presets; `Fast` requires confirmation.
- `Back` long opens help; `Back` short closes help.
- `Back` short from main exits and USB behavior is restored.
- While active, host receives repeated selected events.
- After exit, no stuck mouse/keyboard pressed state remains.
- After restart, persisted settings are restored.

## Notes For Future Agents

- Preserve control contract unless UX changes are intentional and documented.
- If timing behavior changes, keep labels and help text unit-accurate.
- USB config restoration and release-on-stop are non-negotiable safety rules.
- Keep `AGENTS.md` focused on current state and operating guidance; use `CHANGELOG.md` for completed feature history.
- To bump the version of the application you need to adapt `VERSION` in `version.h` and `fap_version` in `application.fam`. Semver is used.
