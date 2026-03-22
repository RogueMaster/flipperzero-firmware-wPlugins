v1.6:
- Feature: Progress bar during theme application with file counter and current file name indicator
- Improve: Replaced blocking SDK merge call with manual recursive copying to prevent UI freezing
- Improve: Optimized copy buffer size for faster theme application from SD card

v1.5:
- Feature: theme file integrity validation — checks meta.txt, frame_0.bm, manifest.txt
- Feature: invalid themes marked with \[!P\], \[!A\], \[!S\] prefix in menu
- Feature: Status line on Info screen (OK / Invalid!)
- Feature: Apply blocked for invalid themes with error popup
- Cleanup: removed remaining redundant timer NULL checks
- Cleanup: auto-formatted via ufbt format

v1.4.1:
- Fix: bounded directory recursion (max depth 8) prevents stack overflow
- Fix: removed dangerous goto in preview draw — uses bool flag pattern
- Fix: MAX_THEMES reduced to 64 (saves ~9KB RAM)
- Fix: removed redundant NULL checks on timers (always allocated)
- Fix: removed unreachable default case in type_label switch
- Improve: added MAX_DIR_DEPTH constant for recursive dir traversal

v1.4:
- Feature: favorites system — mark themes with * prefix, grouped at top of menu
- Feature: animated preview — up to 4 frames cycling on Info screen
- Feature: reboot countdown timer (5 sec auto-reboot after apply/restore)
- Feature: SD card status check on startup with error dialog
- Improve: favorite toggle via Up key on Info screen
- Improve: preview timer stops when leaving Info view (no resource leak)
- Refactor: reboot dialog replaced with custom View + FuriTimer

v1.3:
- Refactor: unified ThemeEntry struct (replaces 3 separate arrays)
- Feature: themes sorted alphabetically (case-insensitive)
- Feature: submenu header shows theme count "Themes (N)"
- Improve: cached theme metadata (anim count, size) — Info screen loads instantly on repeat
- Improve: insertion sort for consistent alphabetical ordering

v1.2:
- Refactor: shared file-reading utility (removed ~80 lines of code duplication)
- Fix: memory management in preview loader (goto cleanup pattern)
- Fix: buffer truncation uses named constant instead of magic number
- Fix: display name length adjusted to actual screen space (13 chars)
- Fix: uint16_t → size_t for storage read/write return values
- Fix: storage_common_mkdir return value now checked and logged
- Improve: named Y-coordinate constants for Info screen layout
- Improve: smarter name truncation avoids redundant strlen call

v1.1:
- Animation preview on theme info screen (first frame thumbnail)
- LZSS/heatshrink decompression for compressed .bm frames
- Configurable paths for custom firmware compatibility
- Code cleanup and optimizations

v1.0:
- Initial release
- Scan SD card for animation packs in /ext/animation_packs/
- Support 3 theme formats: Pack (P), Anim Pack (A), Single (S)
- Theme info screen with type, animation count, and size
- One-tap apply with automatic backup
- Delete themes directly from the app
- Restore previous theme from backup
- Reboot dialog after applying theme
