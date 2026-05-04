# Changelog

All notable changes to the Flipper Suite project are documented in this file.

Format: grouped by date, categorized as **fix**, **feat**, **refactor**, **chore**, or **docs**.

---

## 2026-05-04

### fix
- **subghz_jammer**: Added `furi_assert(worker)` after malloc in `jammer_worker_alloc` and `furi_assert(app->worker)` after `jammer_worker_alloc()` in `jammer_app_alloc` — ensures clean crash message on OOM rather than NULL deref deeper in init. Last unchecked worker allocation in the repo.

### docs
- **subghz_jammer**: Full re-trace review of all ~730 lines. CC1101 device init error paths, scan loop RSSI/window/threshold/consecutive/worst-freq logic, hysteresis behavior, mutex discipline (50ms worker timeout, FuriWaitForever in settings), short-circuit index protection, notification fire outside mutex, volatile on `running`, timer daemon stack safety (~58B), settings callbacks with SDK-clamped indices, 2 views lifecycle, teardown ordering (timer→worker→views→dispatcher→state→records), and back-button exit all confirmed correct.

---

## 2026-05-03

### fix
- **evil_ble**: Removed dead `devices[EVIL_BLE_MAX_DEVICES]` and `device_count` fields from EvilBleApp struct — scanner manages its own internal device array via mutex-protected accessors; the duplicate fields were never referenced, wasting ~2852 bytes of heap.

### docs
- **evil_ble**: Full re-trace review of all ~1150 lines. Buffer sizes (MAC 18B exact, status_buf 256B, device_labels 64B, clone_menu_label 48B, adv payload 31B), view lifecycle (3 views: 2 Submenus + TextBox), UART ISR/worker/DMB pipeline, scanner mutex discipline, extra_beacon clone engine config/start/stop, teardown ordering (beacon→scanner→UART→views→dispatcher→objects), back-button handling, and stack safety (main ~500/4096, UART ~630/2048) all confirmed correct.

---

## 2026-05-02

### docs
- **rayhunter_client**: Full re-trace review — no bugs found. All 1190 lines verified clean across rayhunter_uart.c, rayhunter_worker.c, and rayhunter.c. Buffer sizes (status_row[72], counters[32] exact-fit, footer[48], alert_msg[128], status_buf[64]), view lifecycle (3 views), ViewModelTypeLocking, teardown ordering, UART ISR/worker pipeline, threat keyword parsing, notification de-duplication, settings callbacks, timer daemon stack safety, and back-button handling all confirmed correct. Prior fixes (NULL context guard, volatile connected) verified present.

---

## 2026-05-01

### docs
- **ble_scanner**: Full re-trace review — no bugs found. All ~1300 lines verified clean. Buffer sizes, view lifecycle, mutex usage, volatile annotations, stack safety, teardown ordering, and back-button handling all confirmed correct. Prior fixes (NULL context guard, volatile scanning/connected) verified present.

---

## 2026-04-30

### fix
- **ccid_emulator**: Fix APDU monitor auto-scroll overriding manual scroll during active emulation. The 200ms refresh timer unconditionally snapped `scroll_offset` to the bottom whenever a new APDU arrived, making the Up/Down input handlers non-functional while APDUs were flowing. Added `auto_scroll` flag to `ApduMonitorModel` — disabled on Up press, re-enabled when Down reaches the bottom, reset when emulation starts. The timer now only updates scroll position when `auto_scroll` is true.

---

## 2026-04-29

### fix
- **spi_flash_dump**: Move `settings_return_view` declaration before first use. The file-scope static was declared at line 618 of `spi_flash_dump.c` but referenced at lines 480 and 557 — a C forward-reference error that prevents compilation. Moved the declaration to before `wiring_guide_input_cb` where it is first used.

---

## 2026-04-28

### fix
- **flipperpwn**: Fix `IF_CONNECTED` skip not tracking nested `IF` blocks for depth. The false-branch skip loop in `payload_engine.c` only incremented the depth counter for nested `IF_CONNECTED` but not for regular `IF $VAR == value` blocks. If a module nested `IF...END_IF` inside `IF_CONNECTED`, the skip would stop at the inner `END_IF` and execute commands that should have been skipped when the ESP32 is absent. Added `strncmp(st, "IF ", 3)` to the depth-increment check, matching the pattern already used by the `IF` and `ELSE` skip handlers.

---

## 2026-04-27

### fix
- **subghz_spectrum**: Stop CC1101 worker when navigating back to band select. The radio kept sweeping while the user browsed the band menu, wasting power and holding the radio. Added `view_set_exit_callback` on the spectrum view to stop the worker thread on view exit.

