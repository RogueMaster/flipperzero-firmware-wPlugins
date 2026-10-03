# Game Mode / Games Only

Enable Game Mode from the desktop Up menu or from **CFW Settings → Interface → General → Game Mode** while Game Mode is off. The desktop keeps its battery display and idle animation. The mode persists across restarts and after returning from a game.

| Input on the desktop | Game Mode action |
| --- | --- |
| Short center / OK | Open the custom Game Menu |
| Hold center / OK | Jetpack Joyride |
| Short Up | Lock menu |
| Short Down | Tetris |
| Short Left | Snake |
| Short Right | Passport |
| Hold Up | 2048 Improved |
| Hold Down | Zombiez |
| Hold Right | Doom |
| Hold Left | Clock / Dab Timer |
| Hold Back | Power-off screen |

## Configure the Game Menu

Open **CFW Settings → Interface → Game Menu** to change:

- **Menu Style:** the game menu's independent style; default Wii. All installed RM menu-style plugins are supported, with List available as a built-in fallback.
- **Start Point:** the initially selected game when opening the menu. Left/Right cycles games; OK opens the full selector.
- **Item:** choose the entry to edit. Left/Right cycles entries; OK opens the full selector.
- **Add App, Move App, Remove App:** customize the list and its order.
- **Reset Menu:** restore automatic discovery of all installed Games, GPIO/Games and GPIO/VGM FAPs.

Exit CFW Settings to save changes. Menu changes apply on the next opening or rebuild without a reboot. A customized list keeps its selected entries until reset; automatic defaults discover newly installed games on each opening.

## Exit Game Mode

Hold Left on the desktop to open Clock. Enter **Up, Up, Down, Down, Left, Right, Left, Right** in Clock to toggle Game Mode. Short center in normal mode opens the main menu.

See [RM Game Menu integration](documentation/RM_GAME_MENU.md) for configuration paths, compatibility and build details.
