# Developer Guide

Everything you need to build, extend and release **Flipper API Caller**.

## Toolchain

The project builds with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt).

```sh
pip install ufbt
ufbt update            # install/refresh the SDK (currently 1.4.3, API 87.1)
ufbt build             # builds the .fap
ufbt format            # clang-format (always run before lint)
ufbt lint              # code style checks
ufbt launch            # deploy and run on a connected Flipper
```

Note: qFlipper must be closed while `ufbt launch` is running (it holds the
COM port). The app stack is set to 4 KB in `application.fam`.

## Project structure

```text
src/
├── api_caller.c/h         # Entry point: AppContext, SceneManager + ViewDispatcher
├── api/
│   ├── flipper_http.c/h   # Vendored FlipperHTTP C SDK (v2.2.0, HTTP_TAG "ApiCaller")
│   └── call_runner.c/h    # Non-blocking HTTP request execution
├── scenes/                # scene_main, scene_wifi_*, scene_call_*
├── wifi/
│   └── wifi_manager.c/h   # FlipperHTTP wrapper (UART Usart, 115200 baud)
└── utils/
    ├── wifi_history.c/h   # Saved networks (FlipperFormat)
    ├── call_history.c/h   # Saved API calls (FlipperFormat)
    ├── long_text_view.c/h # Custom scrollable text view
    └── logger.c/h         # debug.log writer
```

## Conventions

- **Scenes**: each scene has `on_enter` / `on_event` / `on_exit`; register them
  in the three handler arrays in `api_caller.c` **in the same order** as the
  `ApiCallerScene` enum in `api_caller.h`. Custom events go through
  `view_dispatcher_send_custom_event`; BACK through
  `scene_manager_handle_back_event`; a 500 ms tick callback is already active.
- **Views**: owned by `AppContext`, reset in `on_exit`.
- **UI text**: ASCII only, English.
- **Storage**: FlipperFormat with repeated keys for lists; `APP_DATA_PATH`
  with `storage_common_resolve_path_and_ensure_app_directory`. **Empty string
  fields are stored as `-`** and restored as empty (FlipperFormat does not
  round-trip empty values reliably).
- **No duplicates**: grep for existing helpers before adding new ones.

## FlipperHTTP protocol (verified, v2.2.0)

- `[PING]` → `[PONG]`; `[VERSION]` → `"2.2.0"`
- `[WIFI/SCAN]` → `[GET/SUCCESS]`, `{"networks":["SSID",...]}`, `[GET/END]`
  (multi-line: wait for the line containing `"networks"`; SSID only, no RSSI)
- `[WIFI/SAVE] {"ssid","password"}` → `[SUCCESS]` / `[ERROR]`
- `[WIFI/STATUS]` → `true`/`false`; `[WIFI/SSID]`, `[IP/ADDRESS]`
- `[GET]url` / `[GET/HTTP]{"url","headers"}` and `[POST|PUT|PATCH|DELETE/HTTP]`
  with `{"url","headers","payload"}` (headers/payload must be JSON; `{}` when
  empty). Replies: `[GET/SUCCESS]{"Status-Code":...,...}`, body lines,
  `[GET/END]` (or `[ERROR]`).
- There is no `WIFI/FORGET` and no HEAD method (HEAD is mapped to GET).

`call_runner.c` executes requests asynchronously: a per-line RX callback
(`user_rx_line_cb`) drives the state machine (`call_received` /
`call_body_done`) and collects the body — never rely on `last_response`,
which fast replies overwrite.

## Gotchas

- `strncat` is **disabled** in the firmware API (APPCHK fails): use bounded
  manual appends.
- `furi_string_set_strn` **replaces** the string (no append).
- The firmware TextBox breaks with long texts: use `long_text_view` for long
  outputs.
- The board sends CRLF line endings: strip `\r` when capturing responses or
  the TextBox renderer stops drawing.
- UART is exclusive: one app at a time; if unavailable, `flipper_http_alloc`
  fails and the app degrades gracefully.

## Releasing

1. Move the version from `[Unreleased]` to its own section in `CHANGELOG.md`:

   ```markdown
   ## [0.1.5] - 2026-08-25

   ### Added
   - ...
   ```

2. Tag and push (a `v` prefix is also accepted):

   ```sh
   git tag 0.1.5
   git push origin 0.1.5
   ```

The `release.yml` workflow builds the FAP (dev + release channels), extracts
the matching CHANGELOG section and creates a GitHub Release with the notes
and the `.fap` attached. Re-tagging an existing version updates the release
instead of failing.