### chore
- **subghz_spectrum**: Remove unused `SPECTRUM_DEFAULT_STEP_KHZ` constant from `spectrum_types.h`. `step_values[]` array is used instead; the define was never referenced.

### fix (uart_sniff — idle-time review)
- **uart_sniff**: Fix ring buffer read returning oldest data instead of newest. `uart_sniff_worker_read` started from `(ring_head - ring_fill)` (oldest byte). Once the 4KB ring had more than 256 bytes, the display showed stale data from the beginning of the capture instead of the most recent bytes. The format function's address calculation (total - got) assumed newest data, producing incorrect hex addresses. Fixed to start from `(ring_head - len)`.
- **uart_sniff**: Add `volatile` to `sniffing` field in `UartSniffApp` for cross-thread visibility. The GUI thread writes the flag and the refresh timer daemon reads it via `uart_sniff_refresh_cb`.

---

## 2026-04-26

### fix
- **hid_exfil**: Fix macOS cleanup leaving session history in `~/.zsh_sessions/`. macOS Terminal.app's session-save mechanism (`/etc/zshrc_Apple_Terminal`) writes per-session history via a `precmd` hook independently of `HISTFILE`. Cleanup now removes `$SHELL_SESSION_FILE` and unsets both `HISTFILE` and `SHELL_SESSION_FILE` so no payload commands persist on disk after exit.
- **hid_exfil**: Add `furi_assert(app->worker)` after `hid_exfil_worker_alloc()` in `hid_exfil_app_alloc` — return value was unchecked, consistent with cross-app malloc assert pattern.

---

## 2026-04-25

### fix
- **badusb_pro**: Clamp MOUSE_SCROLL value to int8_t range in parser. The raw `atoi` result was stored in `int_value` and the executor cast to `int8_t`, silently wrapping values outside [-128, 127] — e.g. `MOUSE_SCROLL 200` scrolled as -56 (wrong direction). Now uses `strtol` with `INT8_MIN`/`INT8_MAX` clamping, matching the existing MOUSE_MOVE pattern.

---

## 2026-04-24

### fix
- **nfc_fuzzer**: Fix worker_running race condition — `app->worker_running = true` was set after `nfc_fuzzer_worker_start()` which already starts the thread. If the worker completed before the assignment, `done_callback` would set it false, then the GUI thread would overwrite to true, leaving the flag stuck. Moved assignment before the start call.
- **nfc_fuzzer**: Fix RATS boundary comment in `nfc_fuzzer_profiles.c` — said 8 PPS cases and 20 total but code correctly computes 6 PPS cases and 18 total.

---

## 2026-04-19

### fix
- **flipperpwn**: Fix use-after-free race in `fpwn_wifi_views_free` teardown. `wifi_status_text` and `wifi_status_mutex` were freed before the marauder log callback was deregistered. If ESP32 sent data during app exit, the UART worker would invoke `fpwn_wifi_rx_callback` which acquires the freed mutex and appends to the freed string. Fix: deregister log callback via `fpwn_marauder_set_log_callback(NULL)` before freeing the string and mutex.
- **ble_scanner**: Add volatile to `connected` field in BleUart struct for cross-thread visibility (UART worker writes on first data received, refresh timer reads via `ble_uart_is_connected()`). Same class of fix as rogue_ap_detector, evil_ble, rayhunter_client, and flipperpwn UART layers.

---

## 2026-04-18

### fix
- **rogue_ap_detector**: Add volatile to `scanning` field in RogueApWorker struct for cross-thread visibility (GUI thread writes via start/stop, timer daemon reads via `rogue_ap_worker_is_scanning()`). Same class of fix applied to ble_scanner, evil_ble, nfc_fuzzer, badusb_pro.
- **rogue_ap_detector**: Add volatile to `connected` field in RogueUart struct for cross-thread visibility (UART worker writes on first data received, API reads via `rogue_uart_is_connected()`). Consistent with evil_ble, rayhunter_client, flipperpwn UART layers.

### refactor
- **rogue_ap_detector**: Remove dead `summary[48]` field and `ROGUE_SCAN_SUMMARY_LEN` constant from RogueScanModel. The field was declared in the view model struct but never written or read — wastes 48 bytes per model instance.

### docs
- **README**: Add Applications section entries for 6 FAPs that were previously undocumented: SubGHz Jammer Detector, UART Sniff, BLE Scanner, Evil BLE, Rogue AP Detector, Rayhunter Client. All 13 FAPs now have feature descriptions in the README.

---

## 2026-04-17

