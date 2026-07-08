# PocketLab – Design

## Goal

Reduce Flipper new-user churn with a guided, gamified, on-device learning path.
Ship a small, polished vertical slice first, then scale by adding content only.

## Principles

- **Content is data.** Labs live in `helpers/pocketlab_content.c` as arrays of
  steps. Adding a lab never touches the engine.
- **Official firmware only.** Keeps the app eligible for the official catalog.
- **Legal and educational.** Labs teach the device; nothing offensive.
- **Local only.** State is a small versioned file on the SD card. No accounts.

## Architecture

```
ViewDispatcher
 ├─ Submenu           main menu
 ├─ Widget            progress / about
 ├─ VariableItemList  settings (language, sound)
 └─ LessonView        custom animated view that runs a lab

SceneManager scenes: menu -> lesson / progress / settings / about
```

The lesson view owns a `FuriTimer` that drives micro-animations (progress dots,
blinking hints, animated reward and XP count-up) and plays sound via the
notification service, gated by the sound setting.

## Lab model

A lab is a list of steps rendered generically by the lesson view:

| Step type | Renders                                    |
| --------- | ------------------------------------------ |
| `text`    | title + multiline body                     |
| `quiz`    | question + options, retry until correct    |
| `try`     | pointer to a real built-in app             |
| `reward`  | XP count-up, badge, level persisted        |

Every string is stored per language (`en`, `ru`).

## State model

`PocketLabState` (see `helpers/pocketlab_storage.h`) holds a magic + version
header, language, sound flag, XP, level and a `completed_mask` bitfield. Level
curve: `level` is the largest `n` with `xp >= n^2 * 50`.

## Localization notes

The app ships English only. Stock firmware fonts do not include Cyrillic glyphs.
The string table (`pocketlab_text`) keeps UI strings centralized, so introducing
languages is straightforward once a bundled Cyrillic font is added.

## Roadmap

- More tracks: Sub-GHz, RFID/NFC, GPIO.
- Bundled Cyrillic font.
- Badges gallery, daily streak.
- Official catalog submission (see `catalog/manifest.yml`).
