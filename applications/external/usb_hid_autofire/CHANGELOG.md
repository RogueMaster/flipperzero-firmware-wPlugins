# Changelog

## 0.7.1

- Fix `fap_version` in `application.fam`

## 0.7

- Major internal refactor: split monolithic app into focused modules for app lifecycle, controller logic, HID engine, UI rendering, and settings persistence
- Replaced delay busy-loop with timer-driven click scheduler and queued event processing for better responsiveness and lower CPU usage
- Added centralized cleanup path for all setup/exit flows to reliably restore previous USB config and release pressed controls
- Bounded delay range to `5..10000 ms` with saturating adjustment logic and safe timer scheduling
- Reduced redraw overhead by updating UI only on state changes plus low-rate refresh ticks for live metrics
- Expanded fire modes: `Mouse Left`, `Mouse Right`, `Key Enter`, `Key Space`
- Improved UX: dedicated help screen, clearer status (`ACTIVE/PAUSED`), selected mode, delay, and real-time CPS display
- Added presets (`Slow`/`Medium`/`Fast`) with safety confirmation for high-CPS preset
- Added long-press acceleration for delay changes and hold-repeat mode switching
- Added persisted settings (delay, mode, preset, startup policy, last active state) with debounced writes
- Updated manifest category to `USB` for consistency with Flipper firmware/app catalog grouping

## 0.6

- Remove now redundant `itoa` function

## 0.5

- Fix compatibility with Flipper Zero firmware `0.74.2`

## 0.4

- Show active/inactive state in primary font (bold)

## 0.3

- Add a delay between key-presses (with left/right buttons)

## 0.2

- Update icon

## 0.1

- Initial release of the USB HID Autofire application