### fix
- **ccid_emulator**: Fix APDU monitor auto-scroll not showing newest entries. `APDU_MON_MAX_VISIBLE` was 6 but only ~3 entries fit on the 128×64 display (each entry = 2 lines at 10px, content area = 52px). Auto-scroll and manual scroll could never reach the most recent APDU exchanges in a full ring buffer. Changed to 3.
- **ccid_emulator**: Fix TLV length fields in embedded test_card sample. MasterFile SELECT response: 6F length 0x19→0x18 (24 bytes actual), A5 length 0x0E→0x0D (13 bytes actual). PSE SELECT response: 6F length 0x1E→0x1C (28 bytes actual), A5 length 0x0C→0x0A (10 bytes actual). Each overstated the BER-TLV content size by 1-2 bytes, causing standards-compliant host parsers to reject or misparse the FCI template.

---

## 2026-04-16

### fix
- **spi_flash_dump**: Move CRC32 computation from FreeRTOS timer daemon to worker thread. `crc32_calc_file` was re-reading the entire dump file from SD card inside `worker_poll_timer_cb`, blocking all system timers for 8-32 seconds on large chips (e.g., W25Q256JV 32 MB) and freezing the UI at ~100% progress. Now computed in the worker thread right after `chip_read` succeeds, with new `spi_worker_get_crc32()`/`spi_worker_has_crc32()` accessors for the timer callback to retrieve results.
- **spi_flash_dump**: Fix Settings Back always returning to WiringGuide regardless of entry point. Settings can be entered from WiringGuide (Right) or ChipInfo (Right), but Back always went to WiringGuide. Now tracks which view opened Settings via a file-scope static variable and returns to the correct view.

### docs
- **spi_flash_dump**: Fix hex_viewer.h header comment — said "Displays 8 bytes per row" but `BYTES_PER_ROW` is 4 (128px display only fits 4 hex bytes + ASCII per line). Updated example to match actual layout.
- **spi_flash_dump**: Correct JEDEC ID database count comment from "30 common SPI NOR flash parts" to "32" in both spi_flash_dump.h and spi_worker.c.

---

## 2026-04-15

### fix
- **hid_exfil**: Fix macOS cleanup leaving payload commands in zsh history. `history -p` is a csh/tcsh command that does nothing in zsh (macOS default since Catalina). After `rm -f ~/.zsh_history`, `exit` caused zsh to rewrite its in-memory history (including all payload commands) to a new `~/.zsh_history`. Replaced with `unset HISTFILE` so zsh skips writing history on exit.

### docs
- **README**: Fix CCID emulator SD card paths — `/ext/apps_data/ccid_emulator/` → `/ext/ccid_emulator/` in 3 locations (description, sample files, FAQ) to match actual code paths
- **README**: Update NFC Fuzzer from "5 Fuzzing Profiles, NFC-A" to "11 Fuzzing Profiles, Multi-Protocol" with 4 fuzz strategies — matches current code

### chore
- Add `check_dist.sh` — verifies `dist/` has a `.fap` for each of the 13 apps and warns when any are stale (source newer than pre-built binary)

---

## 2026-04-14

### fix
- **badusb_pro**: Mark `worker_running` field as `volatile` for correct cross-thread visibility between worker thread (writes `false` on completion) and GUI thread (reads in `app_free` and `start_script_execution`). Same class as nfc_fuzzer, flipperpwn, evil_ble, rayhunter_client, ble_scanner volatile fixes.

---

## 2026-04-13

### fix
- **nfc_fuzzer**: Add `furi_assert` after `malloc` in `nfc_fuzzer_app_alloc` for app struct and results array — prevents NULL dereference on OOM (same pattern as ccid_emulator, ble_scanner, hid_exfil fixes)
- **nfc_fuzzer**: Mark `worker_running` field as `volatile` for correct cross-thread visibility between worker done callback and GUI back-event handler (same class as flipperpwn, evil_ble, rayhunter_client, ble_scanner fixes)
- **nfc_fuzzer**: Fix misleading comment in anomaly notification — LED blink is blue (`sequence_blink_blue_100`), not red
- **subghz_jammer**: Add `furi_assert` after `malloc` in `jammer_app_alloc` for app struct and JammerState — prevents NULL dereference on OOM
- **subghz_spectrum**: Add `furi_assert` after `malloc` in `spectrum_app_alloc` for app struct — prevents NULL dereference on OOM

---

## 2026-04-12

