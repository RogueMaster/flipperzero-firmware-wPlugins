# RM Game Menu

## Behavior

| Area | Restored behavior |
| --- | --- |
| Game Mode, short center | Defaults to the custom Game Menu; configurable through Game Mode Keybinds |
| Normal desktop shortcut | More Actions → Game Menu opens the same menu without changing Game Mode |
| Menu contents | Games selected by the user; automatic defaults include installed game categories |
| Style | Independent `game_menu_style`; Wii by default; all 14 current RM styles supported |
| Starting game | Independent `game_start_point`, clamped safely to the available list |
| Configuration | CFW Settings → Interface → Game Menu: style, start point, item selector, add, move, remove, delete all, rebuild defaults |
| Returning from an app | Current loader releases menu metadata during app startup and rebuilds the menu when the launch queue is empty, retaining the selection |
| Missing style plugin | Falls back to the firmware's List renderer |
| Empty game list | Displays “No games found”; Back returns to the desktop |

`game_mode` selects the desktop shortcut profile. Game Mode's center action defaults to
Game Menu and can be reassigned, like the arrow shortcuts, in Settings → Desktop → Game
Mode Keybinds. Missing saved bindings fall back individually to the existing defaults.
Game Mode remains opt-in. The existing `game_menu.fal` SD resource must be installed.

## Defaults and custom lists

Defaults scan these installed directories recursively:

- `/ext/apps/Games`
- `/ext/apps/GPIO/Games`
- `/ext/apps/GPIO/VGM`

The VGM directory preserves the historical menu's inclusion of VGM games and companion tools. Discovery includes `.fap` case-insensitively, excludes hidden files/directories, `assets` directories and `.fal` plugins, and sorts paths case-insensitively. New games appear automatically while using defaults. Only installed FAPs can appear; source code alone does not install a game.

There is no static game catalog to regenerate and no dependency on `apps_assets/dab_timer/cfw_gamesmenu.default.txt`. Clock remains a game-mode shortcut, but it does not provide menu configuration.

The current file is `/int/.gamemenu_apps.txt`. A custom list contains:

```text
GamesMenuList Version 1
/ext/apps/Games/snake.fap
/ext/apps/Games/tetris.fap
```

Rebuild Menu Apps writes an automatic-defaults marker:

```text
GamesMenuList Version 1
All Games
```

A header-only custom list is intentionally empty. Blank lines and comments are ignored, duplicate paths are removed while retaining the first occurrence, and unsupported versions fall back to defaults. Missing or unreadable custom FAPs retain their paths and display a filename with a fallback icon, so reinstalling a game does not require rebuilding the selected list.

When no current file or recovery backup exists, the helper accepts the historical `/ext/.config/cfw_gamesmenu.txt` version 0 file. Reading it preserves its order. Saving edits writes the current internal file; resetting selects automatic defaults and leaves the historical SD-card file intact. This is list compatibility, not a conversion of an entire historical firmware configuration. Historical style enum value 2 meant Compact; the current RM enum value 2 means DSi. Choose the desired style in the new editor when moving from the old firmware.

Changes save when leaving CFW Settings. Writes use a temporary file, synchronization and no-overwrite renames. The previous file becomes `.bak` during replacement; an interrupted replacement can be read from that backup when the primary file is absent. A failed save keeps the edits in the running editor and reports the failure. This recovery strategy does not claim protection against arbitrary storage corruption.

## Integration

Normal center still sends `DesktopMainEventOpenMenu`. The Game Menu keybind uses a
firmware-internal loader request, independent of the global desktop mode. The loader
snapshots `games_only` when opening the menu and retains it while rebuilding after an app
exits. This allows normal-mode Game Menu shortcuts without temporarily changing
`cfw_settings.game_mode`.

The existing Game Menu plugin interface and API version 1 are unchanged. The loader loads
`/ext/apps_data/loader/plugins/game_menu.fal` only to enumerate entries, copies their data,
and unloads it before loading the menu style or launching a game. No FAL-owned callback or
pointer survives enumeration. The FAL remains optional at firmware startup; a missing or
invalid plugin displays “Update SD resources” when Game Menu is requested.

The game menu selects its style from `cfw_settings.game_menu_style`; the main menu continues
using its loader-selected style. New settings fields are appended to `CFWSettings`,
preserving prior field offsets. This restoration introduces no SDK exports or API bump;
the current 420 base uses firmware API 88.16.

Normal keybinds retain `/int/.desktop_keybinds.txt` with its eight arrow entries. Game Mode
uses `/int/.desktop_game_keybinds.txt` with ten entries, including `PressOK` and `HoldOK`.
The legacy binary normal-keybind migration retains its original four-key layout.
Game Mode `HoldLeft` is reserved for `/ext/apps/Main/dab_timer.fap`: the editor locks it,
the profile reader ignores overrides, the writer stores the fixed value, and the runtime
always uses the fixed default for that key.

Delete All Menu Apps writes an empty custom list through the existing atomic save helper.
Rebuild Menu Apps uses the existing reset helper to restore the `All Games` marker and
reload installed defaults. Both actions ask for confirmation in the editor, reset the
starting game to zero, and preserve the current editor state if saving fails.

`lib/cfw/game_menu.c` owns discovery, current/legacy list loading and save/reset behavior. CFW Settings compiles the same source through `mock_imports/mock_game_menu.c`, following the base's existing mock-import pattern. This avoids introducing SDK functions and keeps both consumers on the same configuration format. Local sorting avoids the disabled external `qsort` symbol.

The CFW game arrays load only when the Game Menu editor is first opened. The loader allocates the live game menu only when requested and releases it during app startup. The editor uses `size_t` indices and a three-value navigation row plus a full submenu selector, avoiding the VariableItem widget's 8-bit value-count limit. Actual menu size remains limited by available device RAM.

The loader now records whether an icon is allocated or a static fallback. It frees allocated FAP icons and keeps firmware icons intact. This also fixes the shared launcher cleanup case for a missing/unreadable FAP or a non-FAP path.
