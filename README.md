# Flipper API Caller

A [Flipper Zero](https://flipperzero.one/) application for saving API calls and sending them through a ESP32 FlipperHTTP-compatible Wi-Fi board.
Call scheduling, `.json` import and per-call run history are on the roadmap.

## Features

**Connection (via FlipperHTTP board)**

- Wi-Fi scan with live, non-blocking updates (auto-refresh every 8 s)
- Saved networks history (SSID + password, quick reconnect)
- Connect / disconnect with status feedback

**Saved API calls**

- Add/edit form: URL, HTTP/HTTPS protocol, method
  (GET/POST/PUT/DELETE/PATCH/HEAD), query, headers, body
- Automatic `http://` / `https://` scheme normalization
- Call list persisted on storage (up to 16 entries)

**Execution and logging**

- Non-blocking request execution, 30 s timeout, distinct error messages
- Full response body view (up to 32 KB) in a custom scrollable view
- Debug log at `/data/debug.log` (auto-truncated at 32 KB)

## Supported hardware

The app only speaks the FlipperHTTP text protocol over the Flipper UART
(TX pin 13, RX pin 14, 3v3 pin 9, GND pin 11), so it is board-agnostic:

- **ESP32** (WROOM/S2) running
  [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) v2.2.0
- **BW16** (RTL8720DN) running
  [BlackMagic](https://github.com/SkeletonMan03/FlipperZeroBlackMagic)
  (FlipperHTTP-compatible)
- Power: the official Wi-Fi Devboard runs from the Flipper 3v3 rail; bare
  devkits are best powered via USB.

## Building

Builds with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt).

```sh
ufbt build    # builds dist/api_caller.fap
ufbt format   # clang-format (run before lint)
ufbt lint     # code style checks
ufbt launch   # deploy and run on a connected Flipper
```

## Project structure

```text
.
├── application.fam            # Application manifest
├── CHANGELOG.md
├── README.md
├── images/                    # Icon assets
└── src/
    ├── api_caller.c/h         # Entry point: AppContext, SceneManager, ViewDispatcher
    ├── api/
    │   ├── flipper_http.c/h   # Vendored FlipperHTTP C SDK (v2.2.0)
    │   └── call_runner.c/h    # Non-blocking HTTP request execution
    ├── scenes/                # scene_main, scene_wifi_*, scene_call_*
    ├── wifi/
    │   └── wifi_manager.c/h   # FlipperHTTP wrapper (UART, scan, connect, status)
    └── utils/
        ├── wifi_history.c/h   # Saved networks (FlipperFormat)
        ├── call_history.c/h   # Saved API calls (FlipperFormat)
        ├── long_text_view.c/h # Custom scrollable text view
        └── logger.c/h         # debug.log writer
```

## Roadmap

### 0.1.0 - Skeleton and navigation ✔️

App scaffold (`AppContext`, `SceneManager` + `ViewDispatcher`), main menu and scene navigation.

### 0.1.1 - Wi-Fi connection ✔️

Vendored FlipperHTTP C SDK, asynchronous AP scan with live updates, SSID/password input, saved networks history.

### 0.1.2 - Saved calls ✔️

`CallEntry` data model, "Add call" form, call list with edit/delete, storage
persistence.

### 0.1.3 - Execution and logging ✔️

FlipperHTTP request execution (GET/POST/PUT/PATCH/DELETE, HEAD mapped to GET),
response display, `debug.log` logging, error and timeout handling.

### 0.1.4 - Optimization and UI

- Icons and assets
- Stack profiling (`top`, `free`)
- Loaders and user feedback where needed
- Hardware testing, documentation and release

### v0.2+ (planned)

- Call scheduling (in-app scheduler / ESP32 autonomous scheduler / hybrid - TBD)
- Import of calls and call lists from `.json`
- Connection details screen
- Auto-connect to the first available saved SSID (continuous background scan;
  no scan without a saved SSID; stops after connecting)
- In-app run history (Call list → Run list) and per-call `.log` export
- n8n / Activepieces nodes (text, number, bool)

## Resources

- [Flipper Zero developer docs](https://developer.flipper.net/)
- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)
- [FlipperHTTP](https://github.com/jblanked/FlipperHTTP)
- [BlackMagic for BW16](https://github.com/SkeletonMan03/FlipperZeroBlackMagic)
- Project template: [flipper_zero_template](https://github.com/gemisis/flipper_zero_template)