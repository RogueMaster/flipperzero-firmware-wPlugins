# Tea Timer

A pour-over tea brewing timer for the Flipper Zero. It keeps the count of pours for one
session, grows every next pour duration by a per-preset increment, and plays a single
sound/vibration when the pour is done.

Presets are plain text `.tea` files. You can create, edit and delete them right on the
Flipper, or write them on a computer and copy them to the SD card.

## Requirements

- Flipper Zero with firmware that supports external apps (FAP).
- A microSD card inserted into the Flipper.
- `ufbt` (uFBT) for building from source: <https://github.com/flipperdevices/flipperzero-ufbt>

## Install

From the project directory:

```bash
ufbt                # build → dist/tea_timer.fap
ufbt launch         # build, upload over USB and run on the device
```

Or copy `dist/tea_timer.fap` to the SD card manually:

```
/ext/apps/Tools/tea_timer.fap
```

## Presets

Preset files live in a single directory on the SD card:

```
/ext/apps_data/tea_timer/
```

Create the directory and put one or more `.tea` files there, or create them on the device
(see below). The app lists only files with the `.tea` extension (the system file browser is
used for selection).

If the directory is missing or contains no `.tea` files, the app shows `No presets found`
with a `Reload` button. If the SD card is not available, it shows `SD card not found` and
lets you retry or exit.

### `.tea` file format

Line-based `key: value` pairs, one pair per line. Comments start with `#`, empty lines are
ignored, unknown keys are ignored. UTF-8/ASCII only (no Cyrillic — the device fonts cannot
render it). Maximum file size is 1024 bytes.

| Key | Type | Allowed range | Example |
|---|---|---|---|
| `name` | text | 1–24 chars: Latin letters, digits, space, `_` | `Green Tea` |
| `leaf_g_per_100ml` | decimal, `.` as separator, exactly one digit after the dot | `0.1`–`50.0` | `5.0` |
| `water_temp_min_c` | integer | `85`–`90` | `85` |
| `water_temp_max_c` | integer | `85`–`90` | `90` |
| `first_pour_sec` | integer | `1`–`3600` | `20` |
| `increment_sec` | integer | `0`–`600` | `5` |
| `max_pours` | integer | `1`–`99` | `6` |

The five keys `name`, `leaf_g_per_100ml`, `first_pour_sec`, `increment_sec` and `max_pours`
are mandatory and must appear exactly once.

The water temperature is an optional pair: `water_temp_min_c` and `water_temp_max_c` must be
given **together** or not at all.

- Both keys are missing → the preset gets the default range `85`–`90`.
- Both keys are present → values are used as is; `water_temp_min_c` must not be greater than
  `water_temp_max_c` (equal values such as `90`/`90` are allowed).
- Only one key of the pair is present → the file is rejected with
  `Missing field: water_temp_min_c` or `Missing field: water_temp_max_c`.

The preset card shows the range as `Water 85-90 C`. Presets created or edited on the device
always get both keys written to the file.

Example `green_tea.tea`:

```tea
# Green Tea — 5 g per 100 ml, 6 pours, water 85-90 C
name: Green Tea
leaf_g_per_100ml: 5.0
water_temp_min_c: 85
water_temp_max_c: 90
first_pour_sec: 20
increment_sec: 5
max_pours: 6
```

New and edited files always contain the seven keys in this order.

With this preset the first pour lasts 20 s, the second 25 s, the third 30 s, and so on.
`max_pours` is only a recommendation: brewing beyond it is allowed and is shown as
`Pours 9 / 8` with an `OVER` badge.

If a file is invalid, the app shows a short dialog with the reason (for example
`Missing field: max_pours`, `Out of range: first_pour_sec`,
`Out of range: water_temp_min_c`, `File is larger than 1 KB`) and returns to the file
browser. The other presets stay usable.

### Creating a preset on the device

Main menu → `New preset`. The form walks through seven steps:

1. `Tea name` — 1–24 characters: Latin letters, digits, space, `_` (the on-screen keyboard).
2. `Leaf, 0.1 g/100ml` — 0.1–50.0 g (entered in tenths of a gram).
3. `Water min, C` — 85–90 (default `85`).
4. `Water max, C` — from the entered minimum to 90 (default `90`), so the minimum can never
   exceed the maximum.
5. `First pour, sec` — 1–3600 s.
6. `Step, sec` — 0–600 s (the increment added to every next pour).
7. `Max pours` — 1–99.

