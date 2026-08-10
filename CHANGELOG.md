## Main changes
- Current API: 88.2
* NFC: **Fix reading an EMV card with malformed TLV lengths crashing or overflowing buffers** - the poller trusted the card's own length bytes: some tags aborted the firmware through `furi_check`, others were copied straight into fixed-size fields (AID, application name/label, cardholder name, track 1/2), and the PAN/track-2 loops ran past their arrays. Lengths are now bounded against the destination, the PDOL is capped at what the poller can transmit, and rejected tags are logged (by @Endika | PR #1048)
* NFC: **Fix a crafted EMV `.nfc` file corrupting the heap on load** - `emv_load()` used the file's own lengths as write sizes: three unbounded `strcpy`s into the cardholder name, application name and label, and `PAN length`/`AID length` read as 32-bit then written into 10- and 16-byte fields; an oversized `PAN length` also walked the card-number render loops past the array. Saving a card no longer writes a garbage `PIN try counter` either (by @mishamyte | PR #1056 | Fixes #1055)
* NFC: **Fix "Unlock with Dictionary" destroying a saved MIFARE Classic dump** - pressing Skip while no card was on the reader adopted the dictionary poller's still-empty data as the loaded card, so the results screen offered to save a blank dump over the file. Saving under a different name did not help either, because renaming on save deleted the previously loaded file first (fixed separately below). The MIFARE Plus dictionary attack could lose data the same way and now merges its result instead of replacing (by @mishamyte | Fixes #1063)
* NFC: **Fix "Update from Initial Card" dropping sectors from a MIFARE Classic dump** - the refresh replaced the dump with whatever that pass re-read instead of merging into it, so a sector that failed to authenticate this time lost both its key and its blocks. Recoverable through "Restore to Original State", but silent (by @mishamyte | Fixes #1064)
* NFC, LF RFID, iButton: **Renaming a saved file no longer deletes it before the replacement is written** - all three apps unlinked the loaded file first, so a save that then failed (full SD, card pulled) left the user with neither copy. The new file is written first and the old one dropped only once it is safely on disk, a stale NFC shadow file can no longer override a fresh save, and a failed save no longer leaves the app pointing at a file that was never created. Note this protects renames; re-saving under the same name still writes over the only copy (by @mishamyte | Fixes #1065)
* Apps: Build tag (**9aug2026**) - **Check out more Apps updates and fixes by following** [this link](https://github.com/xMasterX/all-the-plugins/commits/dev)
## Other changes
* OFW PR 4361: fix HID limits to support international keyboards and add JP keyboard layout (by @d3npa)
* OFW: CCID: move the debug app out of the firmware repository
* JS: **JS Runner moved out of the firmware into an external app** (`apps/assets/js_app.fap`) to free up internal flash and RAM; the `js` CLI command is now a CLI plugin and the JS examples ship with the extra resources
* Docs: **How to build FAPs on Android with Termux** - `ufbt` on a stock phone, no PC/proot/VM (by @CamsShaft | Closes #1028)
* HID: **Mouse Jiggler Stealth now shows which buttons change the intervals** - Up/Down (Min) and Left/Right (Max) already worked but nothing on screen said so; the arrows appear next to each row while the jiggler is stopped, and only for presses that would actually change the value (by @sequesters | PR #1020)
<br><br>

----

### Added:
- Apps:
  - Games: Checkers (by @H4W9)
  - GPIO: CAN Commander (by @MatthewKuKanich)
  - NFC:
    - ISO 15693-3 NFC Writer (by @ch4istO)
    - UL-C Bruteforce (by @noproto)
    - UL-C Relay (by @noproto)
    - ULCFkey (by @noproto)
  - Sub-GHz: ProtoPirate (by @RocketGod-git & @xMasterX & @zero-mega et al.)
  - Tools: Flipper Wedge (by @dangerous-tac0s)
- Sub-GHz:
  - UL: Add support for 42+ Keeloq based systems (with partial Add Manually support) (by @zero-mega, @xMasterX, ARF Team)
  - UL: Cardin S449 protocol full support (64bit keeloq) (with Add manually, and all button codes) (use FSK12K modulation to read the remote) (by @xMasterX & @zero-mega)
  - UL: Beninca ARC AES128 protocol full support (128bit dynamic) (with Add manually, and 2 button codes) (by @xMasterX & @zero-mega)
  - UL: Jarolift protocol full support (72bit dynamic) (with Add manually, and all button codes) (by @xMasterX & d82k & Steffen (bastelbudenbuben de))
  - UL: Treadmill37 protocol support (37bit static) (by @xMasterX)
  - UL: Ditec GOL4 protocol (with programming mode, button switch, add manually) (by @xMasterX & @zero-mega)
  - UL: KeyFinder protocol (24bit static) (by @xMasterX & @mishamyte)
  - UL: Nord ICE protocol (33 bits, Static) (by @xMasterX)
  - UL: Allstar Firefly 318ALD31K protocol (18 bits, Static) (by @jlaughter)
  - UL: Add CAME TOP44FGN support in CAME TWEE protocol (by @xMasterX)
  - UL: New modulation FSK with 12KHz deviation (by @xMasterX)
  - UL: KingGates Stylo 4k Add manually and button switch support and refactoring of encoder (by @xMasterX)
  - UL: Stilmatic (R-Tech) 12bit discr. fix & button 9 support (two buttons hold simulation) (mapped on arrow keys) (by @xMasterX)
  - UL: Sommer last button code 0x6 support (mapped on arrow keys) (by @xMasterX)
  - UL: V2 Phoenix (Phox) added 2 counter modes support (docs updated) (by @xMasterX)
  - UL: Add 390MHz and 430.5MHz to default hopper list (6 elements like in OFW) (works well with Hopper RSSI level set for your enviroment) (by @xMasterX)
  - UL: Add signals button editor and real remote simulation (full signal transmit with just one click) (by @Dmitry422)
  - UL: KeeLoq add counter mode 7 (sends 7 signals increasing counter with 0x3333 steps) - may bypass counter on some receivers! (by @xMasterX)
  - UL: TX Power setting (by @LeeroysHub)
  - UL: Somfy Keytis button switch and Add Manually support (by @xMasterX)
  - UL: Genius Echo/Bravo add 2 buttons hold simulation (0xB btn code) (by @xMasterX)
  - UL: Add all 0x0s and all 0xFs KeeLoq MF codes for normal and simple learning (by @xMasterX)
  - UL: Better support for CAME Atomo type remotes (TOPD4REN) (decode + button codes) (by @xMasterX & Roman)
- NFC:
  - Add Mifare Ultralight C Write Support (#524 by @haw8411)
  - UL: Add ISO15693-3 and SLIX write-back support (by @DoniyorI)
  - UL: Add new parsers SZPPK, SKPPK and SevPPK, upgrade Plantain parser, fix TwoCities parser (by @mxcdoam)
- OFW PR 4343: RFID: Add Indala 224-bit (long format) protocol support (by @kuzaxak)
- UL: JS: Add IR capabilities to the JS engine (by @LuisMayo)
- FBT: Allow apps to specify custom cflags (by @WillyJL)
- UL: Docs: Add [full list of supported SubGHz protocols](https://github.com/Next-Flip/Momentum-Firmware/blob/dev/documentation/SubGHzSupportedSystems.md) and their frequencies/modulations that can be used for reading remotes (by @xMasterX)
UL: API: Add `canvas_get_buffer`, `canvas_get_buffer_size` to public API (by @xMasterX)

### Updated:
- Apps:
  - XERO: MFKey: 4.1 with 40% faster key recovery, improved memory efficiency (by @dchristle), new SEN dictionary for 10x faster recovery (by @noproto)
  - UL: Update Sub-GHz apps for FM12K modulation (by @xMasterX)
  - UL: USB/BT Remote: PTT improvements (by @hryamzik)
  - Ami Tool: Remove copyrighted names, write card fully using low level poller (by @Firefox2100)
  - CAN Tools: Parity with DBC format, support importing DBC files (by @MatthewKuKanich)
  - ESP Flasher: Bump Marauder 1.12.1 (by @justcallmekoko) FlipperHTTP 2.1.1 (by @jblanked) Blackmagic 0.1.2-rc (by @hedger), advanced flashing mode (by @H4W9)
  - ESP32 WiFi Marauder: Marauder 1.12.0 support (by @pfefferle), Marauder 1.10.0 support (by @justcallmekoko), Marauder 1.9.0 support (by @H4W9)
  - FlipLibrary: Added Fahrenheit, current weather, and wind speed/direction (by @H4W9)
  - FlipSocial: Autocomplete, keyboard improvements, explore and profile view enhancements, bugfixes (by @jblanked)
  - FlipWeather: Added Fahrenheit, current weather, and wind speed/direction, custom location option (by @H4W9)
  - Free Roam: Minimap, code optimization, ux improvements, online mode, option to hide/show player (by @jblanked)
  - Flipper Blackhat: TUI command (by @o7-machinehum)
  - Geometry Dash: Major refactor, bugfixes and performance improvements, rename from Geometry Flip (by @gooseprjkt)
  - GhostESP: Many bugfixes, added Flock Detection, Flock List, and Stop Flock Detection commands (by @jaylikesbunda)
  - HC-SR04 Distance Sensor: Option to change measure units (by @Tyl3rA)
  - IconEdit: Save/Send animations, settings tab with canvas scale and cursor guides, bugfixes (by @rdefeo)
  - INA2xx INA Meter: Fixed application freezing when the sensor is not connected (by @cepetr)
  - KT0803 FM Transmitter: Many bugfixes (by @nixvio64)
  - Lidar Emulator: Settings menu with external output and 5V power on support, separate thread for timing the infrared signal (by @ANTodorov)
  - Lidar Emulator: Support external IR boards (by @ANTodorov)
  - Metroflip: Bugfixes (by @baalpeteor2)
  - NearbyFiles: GPS Baudrate selection, Ability to add GPS to file (by @xMasterX)
  - NFC Login: Code refactor, bugfixes, renamed from NFC PC Login, restore BLE profile on app exit (by @Play2BReal)
  - Passy: Bugfixes, Save advanced files as well (by @bettse)
  - Picopass: Option to Create credential without a card (by @redteamlife), info screen improvements, allow Emulation of NRMAC dumped cards, Clean MKF option (by @bettse)
  - Seader: SAM ATR3 support, better IFSC/IFSD handling, various improvements (by @bettse), reverse engineered U90 packet, SNMP support, multi-card selection, memory optimization, refactoring (by @cindersocket)
  - Seos Compatible: Seos write support, various improvements (by @aaronjamt), support switching key sets (by @pcunning), code refactoring, various bugfixes (by @bettse)
  - Sub-GHz Scheduler: Added new interval times, bugfixes and improvements (by @shalebridge)
  - Tetris: Various bugfixes (by @Bricktech2000)
  - Unitemp: Numerous improvements from @MLAB-project fork (by @MLAB-project), Many changes, new sensors and code refactoring in v2.0 (by @quen0n)
  - Weebo: Cycle between .nfc files in the same directory (by @sethgregory), new figure IDs (by @Brickleberry19), bugfixes and refactoring (by @bettse)
  - XRemote: Add dolphin xp and crash bugfix (by @teohumeau)
  - Yatzee: Fix Full House calculation, improve code readability (by @zessx)
- Sub-GHz:
  - Sub-GHz app is now external on SD card, like on OFW (by @WillyJL)
    - This was done to free some space on flash, which is always running out
    - It means RAM consumption while using the Sub-GHz app has increased
    - Usability remains mostly unaffected, our testing showed it can receive up to ~50 signals normally and ~10 signals with qFlipper / Flipper Mobile App connected
    - You may experience some "out of memory" messages, this is inevitable as RAM eventually fragments and needs to be rearraged, simply resume as normal after the reboot
  - Add Manually menu transparently refactored into a "separate app" for lower RAM usage, it is still accessed as before from Sub-GHz app (by @WillyJL)
  - UL: Counter editor refactoring (by @Dmitry422)
  - UL: Alutech AT-4N & Nice Flor S turbo speedup (by @Dmitry422)
  - UL: Sommer fm2 in Add manually now uses FM12K modulation (Sommer without fm2 tag uses FM476) (try this if regular option doesn't work for you) (by @xMasterX)
  - UL: Replaced Cars ignore option with Revers RB2 protocol ignore option (by @xMasterX)
  - UL: Improve docs on low level code (by @Dmitry422)
  - UL: KeeLoq display decrypted `Hop` instead of showing encrypted as is (encrypted non byte reversed hop is still displayed in `Key` field) (by @xMasterX)
  - UL: BFT KeeLoq try decoding with zero seed too (by @xMasterX)
  - UL: KeeLoq display BFT programming mode TX (when arrow button is held) (by @xMasterX)
  - UL: Signal Settings Improvements (by @Dmitry422)
  - UL: KeeLoq change delta size (by @xMasterX)
- Archive: Support opening and pinning ProtoPirate files from Archive (#510 by @LeeroysHub)
- Display: Extend lcd contrast range to -10 to +18 (by @ShaTie)
- OFW PR 4297: RFID: Make FDX-B readout more descriptive (by @snowsign)
- OFW PR 4320: API: Make `view_port_send_to_back()` public (by @loftyinclination)

### Fixed:
- Sub-GHz:
  - Fix display of memory full (by @WillyJL)
  - UL: Fixed button mapping for FAAC RC/XT (by @xMasterX)
  - UL: Possible Sommer timings fix (by @xMasterX)
  - UL: Various fixes and cleanup (by @xMasterX)
  - UL: Nice Flor S remove extra uint64 variable (by @xMasterX)
  - UL: Fix Alutech AT4N false positives (by @xMasterX)
  - UL: Fix documentation link for HT12A protocol (by @carlogrisetti)
  - UL: BFT Mitto fix decode bug (seed was not resetting after one successful decode) (by @xMasterX)
  - UL: KeeLoq fix display of AN-Motors and HCS101 keys (by @xMasterX)
  - UL: Fix CAME TWEE repeats count for button click (by @xMasterX)
  - UL: Fix small logic error in KingGatesStylo4k protocol (by @xMasterX & @X-Stuff)
  - UL: Improve Nice FLO decoding (by @xMasterX & Roman)
  - UL: Fix duplicate code in RAW protocol (by @xMasterX)
  - UL: Fix FAAC SLH wrong decode/encode, apply little code cleanup (by @xMasterX)
- NFC:
  - Fix sending 32+ byte ISO 15693-3 commands (by @WillyJL)
  - Fixes to `READ_MULTI` and `GET_BLOCK_SECURITY` commands in ISO 15693-3 emulation (#501 by @WillyJL & aaronjamt)
  - Fix Type 4 Tag read edge case for cards not supporting specific EF in DF select mode (by @WillyJL)
  - Fix CLI with NTAG4xx and Type 4 Tag support (by @WillyJL)
  - UL: Fix LED not blinking at SLIX unlock (by @xMasterX)
  - UL: Fix "MIR" and other EMV cards crash on Read (by @Dmitry422)
  - OFW PR 4362: Fix BusFault in Write to Initial Card (by @akrylysov)
  - OFW PR 4369: Fix stack buffer overflows in MFUL FAST_READ and DESFire file settings parsers (by @qp-x-qp)
- uFBT: Fix .clangd config for IDEs besides VSCode (by @WillyJL)
- UL: Settings: Storage settings exit scenes properly if used via favourites (by @xMasterX)
- UL: UI: Some small changes (by @xMasterX)
- OFW PR 4364: JS: Address utf8 support when uploading JavaScript application (by @bekindpleaserewind)
- OFW PR 4338: USB: Fix USB HID keyboard LED state reporting (by @Caballosanex)

### Removed:
- Sub-GHz:
  - Removed Starline, ScherKhan and Kia protocols from main Sub-GHz app, they can be decoded with `Apps > Sub-GHz > ProtoPirate` external app
  - Disabled X10 and Hormann Bisecur protocols due to flash space constraints and very limited usefulness, Momentum now has the same protocol list as Unleashed
- Desktop: Removed winter holidays anims
