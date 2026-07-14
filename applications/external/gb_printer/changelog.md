# v0.5
- Add printer protocol compression support

# v0.4
- Move pinout selection to main menu
- Update to improved pinout configuration handling w/ auto-save and auto-load pinouts
- Add fine grained save options
- Shift to new naming scheme. Files still placed in dated folder, will have date in output file name(s), as well as a globally incrementing (as opposed to reset each day) 4 digit number. The number is automatically saved and loaded but there are limited protections in place to prevent file overwrite. e.g. if the file number fails to save for any reason, or the file on SD card used to track this number is removed, it is possible that saved photos may be overwritten. This issue is complicated in that, arbitrarily tall images are handled by opening and reopening an existing file; so reopening an existing image file is a valid behavior pattern. This will be dealt with in a future release to ensure there are no cases where files could get clobbered.

# v0.2
- Supports images of varying height
- Saves multiple images printed without margins as a single stacked image
- Selectable pinout for Link Cable interface

# v0.1
Initial release
