# Amiibo Emulator

Flipper Zero app that emulates NTAG215 dumps and can present a new tag UID each time emulation starts or Next ID is pressed.

The original `.bin` keeps its UID until you press Save. A new UID is signed in memory, the same way a Joysfusion / Pixl.js unit does it. Nintendo's key file is not included.

## Install

Build with the Flipper SDK:

```
ufbt
ufbt launch
```

Copy `dist/flipper_amiibo.fap` to `SD Card/apps/NFC/` if you are installing it by hand.

## SD card

```
apps_data/flipper_amiibo/key_retail.bin
apps_data/flipper_amiibo/db/
```

`key_retail.bin` is 160 bytes: the 80-byte data key (`unfixed-info.bin`) followed by the 80-byte tag key (`locked-secret.bin`). The app also checks `/ext/nfc/assets/amiibo_keys.bin` and `/ext/nfc/amiibo_keys.bin`.

Put `.bin` dumps (520, 540 or 572 bytes) or complete NTAG215 Flipper `.nfc` files under `db/`, in series folders if you want. A folder named `ZE` always uses a new UID. A folder named `SSB` always keeps the UID stored in the file.

The browser can also select supported dumps elsewhere on the SD card. Use backups of your own tags.

## On device

- Browse dumps, then Emulate.
- Auto UID, when on, signs a new 7-byte UID before the tag is presented. Press Next ID on the emulate screen for another serial without leaving the screen.
- New UID does the same thing once, without leaving the detail screen.
- Save writes the current UID and any pages the console wrote back to the same file, preserving the BIN length and optional 32-byte suffix or the NFC metadata
- Saving stages and verifies a temporary file before replacing the original, with rollback on rename failure. If interrupted, keep the `.amiibo.bak` original and `.amiibo.tmp` candidate beside the dump until you recover the intended file
- Changing a UID re-signs the Amiibo data, not the tag manufacturer signature

Games that remember the tag serial treat each new UID as another figure. Games that remember the character still apply that character's own limit.
