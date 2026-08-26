# Changelog

All notable changes to the ESP32 API Caller project are documented in this file.
___

## [0.1.4] - 2026-08-25

### Added

- App icon
- i18n system (`src/utils/locale.c/h`): stable `LocKey` string keys, Italian
  and English translation tables, language persisted in `/data/settings.txt`
  (default English, file created on first launch); every UI string now goes
  through `locale_get`

### Fixed

- Saved networks not shown after restart: `wifi_history_load` stopped
  restoring the SSID into the history entries (nameless ghost items that
  froze the UI when selected), empty passwords are now correctly mapped back
  from the `-` placeholder, corrupted empty-SSID entries are skipped on load,
  and connecting with an empty SSID is rejected early

## [0.1.3] - 2026-08-25

### Added

- **Execution and logging**:
  - `src/api/call_runner.c/h`: non-blocking FlipperHTTP request execution
    (GET/POST/PUT/PATCH/DELETE; HEAD mapped to GET), URL+query building,
    per-line response capture, 30 s timeout and distinct error handling
  - `src/utils/logger.c/h`: uptime-timestamped log at `/data/debug.log` with
    automatic truncation at 32 KB
  - "Call detail" scene: Run/Edit menu, "Sending request..." screen, result
    (method, URL, status code, body) with SEND/RESULT/ERROR logging
  - Call list entries now open the detail scene (execution + editing)

### Fixed

- Device freeze during execution: request state is now driven by the RX
  callback instead of `last_response` (which fast replies overwrote); app
  stack raised to 4 KB; logger path resolved once; progress markers written
  to `debug.log`
- Unreadable responses (blank screen while scrolling): CRLF `\r` characters
  stripped and control characters normalized; response buffer raised to 32 KB
  with an explicit `[response truncated]` notice
- Long responses disappearing while scrolling (firmware TextBox limitation):
  new custom scrollable view `src/utils/long_text_view.c/h` with local line
  wrapping (UP/DOWN one line, OK full page) used for call results
- Saved calls lost after restart: empty fields (query/headers/body,
  open-network Wi-Fi passwords) are now persisted with a `-` placeholder and
  restored as empty on load

## [0.1.2] - 2026-08-25

### Added

- **Saved calls**:
  - `CallEntry` data model (URL, HTTP/HTTPS protocol, method, query, headers,
    body) in `src/api_caller.h`
  - Call history `src/utils/call_history.c/h` on storage (FlipperFormat,
    `/data/call_history.txt`, up to 16 entries)
  - "Add call" form: protocol switch, method list
    (GET/POST/PUT/DELETE/PATCH/HEAD), URL/Query/Headers/Body fields, automatic
    `http://` / `https://` scheme normalization
  - "Call list" scene: submenu of saved calls, pre-filled edit form, delete
  - Back from a text field returns to the form without leaving the scene

## [0.1.1] - 2026-08-25

### Added

- **Skeleton**: `AppContext`, `SceneManager` + `ViewDispatcher`,
  main menu (VariableItemList), stub scene navigation
- **Wi-Fi via FlipperHTTP**:
  - FlipperHTTP C SDK v2.2.0 vendored in `src/api/flipper_http.c/h`
    (ESP32-WROOM target)
  - `src/wifi/wifi_manager.c/h` wrapper (ping, scan, ssid, ip, status,
    save+connect, disconnect)
  - "Connection" scene (network scan / saved networks / connected to /
    disconnect), scan list, password input, result TextBox
  - **Asynchronous scan**: "Searching... Xs" feedback, live list with 8 s
    auto-refresh, non-blocking UI
  - **Saved networks history** (`src/utils/wifi_history.c/h`): automatic
    SSID+password persistence (FlipperFormat, up to 8 entries), reconnect
    without retyping the password

### Fixed

- Scan: list truncated to a single network (`furi_string_set_strn` replaces
  instead of appending)
- Scan: multi-line board replies (wait for the JSON line containing
  "networks")
- Build: entry point aligned with `application.fam`
___

## [Unreleased]

- **0.1.4** (remaining): extended keyboard with symbols (based on
  [UART Terminal](https://github.com/cool4uma/UART_Terminal), MIT), stack
  profiling, loaders and feedback, optimization and fixes, about scene
- **0.1.5**: settings (languages, log path...) + API client usability
  - Key-value headers editor and body type selection
    (JSON / form-urlencoded / raw)
  - Auth helpers: Basic auth, Bearer token / API key stored per call
  - JSON pretty-print in the response view
  - One-tap "Run again" from the result screen
- **0.1.6**: single-call automation
  - Variables and templates (`{{now}}`, `{{random}}`, value prompt at run time)
  - Extract values from the response (JSONPath) and reuse them in follow-up
    calls (simple chains: login → token → authenticated call)
  - Save response to file (raw text / bytes)
  - Per-call `.log` export

### v0.2+

- Call scheduling (in-app scheduler / ESP32 autonomous scheduler / hybrid TBD)
- Import of calls and call lists from `.json`
- Connection details screen
- Auto-connect to the first available saved SSID (continuous background scan; no scan without a saved SSID; stops after connecting)
- In-app run history: Call list → [Call name] → Run list
- n8n / Activepieces nodes (text, number, bool)
- Response headers exposed (firmware patch) and binary/file response support
- Retry / rate-limit helpers
- Support for [Postman Firmware](https://github.com/MassivDash/flipper-postman-esp32s2), 
___

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).