## v2

## UI Overhaul

- Custom Main Menu - Canvas-based menu with per-item icons and animated selection highlights (NFC pulse, save arrow, ticket slide, info pulse, sparkle)
- Card View System - New multi-page scrollable card result display replacing plain text scroll
  - Inverted header bar with card title, animated icon, and page indicator
  - Full-width field rendering with auto line-split for long values
  - Left/Right page navigation, Up/Down scrolling with scrollbar
  - OK=Save / OK=Delete button in footer
  - Random animated icon (train, wallet, ticket, card) assigned per scan
- Scan Animation - Dolphin NFC scan screen with progressive wave animation and animated "Scanning..." text
- Supported Cards - Converted from plain text to scrollable submenu list with 20 cards by name and city
- Unsupported Card Scene - Card view UI with animated X icon, protocol/lock info, and GitHub reporting link
- Loading Screen - "Parsing card data..." popup when loading saved files (no more menu flash)

## Plugin Migrations (17/19 plugins)

All plugins migrated to the new card view system with structured field display:
Clipper, Opal, myki, ITSO, Nol, Bip!, CharlieCard, GoCard, MetroMoney, SmartRider, Troika, Two Cities, RENFE Suma 10, RENFE Regular, Intertic, T-Mobilitat, TRT

Suica and Calypso excluded (already have custom UIs).

## Custom Icons

- 13 static icons: CardGeneric, Wallet, Calendar, Ticket, Train, Check, Cross, Lock, NfcScan, Save, Info, ArrowLeft, ArrowRight
- 39 animation frames across 13 icon sets for menu and parser animations
- 3 DolphinScan frames (progressive NFC waves from original dolphin image)

## RENFE Regular Parser Rewrite (beta.3)

- Proper date decoding using LSB-first 15-bit DMY format (year+2000 | month | day)
- Card Dates page: card start date, card expiry, price, and tariff from Block 8/12
- Title / Charge page: title start (Block 13), last charge date/time and terminal ID (Block 61)
- Sale History page: sale date/time, terminal, and tariff from Block 57
- Trip history time-of-day decoding from byte5 + (byte6 & 0x07) << 8 formula
- Broader history entry detection (byte2 A6/DE + byte7 0x10) instead of only 4 hardcoded headers
- Transaction classification using byte 2 (A6=entry/check, DE=exit/transfer) and byte 0 bit 0
- Gate direction (IN/OUT) and zone display on trip pages
- Removed bogus region detection patterns that matched random data
- Cleaned up dead code and overly broad bono history scanning

## Bug Fixes (beta.2)

- Suica/FeliCa crash fix - Fixed crash when reading Suica cards with many services. The FeliCa poller was traversing all 60+ services on the card, exhausting memory. Now reads only the needed history blocks directly, avoiding the full traversal.
- Navigo/Calypso AID selection - Fixed newer Navigo cards that reject the legacy CLA 0x94 class byte. Now performs an upfront ISO 7816 SELECT APPLICATION by AID before falling back to Calypso native commands.
- Unknown card scene - Added tick event handling for card view animation on the unsupported card screen.

## Memory Safety and Stability

- Fixed null pointer dereference in main menu draw callback (model freed while view active)
- Fixed card view cleanup order - card view freed by parse scene AFTER plugin unload
- Fixed double nfc_device_alloc leak in metroflip_alloc
- Fixed storage file handle leaks in manage_keyfiles
- Fixed plugin_manager leak when plugin load fails
- Fixed ATR/T-Money fall-through to plugin load after sending WrongCard event
- Fixed unguarded plugin_on_event calls when plugin_manager is NULL
- Replaced all FuriTimer usage with view_dispatcher tick callback to prevent usagefault on app exit
- Added null guards to all draw callbacks
- Cleared view callbacks before freeing in metroflip_free to prevent dangling function pointers
- OV-Chipkaart removed from main menu (still parseable from saved files)
- Show actionable error when plugin fails to load

## Previous Fixes

- Orca changes (FatherDivine)
  - Added Orca AID variant 0xF013F2 based on user testing
  - Fixed array size from 89 to 90 to accommodate new AID entry

- Ventra/Ultralight File Loading Fix (FatherDivine)
  - Added "NTAG/Ultralight" device type string
  - Now loads any Ultralight card even without Ventra signature
  - Files saved by Metroflip now load properly

- Buffer Size Verification (FatherDivine)
  - Verified all static buffer sizes are sufficient for their data formats
  - Fixed get_navigo_service_provider() buffer from 8 to 12 bytes

