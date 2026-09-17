Unreleased:

- Added a standalone Tag Control tool after EPC Fuzzing, with EPC-filtered reversible lock and unlock actions for EPC, TID, and User banks.
- Added guarded erase-by-zero for writable EPC and User banks inside Tag Control.
- Added a separate Access Keys tool with four persistent Access Password slots used by write, erase, lock, and unlock operations.
- Restored the original three-row, two-column main-menu tile size with vertical scrolling.
- Prevented locking with a zero Access Password and added a zero-password recovery action for initializing the tag key.
- Required one unique EPC to remain stable before Tag Control permits tag operations.
- Replaced the oversized Tag Control `Scanning` button label with actionable `Stop` / `Scan` labels.
- Kept cleared EPC tags recoverable and visible, using a minimal non-zero EPC placeholder for future clears.
- Added Access Passwords to protected reads and verification after setting a tag access key.
- Moved Erase to the Tag Control right-button action and kept the Actions menu to three items.
- Left-aligned multi-line tag memory data and added a right-side scrollbar for content longer than three rows.
- Separated the Access Keys cursor from the active slot: Up/Down browse, center selects, and Right edits, with an aligned radio marker on the active slot.
- Added a timed two-press Back confirmation popup before exiting from the main menu.
- Added an EPC Display setting with HEX and ASCII modes for Tag Inventory.
- Added an EPC ASCII tool for capturing one tag, editing up to 12 ASCII characters, and writing a verified 96-bit EPC.
- Allowed EPC ASCII writes to target a different tag after removing the captured source tag.
- Moved TID Decoder directly before Tag Control in the main menu.
- Started inventory automatically when entering Inventory from the main menu or Startup App.
- Added wraparound main-menu navigation across each two-item row and between the top and bottom rows.
- Hid the Tag Control Erase button while the read-only TID bank is selected.
- Added an Access Key submenu for setting a key from zero or restoring the tag password to zero after banks are unlocked.
- Clarified Access Key actions with explicit `00000000` labels and shortened the reset warning to fit the display.
- Removed the crowded Access Key warning line and changed reset-to-zero into a long-press action.
- Inverted the Back-again-to-exit confirmation dialog to a black border, white background, and black text.

v1.3:

- Refined the feature menu, icons, About page, and compact UHF data layouts.
- Added automatic Radar inventory startup and resilient single-tag TID decoding.
- Added byte-grouped TID/EPC displays and verified EPC fuzzing sequence advancement.
- Added direct CSV filename prompts with collision-free date-based names.
- Updated project screenshots for the redesigned tools.

v1.2:

- Added a six-item feature menu after reader detection, with separate Radar and Inventory entries.
- Added automatic single-tag TID decoding with scrollable MDID, tag model, and raw TID details.
- Added EPC sequence generation and explicit write/verify controls.
- Added persistent sound, 0-20 dBm RF output power, and startup-tool settings.
- Updated the About page for MTools Tec / MTCK.

v1.1:

- Added tag memory actions for reading EPC, TID, and User Data.
- Added verified EPC, TID, and User Data writes for compatible changeable tags.

v1.0:

- Added real-time UHF EPC inventory over the GPIO UART bridge.
- Added counter, radar, paged tag list, tag details, sound feedback, and CSV export.
- Added support for valid EPC values regardless of prefix, up to 48 bytes.
- Added reader temperature and output-power telemetry on the radar screen.
- Added reader startup recovery, hardware reset, and continuous-inventory renewal.
