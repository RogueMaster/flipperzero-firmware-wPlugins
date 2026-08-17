v0.4.6:
- removed the 20-entry password vault cap so saved accounts grow until Flipper memory or SD write fails
- existing encrypted `vault.pv1` format is unchanged; larger vaults use the same AES-GCM file
- FIDO2 credential storage remains separately limited to 20 records

v0.4.5:
- added editing for saved Name, Username, and Password fields
- added Keep Existing, Generate Password, and Enter Custom choices during editing
- pre-fills current values and requires confirmation before encrypted save
- restores the original entry in memory if the encrypted save fails
- added an explicit `macOS Keyboard Setup` action for Apple's Keyboard Setup Assistant
- sends the standard ANSI identification positions (`Z`, then `/`) only when the user presses Send
- keeps the normal Flipper USB HID identity instead of spoofing an Apple VID/PID
- keeps HID active after setup so repeated injections do not re-enumerate the keyboard
- restores the previous USB mode only when PassVault exits or setup is cancelled
- leaves the existing Windows/Linux password injection path unchanged

v0.4.3:
- added a clean-room FIDO 2.0 security-key runtime with CTAPHID and CTAP2
- added non-resident ES256 WebAuthn credentials with physical Approve/Deny user presence
- stores up to 20 FIDO credentials in a separate AES-GCM encrypted `fido2.pv1` store
- added credential persistence, allow-list matching, signature counters, cancellation, and USB restoration on exit
- verified Chromium registration and authentication, restart persistence, ES256 signatures, and physical USB lifecycle behavior
- improved new-entry navigation so saved entries open on Inject and return there after typing

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