### fix
- **flipperpwn**: Fix marauder `get_*` race condition — migrate all 14 callers in `wifi_views.c` and `payload_engine.c` from unsafe `fpwn_marauder_get_aps/hosts/ports/stations/creds()` to safe `fpwn_marauder_copy_*()` with heap-allocated temporary buffers. Remove dead `get_*` functions from `marauder.h` and `marauder.c`
- **flipperpwn**: Mark `connected` field in `wifi_uart.c` as `volatile` for correct cross-thread visibility between UART worker and GUI threads (same class as rogue_ap_detector, ble_scanner, rayhunter_client, evil_ble fixes)

---

## 2026-04-11

### fix
- **evil_ble**: Add NULL context guard in `evil_ble_scanner_rx_cb` to prevent crash from UART callback teardown race (same class as rogue_ap_detector, ble_scanner, and rayhunter_client fixes)
- **evil_ble**: Mark `scanning` field (scanner struct) as `volatile` for correct cross-thread visibility between GUI and UART worker threads
- **evil_ble**: Mark `connected` field (UART struct) as `volatile` for correct cross-thread visibility between UART worker and API callers

---

## 2026-04-10

### fix
- **rayhunter_client**: Add NULL context guard in `rh_worker_rx_line` to prevent crash from UART callback teardown race (same class as rogue_ap_detector and ble_scanner fixes)
- **rayhunter_client**: Mark `connected` field as `volatile` for correct cross-thread visibility between UART worker and timer daemon threads

---

## 2026-04-09

### fix
- **ble_scanner**: Add NULL context guard in `worker_rx_line` to prevent crash from UART callback teardown race (same class as rogue_ap_detector fix)
- **ble_scanner**: Mark `scanning` field as `volatile` for correct cross-thread visibility between GUI and UART worker threads

---

## 2026-04-08

### fix
- **rogue_ap_detector**: Wire up min RSSI filter — Settings UI stored the value but the worker thread never applied it; APs below threshold are now dropped before detection analysis
- **rogue_ap_detector**: Add NULL context guard in `rogue_uart_line_cb` to prevent crash from race condition during UART callback teardown

### docs
- **rogue_ap_detector**: Update README — min RSSI filter now active, document SSID digit-truncation limitation

---

## 2026-04-06

### fix
- **ccid_emulator**: Add `furi_assert` after `malloc` in `ccid_emulator_app_alloc` entry point
- **ccid_emulator**: Widen `log_count` from `uint16_t` to `uint32_t` — APDU monitor and log export showed 0 entries after 65,535 APDU exchanges due to integer wrap

---

## 2026-04-05

### fix
- **spi_flash_dump**: Fix progress bar integer overflow for chips >37 MB — cast to `uint64_t` in read and verify progress views (`spi_flash_dump.c`)
- **spi_flash_dump**: Add `furi_assert` after `malloc` in `spi_worker_alloc` and `hex_viewer_alloc`

### docs
- **spi_flash_dump**: Fix README dump path (`/ext/spi_flash_dump/` → `/ext/spi_dumps/`) to match actual code

---

## 2026-04-04

### fix
- **hid_exfil**: Fix USB config loss on repeated runs — `usb_prev` was being overwritten with the HID config on DataViewer→Back→re-run path (`hid_exfil.c`)
- **hid_exfil**: Add `malloc` NULL check in app entry point

### docs
- **rogue_ap_detector**: Add README with hardware setup, wiring, usage, and detection algorithm docs
- **ble_scanner, evil_ble, subghz_jammer, uart_sniff, rayhunter_client**: Add per-app READMEs — all 13 FAPs now documented

---

## 2026-04-03

### fix
- **badusb_pro**: Add `malloc` NULL check in app entry point (`badusb_pro.c`)

### refactor
- **badusb_pro**: Remove dead code — unreachable `InputKeyLeft`/`InputKeyBack` switch cases in input handler

