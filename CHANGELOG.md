v1.4:

- Added Tag Control with reversible EPC/TID/User lock, unlock, and guarded erase.
- Added Access Keys with four persistent password slots and protected read/write verification.
- Added EPC ASCII capture/edit/write and HEX/ASCII inventory display.
- Added auto-start Inventory and two-press Back exit confirmation.
- Optimized main-menu navigation with 3x2 tiles, vertical scrolling, and wraparound.
- Optimized Tag Control flow, stable-EPC validation, and read-only TID handling.
- Optimized tag data layout, Access Keys labels, and exit dialog contrast.

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
