# Flipper Animation Switcher

A Flipper Zero application for creating, managing, and switching **animation playlists** - template manifest.txt files that control which background animations play on your Flipper. Supports up to 128 animations per playlist.

## Menus

- **Create Playlist** - Select animations, optionally fine-tune per-animation settings, then save as a named playlist.
- **Choose Playlist** - Apply a saved playlist (overwrites manifest.txt).
- **Backup Playlist** - Snapshot the active animation set (/ext/dolphin/manifest.txt) as a named playlist.
- **Delete Playlist** - Remove any saved playlist.
- **About / Help** - App info and help section.

## Controls

**Main Menu**
- Up / Down - Navigate options
- OK - Select option
- Back - Exit app

**Create Playlist -- Animation List**
- Up / Down - Navigate animations
- OK (short) - Toggle checkbox
- OK (long) - Edit per-animation settings
- Right - Proceed to name entry (requires 1+ selected)

**Create Playlist -- Animation Settings**
- Up / Down - Navigate settings
- Left / Right - Adjust value
- Back - Return to animation list

**Choose / Delete Playlist**
- Up / Down - Navigate playlists
- OK (short) - Apply playlist / Confirm delete
- OK (long) - Preview playlist animations
- Back - Return to main menu

## Default Animation Values

When an animation is added without customising its settings, these defaults are used:

- Min Butthurt: 0
- Max Butthurt: 14
- Min Level: 1
- Max Level: 30
- Weight: 3

## File Locations

- **/ext/dolphin/** - Animation folders (read-only by this app)
- **/ext/dolphin/manifest.txt** - Active animation manifest (overwritten on Apply)
- **/ext/dolphin/manifest.txt.bak** - Backup of the previous manifest, written automatically before each Apply
- **/ext/apps_data/animation_switcher/** - Saved playlist .txt files

## Building

This app targets the **official Flipper Zero firmware**. Most custom firmware builds should also be supported.

Install and run ufbt - instructions on its official [GitHub page](https://github.com/flipperdevices/flipperzero-ufbt).

## Roadmap

- Animation previewer screen
- Increase maximum animation count beyond 128