### chore
- Add `.github/workflows/build.yml` CI workflow using `flipperzero-ufbt-action` with matrix strategy for all 13 FAPs (Issue #3)
- Add `build_all.sh` local build script

---

## 2026-04-02

### fix
- **nfc_fuzzer**: Fix SD card log truncation — 256-byte buffer was too small for 1555-char fuzz result lines (`nfc_fuzzer.c`)
- **nfc_fuzzer**: Fix progress bar `uint32` overflow on large fuzz runs
- **nfc_fuzzer**: Add `volatile` annotation for thread-safety on shared worker state
- **nfc_fuzzer**: Remove redundant `free(NULL)` call

---

## 2026-04-01

### fix
- **flipperpwn**: Fix EXFIL_USB Windows COM port filtering for Windows 11 — parity with `os_detect.c` CDC device enumeration fix

---

## 2026-03-31

### fix
- **subghz_jammer**: Show CC1101 hardware error on screen instead of silent failure (`subghz_jammer.c`)

### chore
- Remove stale `fap_icon_assets="images"` from 7 apps that had empty `images/` directories (Issue #6): badusb_pro, ccid_emulator, flipperpwn, hid_exfil, nfc_fuzzer, spi_flash_dump, subghz_spectrum

---

## 2026-03-30

### fix
- **uart_sniff**: Batch-read worker stream buffer to reduce mutex overhead (`uart_sniff_worker.c`)
- **flipperpwn**: Fix 3 `.fpwn` payload lines exceeding 512-byte parser limit (av_detect, browser_creds, fake_login modules)
- **flipperpwn**: Fix `persist_startup` OPTION default truncated by whitespace parser — changed default from path with spaces to `/tmp/beacon.sh`

### chore
- Add Python bytecode (`*.pyc`, `__pycache__/`) to `.gitignore`

### docs
- Add `TODO.md` for daily maintenance session tracking
- Complete Tier 1 module audit: all 21 `.fpwn`, 3 `.ds`, 2 `.ccid`, and `evil_portal/` files verified

---

## 2026-03-29

### fix
- **spi_flash_dump**: Fix 24-bit address overflow when reading chips >16 MB — 4-byte address mode was not being entered (`spi_worker.c`)
- **spi_flash_dump**: Fix hex viewer column overlap — ASCII column was rendering over hex bytes
- **nfc_fuzzer**: Fix NFC poller crash on rapid fuzz cycles — poller was not stopped before reallocation
- **ble_scanner**: Add BLE hardware availability warning when radio is in use

---

## 2026-03-28

### fix
- **hid_exfil**: Fix CapsLock case inversion — typed characters were wrong case when CapsLock was active on host
- **ccid_emulator**: Fix crash on app exit — CCID handler was freed while USB callback was still registered
- **flipperpwn**: Fix CDC OS detection — port name filtering, case-insensitive matching, abort responsiveness (`os_detect.c`)
- **rayhunter_client**: Fix false-connected state — validate UART response content before marking ESP32 as connected
- **badusb_pro**: Fix dropped keystrokes under rapid typing — add inter-keystroke delay
- **ccid_emulator**: Fix crash when switching card profiles — parser state was not reset
- **badusb_pro**: Add parser diagnostics for malformed `.ds` files

### feat
- **hid_exfil**: Add LED channel pre-flight probe and firmware version warning

### refactor
- **rogue_ap_detector, ble_scanner, evil_ble**: Rewrite Marauder UART parsers to handle real ESP32 output formats instead of synthetic test data
- **badusb_pro**: Remove dead BLE mode setting (never implemented)
- **ccid_emulator**: Remove dead VID/PID customization UI (SDK doesn't support it)

### chore
- Remove GitHub Actions workflow — builds done locally with `ufbt`

---

## 2026-03-26

### fix
- **badusb_pro**: Fix integer overflow in variable arithmetic — `VAR` operations on large values wrapped silently
- **badusb_pro**: Fix silent `$var` dropping — undefined variable references in `STRING` lines were removed instead of producing an error

---

## 2026-03-25

### fix
- **flipperpwn**: Fix WiFi scan/connect failures — Marauder command sequencing was not waiting for scan completion before issuing connect
- Cross-app: Fix 20+ bugs from full codebase code review (buffer overflows, null derefs, resource leaks, race conditions)
- Cross-app: Fix 14 bugs from second review pass (edge cases in parsers, protocol handlers, view lifecycle)
- Cross-app: Fix 10 consensus bugs from triple-agent chain-of-thought verification

---

## 2026-03-23

### fix
- Cross-app: Fix 20+ bugs found during first full codebase code review — buffer overflows, null pointer dereferences, resource leaks, unchecked return values across all 13 apps

---

## 2026-03-16

### fix
- Fix 5 device crashes found during on-device testing — stack overflows, GPIO conflicts, view lifecycle issues
- Fix GitHub issues #1 and #2

### docs
- Update README: fix app count (7 → 13), add project status note

---

## 2026-03-15

### fix
- **flipperpwn**: Fix payload execution engine — commands were not dispatched to correct OS handler
- **flipperpwn**: Fix Marauder scan state machine — scan results were lost on state transitions

---

## 2026-03-09

### feat
- **flipperpwn**: Add EXFIL_USB command for high-bandwidth USB CDC serial exfiltration — enables data extraction at ~115 KB/s vs HID channel's ~30 B/s

### fix
- **flipperpwn**: Fix EXFIL_USB memory barrier, drain loop, and buffer truncation issues
- **flipperpwn**: Reduce memory footprint by ~50 KB to fix OOM crash on device — pre-allocated buffers were oversized
- **hid_exfil**: Fix clang-format lint failure
