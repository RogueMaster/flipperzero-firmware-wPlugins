# Changelog - Patch Notes

## Version 2.3
- Fix potential bug in copying incoming trade data to the live working struct

## Version 2.2
**New Features**
- Update to gblink v0.63 which includes saving/loading of pin configurations for the EXT link interface

## Version 2.1
**New Features**
- Add ability to reset trade state without affecting current Pokemon being configured

**Refactor**
- Move files, consolidate files where appropriate, change scene management to be more in-line with Flipper paradigms

## Version 2.0
**Bug Fixes**  
- Older MALVEKE style pinout no longer breaks OK button, MALVEKE users now able to modify traded Pokemon

**New Features**  
- Generation II support (Gold, Silver, Crystal)  
  - Ability to configure name, level, held item, moveset, EV/IV, shininess, gender, pokerus, Unown form, OT ID/name
- Custom Sprite Art for all Pokemon, all sprites are the full 56x56 px size  
- Better state synchronization during trades, able to request canceling a trade
- Exit confirmation to prevent accidentally losing configuration  
- Add main menu to select generation and pinout  
- Adjust Trade and Select views to show full sprite, with proper transparency

**Refactor**
- Sprites moved to file on SD card  
- Create accessors for generically modifying Pokemon struct data  
