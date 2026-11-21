# RF Studio

Turn the Sub-GHz remotes around your home into virtual remotes on your Flipper Zero.

Create a remote, choose **Add Button**, press the button on your original remote, and give it a name. Repeat for every button. Each remote can hold as many buttons as you need, shown as a scrolling list.

## Features

- **Guided capture**: detects the frequency automatically, decodes with every protocol your firmware supports, and keeps a raw recording when nothing decodes
- **Reliable codes**: a code is saved only after it is received twice, and you are warned when a new button has the same code as an existing one
- **Signal Library**: import *.sub* files, rename, test, export back to the Sub-GHz folder, and reuse one signal on many buttons
- **Remote library**: search, favorites, categories, sorting, duplicate, export and import
- **Firmware-aware**: frequencies, modulations and transmit permission come from your installed firmware, with support for internal and external CC1101 radios
- **Safe by design**: rolling codes are identified but never replayed, and raw recordings are marked untested until you confirm they work

## Controls

- **Up / Down**: choose a button
- **Left / Right**: page through long remotes
- **OK**: send
- **Hold OK**: remote and button options

Source code and documentation: [GitHub](https://github.com/cruzer45/rf-studio)
