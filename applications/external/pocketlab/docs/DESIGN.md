# PocketLab - Design

## Goal

Reduce Flipper new-user churn with a guided, gamified, on-device learning path.
Content scales by adding data; the engine stays fixed.

## Principles

- **Content is data.** Labs live in `helpers/pocketlab_content.c` as arrays of
  steps. Adding a lab never touches the engine.
- **Official firmware only.** Keeps the app eligible for the official catalog.
- **Legal and educational.** Labs teach the device; each ends with a hands-on
  step that points at the real built-in app.
- **Local only.** State is a small versioned file on the SD card. No accounts.
- **Lean bundle.** A `.fap` is loaded whole into RAM, so the app avoids heavy
  assets; the bundled fonts and translations are kept as small as possible.

## Architecture

`ViewDispatcher` + `SceneManager`. The UI is built from custom canvas views
rather than stock GUI modules, because the stock modules render Latin only and
cannot show Cyrillic.

```
ViewDispatcher views:
  home_view       tile menu
  labs_list_view  grouped lab list (marquee for long titles)
  lesson_view     runs a lab step by step
  exam_view       standalone Quiz
  progress_view   Profile: level/XP cards, streak, XP glitch at the cap
  badges_view     achievement gallery
  levelup_view    level-up flash
  about_view      matrix-rain intro + self-typing terminal + coffee page
  settings_view   custom list styled like the stock one (renders Cyrillic)
  reset_view      reset confirmation for Russian (custom, renders Cyrillic)
  widget          reset confirmation for English (stock)

SceneManager scenes: menu, labs, lesson, progress, badges, levelup,
                     settings, reset_confirm, about, exam
```

Most views own a `FuriTimer` for micro-animations (the breathing tile cursor,
blinking hints, the reward count-up, the About animations, the XP glitch) and
play sound via the notification service, gated by the Sound setting.

## Lab model

A lab is a list of steps rendered generically by the lesson view:

| Step type | Renders                                    |
| --------- | ------------------------------------------ |
| `text`    | title + multiline body                     |
| `quiz`    | question + options, retry until correct    |
| `try`     | pointer to a real built-in app             |
| `reward`  | XP count-up, badge, level persisted        |

Lab and quiz content is authored in English in `helpers/pocketlab_content.c`.
Translations live in a separate table looked up by `pocketlab_tr(en)` (English
to Russian); anything not translated falls back to English. Flipper menu names
inside instructions stay in English, matching the on-device UI.

UI chrome (menus, labels, buttons) is centralized in `pocketlab_text`
(`helpers/pocketlab_i18n.c`), one entry per language.

## State model

`PocketLabState` (see `helpers/pocketlab_storage.h`) holds a magic + version
header, then the Sound / LED / Vibro toggles, the language, XP, level, a
`completed_mask` bitfield and a daily streak (`streak_days`, `last_day`). Older
on-disk layouts migrate forward on load. Level curve: `level` is the largest `n`
with `xp >= n^2 * 50`. XP is capped at 9999; both finishing a lab and the Quiz
award it. At the cap the XP number glitches, reusing the About "coffee" effect.

## Localization and fonts

The app ships **English and Russian**, switchable in Settings.

- English uses the stock Flipper fonts (native look).
- Russian uses bundled public-domain X11 misc-fixed fonts (a bold 6x13 and a
  compact 6x12), reprocessed to proportional spacing and carrying Cyrillic,
  drawn via `canvas_set_custom_u8g2_font()`. The big XP number reuses the stock
  `FontBigNumbers` (ProFont), which already covers the glitch glyphs.
- Stock GUI modules (VariableItemList, Widget) render Latin only, so any screen
  that must show Cyrillic is a custom canvas view (Settings, the reset dialog).
- Long lab and quiz titles scroll (marquee) so the whole title can be read.

## Roadmap

- Spanish (and further languages) via the same translation table.
- More tracks and labs (content only).
- Keep the bundle lean for reliable loading; move translations to an SD data
  file if the app grows enough to risk the RAM budget.