The summary screen shows the entered values (`Leaf 5.0  Water 85-90 C` on one line) with a
`Save` button. On `Save` the app writes
the preset to `/ext/apps_data/tea_timer/`, reads it back to verify, and shows `Saved` with
the file name; `OK` returns to the preset list. The file name is derived from the tea name
(`Green Tea` → `green_tea.tea`). If that name is already taken, the app adds a suffix
(`green_tea_2.tea`, `green_tea_3.tea`, …) — nothing is overwritten.

`BACK` on any step returns to the previous step (from the first step — to the main menu)
and leaves no files behind. Invalid values are rejected with a `Bad value` dialog that
shows the field and its allowed range; the entered values are kept.

### Editing and deleting a preset

On the preset card press `>` to open the actions menu:

- `Edit` — opens the same form pre-filled with the current values. `Save` writes to the
  same file (the file name does not change); the pour counter of an active session is kept.
  `BACK` on the first step returns to the actions menu without touching the file.
- `Delete` — shows a confirmation with the tea name, the file name and a `No`/`Yes` choice.
  `No` (center, also `BACK`) keeps the file; `Yes` (right) removes it from the SD card, shows
  `Deleted` and returns to the preset list. Deleting the preset that is currently being
  brewed also resets the pour session.

If a write or delete fails (for example, the SD card is missing), the app shows a
`Cannot save` / `Cannot delete` dialog and never loses the existing file.

## Controls

| Screen | Button | Action |
|---|---|---|
| Main menu | `OK` / `Right` | Open the highlighted item: `Presets` list or `New preset` form |
| Main menu | `BACK` | Exit the app |
| File browser | `OK` / `Right` | Open the highlighted preset |
| File browser | `BACK` | Back to the main menu |
| Preset card | `OK` (center `Start`) | Start the pour immediately |
| Preset card | `>` (right) | Open the actions menu (`Edit` / `Delete`) |
| Preset card | `BACK` | Back to the file browser |
| Actions menu | `OK` / `Right` | Run `Edit` or `Delete` for the current preset |
| Actions menu | `BACK` | Back to the preset card |
| Preset form | `OK` | Confirm the current step / `Save` on the summary |
| Preset form | `BACK` | Previous step (file is not written) |
| Delete confirmation | `OK` (center `No`) / `BACK` | Keep the file, back to the preset card |
| Delete confirmation | `>` (right `Yes`) | Delete the file |
| `Saved` / `Deleted` | `OK` / `BACK` | Back to the preset list |
| Brewing (`RUNNING`) | `OK` | Stop the pour; the pour counter does not change |
| Brewing (`RUNNING`) | `BACK` | Stop and go back to the preset card |
| `Time is up!` (`DONE`) | `OK` | Start the next pour |
| `Time is up!` (`DONE`) | `BACK` | Back to the preset card |

The display backlight is forced on while a pour is counting down and returns to the system
auto mode when the pour finishes or the app exits. There is no autostart: every next pour
is started manually.

## Build and tests

All commands are run from the project directory and use `ufbt` only:

```bash
ufbt                # build dist/tea_timer.fap
ufbt launch         # build, upload and run
ufbt format         # apply clang-format
ufbt lint           # static analysis
```

Unit tests run **on the device** from the app itself (minunit runner compiled into the FAP).
The runner is compiled only in debug builds (`FURI_DEBUG`) or when the `TEA_TIMER_TESTS`
define is added to `application.fam` by hand; the catalog build has neither, so the test
command is not registered there.

```bash
ufbt launch         # upload and run the app
ufbt cli            # in another terminal — CLI session
```

Inside the CLI:

```
tea_timer_test               # all suites
tea_timer_test tea_preset    # .tea parser only
tea_timer_test brew_math     # pour arithmetic only
tea_timer_test preset_write  # on-device preset write/delete logic
```

Expected report:

```
test_reference_file()
...
40 tests, 273 assertions, 0 failures
Time: 1502 ms
Leaked: 0
Status: PASSED
```

A successful run is also confirmed by the device notification (sound/vibration); a failure
plays the error notification. The CLI test command is registered only in builds with
`FURI_DEBUG` or a manually added `TEA_TIMER_TESTS` define, so a release build omits it.

## Project layout

```
tea_timer/
├── application.fam        # FAP manifest
├── tea_timer.c            # entry point, resource ownership
├── scenes/                # SceneManager scenes
├── views/                 # custom brew view (Canvas)
├── lib/tea_logic/         # pure logic: .tea parser, pour arithmetic
├── unit_tests/            # on-device unit tests (minunit)
├── images/                # icons
└── specs/                 # feature specifications (Russian)
```
