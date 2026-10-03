# RM Game Menu

## Behavior

| Area | Restored behavior |
| --- | --- |
| Game Mode, short center | Opens the custom Game Menu instead of launching Jetpack directly |
| Menu contents | Games selected by the user; automatic defaults include installed game categories |
| Style | Independent `game_menu_style`; Wii by default; all 14 current RM styles supported |
| Starting game | Independent `game_start_point`, clamped safely to the available list |
| Configuration | CFW Settings → Interface → Game Menu: style, start point, item selector, add, move, remove, reset |
| Returning from an app | Current loader releases menu metadata during app startup and rebuilds the menu when the launch queue is empty, retaining the selection |
| Missing style plugin | Falls back to the firmware's List renderer |
| Empty game list | Displays “No games found”; Back returns to the desktop |

`game_mode` continues to control which launcher opens. Game Mode's center action always opens the custom menu, even with defaults; no keybinding or assets must be installed to enable that route. Game Mode remains opt-in, matching the supplied base.

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

Reset writes an automatic-defaults marker:

```text
GamesMenuList Version 1
All Games
```

A header-only custom list is intentionally empty. Blank lines and comments are ignored, duplicate paths are removed while retaining the first occurrence, and unsupported versions fall back to defaults. Missing or unreadable custom FAPs retain their paths and display a filename with a fallback icon, so reinstalling a game does not require rebuilding the selected list.

When no current file or recovery backup exists, the helper accepts the historical `/ext/.config/cfw_gamesmenu.txt` version 0 file. Reading it preserves its order. Saving edits writes the current internal file; resetting selects automatic defaults and leaves the historical SD-card file intact. This is list compatibility, not a conversion of an entire historical firmware configuration. Historical style enum value 2 meant Compact; the current RM enum value 2 means DSi. Choose the desired style in the new editor when moving from the old firmware.

Changes save when leaving CFW Settings. Writes use a temporary file, synchronization and no-overwrite renames. The previous file becomes `.bak` during replacement; an interrupted replacement can be read from that backup when the primary file is absent. A failed save keeps the edits in the running editor and reports the failure. This recovery strategy does not claim protection against arbitrary storage corruption.

## Integration

The desktop sends its existing `DesktopMainEventOpenMenu` event. `loader_menu_alloc()` snapshots whether this is a game menu using `!settings_only && cfw_settings.game_mode`. The existing loader service, message types and public menu API remain in use.

The game menu selects its style from `cfw_settings.game_menu_style`; the main menu continues using its loader-selected style. New settings fields are appended to `CFWSettings`, preserving prior field offsets. The SDK export list and API version remain 88.7.

`lib/cfw/game_menu.c` owns discovery, current/legacy list loading and save/reset behavior. CFW Settings compiles the same source through `mock_imports/mock_game_menu.c`, following the base's existing mock-import pattern. This avoids introducing SDK functions and keeps both consumers on the same configuration format. Local sorting avoids the disabled external `qsort` symbol.

The CFW game arrays load only when the Game Menu editor is first opened. The loader allocates the live game menu only when requested and releases it during app startup. The editor uses `size_t` indices and a three-value navigation row plus a full submenu selector, avoiding the VariableItem widget's 8-bit value-count limit. Actual menu size remains limited by available device RAM.

The loader now records whether an icon is allocated or a static fallback. It frees allocated FAP icons and keeps firmware icons intact. This also fixes the shared launcher cleanup case for a missing/unreadable FAP or a non-FAP path.