- Memory Leak Fixes (FatherDivine)
  - read_calypso_data() - Fixed memory leak when data not found
  - get_country_string() - Changed from malloc to static buffer
  - Unused nfc_scanner_alloc() calls - Removed from 8 plugins
  - Transit display functions - Converted to static buffers

- Memory Safety Improvements (FatherDivine)
  - CalypsoCardData initialization - Initialize pointers to NULL for safe cleanup
  - Allocation failure handling - Added NULL checks for malloc returns
  - Cleanup on failure - Added proper cleanup path for card data on early exit
  - NULL checks in cleanup - Added check for ctx->card in calypso_on_exit

- Suica/Octopus Fix
  - Fixed broken FelicaData vs FelicaSystem struct access in suica parser
  - Restored system-code matching for Suica (0x0003) and Octopus (0x8008)

- T-Mobilitat Fix
  - Fixed crash on card read caused by stack overflow from deep scene nesting
  - ATR plugin now defers scene transition via custom event
  - Removed erroneous poller cleanup from tmobilitat_on_exit
  - Card number now displays formatted as XXX XXX XXXCC with control characters

- Calypso/Navigo Stack and Memory Fixes
  - Replaced 11 variable-length arrays with fixed-size buffers to prevent stack overflow on 2KB stack
  - Fixed 3 memory leak paths in navigo.c station lookup
  - Reduced get_token() static buffer from 512 to 64 bytes
  - Added NULL check for CalypsoContext allocation in poller callback

- Allocation Safety (intercode, opus, ravkav)
  - Added NULL checks for cascading malloc calls in all structure-building functions
  - If a nested malloc fails, parent allocations are now properly freed

- calypso_util.c
  - Added NULL checks in get_calypso_node_offset() and get_calypso_node_size()

## v1.0
- Suica Fixes
- New Cards:
- Intertic disposable ST25TB cards adding 21 French cities and companies
- T-Mobilitat can parse card number from historical bytes
- TRT
- Fix a lot of bugs relating mifare classic and stuff

## v0.9

- Fix unsupported card crash
- RENFE Suma 10 support ADDED
- GEG Connect AID added to DESfire list.
- Top Up log parsing and animations.
- 16 new rail lines, including JR lines like Chuo, Negishi, Joban, and Yamanote; and all of Tokyu's operating lines.
- Added support for parsing area codes for future expansion on non-Tokyo areas.
- Added saving function for Suica/Japan Rail IC readings.
- Various bug fixes and safer memory manegements improvements.

## v0.8

- Added 80+ card AIDs (most may not be fully parsed)
- Added more AIDs for DESFire
- Added Calypso card saving support
- Fixed DESFire parsing
- Fixed Navigo crash
- Fixed crash when opening Navigo files after exit
- Fixed Clipper timestamp epoch conversion
- Fixed Calypso file saving

## v0.7

- Fixed the stuck-in-app loop
- Added balance to Rav-Kav
- Added Suica parser
- Stylization updates

## v0.6

- Added a load mode and a save mode to store card info
- Fixed a major bug due to API symbol not existing

## v0.5

Big update!

- Custom API Added: A custom API for Metroflip has been introduced for smoother operation and better scalability
- Parsers Moved to Plugins: All parsers have been moved to individual plugins, loaded from the SD card as '.fal' files
- Scene Optimization: All scenes merged into 'metroflip_scene_parse.c' for simplification
- RAM Usage Reduced: Over 45% reduction in RAM usage
- Navigo Station List: Moved to 'apps_assets'
- Unified Calypso Parser: Thanks to DocSystem
- Rav-Kav Moved to Calypso Parser: Credit to luu176

## v0.4

- Updated Navigo parser (thanks to DocSystem)
  - Now uses a global Calypso parser with defined structures
  - Fixes BusFault and NULL pointer dereferences
- Updated all Desfire parsers (Opal, ITSO, Myki, etc.)
  - Fixed crash when pressing back button while reading
- Fix Charliecard parser

## v0.3

- Added Clipper parser (San Francisco, CA, USA)
- Added Troika parser (Moscow, Russia)
- Added Myki parser (Melbourne, VIC, Australia)
- Added Opal parser (Sydney, NSW, Australia)
- Added ITSO parser (United Kingdom)

## v0.2

- Updated Rav-Kav parsing to show more data like transaction logs
- Added Navigo parser (Paris, France)
- Bug fixes

## v0.1

- Initial release by luu176
