v0.4:
- added master PIN setup/unlock gate before vault access
- replaced plaintext active vault storage with AES-GCM encrypted `vault.pv1`
- first unlock setup migrates legacy plaintext `vault.tsv` once, then removes it after encrypted save succeeds

v0.3:
- generated passwords are checked against saved vault entries before saving to prevent duplicate generated passwords already in the vault
- updated app/about/README wording to describe PassVault plainly as a password storage, generation, and USB HID typing tool

v0.2:
- added `About / ck42x.com` menu item
- updated app description to include CK42X.com
- replaced app icon with a Flipper-compatible 10x10 monochrome simplification of the CK42X crowned bee logo from ck42x.com
- added catalog screenshot

v0.1:
- first public build
- local plaintext TSV vault in app data
- add account / username / password entries
- generated password presets
- opt-in USB HID password typing after confirmation
