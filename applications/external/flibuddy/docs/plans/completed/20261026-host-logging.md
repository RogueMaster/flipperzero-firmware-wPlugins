# Host logging: a readable log of everything the app does

## Overview
- The Apple apps have no logging at all (`git grep Logger host/Sources` finds
  nothing). When something fails on the owner's Mac, the only evidence is
  what the status bar says, which makes debugging a guessing game.
- Add structured logging to `FliBuddyCore` and the three apps: every link,
  wire, session, Bambuddy and settings event goes to the unified log
  (`os.Logger`, subsystem `com.sokolachko.flibuddy`) **and** to a plain text
  log file that can be read with `tail -f` and attached to a bug report.
  Secrets never reach the log.
- Nothing visible changes in the UI except a "Show Log File" item in the
  Mac app's Help menu.

## Context (from discovery)
- Seams to instrument: `host/Sources/FliBuddyCore/Transport/SerialTransport.swift`
  (`connect`, `send`, `disconnect`, the read loop), `Transport/BLE/
  CoreBluetoothCentral.swift` and `BLETransport.swift` (scan, connect,
  MTU, notifications, errors), `Transport/TransportSession.swift`
  (`send`, `receive`, hello handshake), `ViewModels/FlipperLink.swift`
  (`connect`, `disconnect`, `LinkState` changes, retries),
  `Session/Session.swift` (`checkBambuddy`, `handleScan`, `handleIntent`,
  create on scan, link and delete), `Bambuddy/BambuddyClient.swift`
  (`send`: method, path, status, duration, error), `Session/Settings.swift`
  (`load`, `save`, Keychain status codes), `Session/WatchSettingsSync.swift`,
  `ViewModels/SettingsViewModel.swift` (test connection, catalog loads).
- Message types: `Protocol/Message.swift` (`hello`, `scan`, `lookup`,
  `intent`, `result`); a scan carries the whole tag dump, so wire lines
  can be long.
- The API key is in `Settings.apiKey`, the `X-API-Key` header and the QR
  payload (`Session/APIKeyQRPayload.swift`).
- Apps: `host/Sources/FliBuddyMac/App.swift` (SwiftUI `Window`, `Settings`),
  `host/Apple/iOS/App.swift`, `host/Apple/Watch/App.swift`.
- Tests: `host/Tests/FliBuddyCoreTests/` with swift-testing; fakes in
  `FakeCentral.swift`, `FakeInventory.swift`, `TransportTestSupport.swift`.
- Repo rules: `AGENTS.md` (English, `@ViewState` in views, 80 % coverage,
  `make test`, `make lint`, `make coverage`).

## Development Approach
- **Testing approach**: TDD.
- Complete each task fully before moving to the next.
- **CRITICAL: every task MUST include new/updated tests** for code changes
  in that task; **all tests must pass before starting the next task**.
- **CRITICAL: update this plan file when scope changes during implementation.**
- Log lines are English, one event per line, no trailing punctuation, in
  the form `<category> <event>: key=value …`. Never log the API key, the QR
  payload or a full settings dump; never log a whole tag dump above debug.

## Testing Strategy
- **Unit tests**: an in-memory sink captures lines; tests assert the events
  each seam emits, redaction, level filtering, file rotation.
- **E2E tests**: none in the project.

## Progress Tracking
- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix

## What Goes Where
- **Implementation Steps**: code in this repository.
- **Post-Completion**: reading the log on the owner's Mac.

## Implementation Steps

### Task 1: `Log` facade with unified log and file sinks
- [x] write the failing tests first in `host/Tests/FliBuddyCoreTests/LogTests.swift`:
      a line logged at `.info` reaches an in-memory sink with timestamp,
      level, category and message; `.debug` lines are dropped unless the
      level is `.debug`; `redact` replaces any `bb_…` API key and any
      `key=` / `X-API-Key` value with `«redacted»`; the file sink rotates
      at the size limit and keeps the configured number of old files
- [x] add `host/Sources/FliBuddyCore/Logging/Log.swift`: `public enum Log`
      with `static func info/notice/error/debug(_ category: Category,
      _ message: @autoclosure () -> String)`, `Category` cases `link`,
      `wire`, `session`, `bambuddy`, `settings`, `sync`, `ui`, `app`;
      `LogLevel` from the `FLIBUDDY_LOG` environment variable (`debug`,
      `info` default, `error`) or `Log.level` set by the app; a
      `LogSink` protocol (`write(line:)`) with `OSLogSink` (one `os.Logger`
      per category, subsystem `com.sokolachko.flibuddy`), `FileSink` and
      `MemorySink` (tests); `Log.sinks` is configurable
- [x] `FileSink`: `~/Library/Logs/FliBuddy/flibuddy.log` on macOS, the app's
      Application Support `Logs/flibuddy.log` on iOS and watchOS; line
      format `2026-10-26T13:04:05.123+0300 INFO link message`; appends,
      flushes per line, rotates at 5 MB keeping 3 old files; a write
      failure is reported once through the unified log and then ignored
- [x] `Log.redact(_:)` applied to every message before the sinks
- [x] run `cd host && swift test` - must pass before task 2

### Task 2: Link and wire events
- [x] tests first (`TransportSessionTests`, `FlipperLinkTests`,
      `SerialTransportTests`, `BLETransportTests` with `MemorySink`): each
      event below appears once with the expected fields
- [x] `FlipperLink`: `link connect: transport=usb port=/dev/…` /
      `transport=bluetooth peripheral=<uuid>`, `link state:
      <old> -> <new>` with the problem text, `link retry: in=2s`,
      `link disconnect`
- [x] `SerialTransport`: open with the resolved path and the errno on
      failure, close, write errors; `CoreBluetoothCentral` / `BLETransport`:
      scan start/stop, discovered peripheral (name, RSSI), connect,
      service/characteristic found or missing, MTU, disconnect with the
      CoreBluetooth error
- [x] `TransportSession`: `wire tx: <line>` and `wire rx: <line>` at
      `.debug`, truncated to 300 characters with `… (+N bytes)`; dropped
      lines (`ProtocolError`) at `.error` with the reason and the first 120
      characters; `hello sent: app=…`, `hello received: app=… fw=… os=…`,
      `hello timeout: after=2s`
- [x] ➕ `TransportSession` only sees decoded messages, so the wire lines
      are logged where lines are encoded and decoded: `WireLog` (in
      `Transport/WireLog.swift`) is called by `SerialTransport` and
      `BLEChunker`; it also reports lines the framer drops as too long.
      `SerialTransport` reads on a dispatch queue and logs to the log in
      effect at `connect()`. The peripheral's name is logged by
      `BLETransport` when the scan finds it, not by `FlipperLink`, which
      only knows the identifier. "No Flipper found" is logged once while
      it lasts, its retries at `.debug`
- [x] run tests - must pass before task 3

### Task 3: Session, Bambuddy and settings events
- [x] tests first (`SessionTests`, `BambuddyClientTests`, `SettingsTests`,
      `SettingsViewModelTests`, `WatchSettingsSyncTests`)
- [x] `Session`: `session bambuddy: <state>` on every change with the
      message; `session scan: id=… uid=… tray=… kind=mfc|ntag material=…`;
      `session lookup: id=… status=found spool=… | not_found | created
      spool=… | error=…` with the elapsed ms; `session intent: id=… action=…
      spool=… grams=… mode=…` and `session result: id=… ok|error=…`;
      create on scan, link, delete and their failures
- [x] `BambuddyClient.send`: `bambuddy GET /inventory/spools/by-tag -> 200
      in 43 ms` at `.info`, the response body's first 200 characters at
      `.debug`, errors at `.error` with the Bambuddy message; the
      `X-API-Key` value never appears (assert in a test)
- [x] `SettingsStore`: `settings load: url=… key=present|absent catalog=…
      core=… mode=… transport=…`, `settings save: …`, Keychain failures
      with the `OSStatus` and `SecCopyErrorMessageString`
- [x] `SettingsViewModel`: test connection start/result, catalog load
      result; `WatchSettingsSync`: payload sent/received (version only, no
      key)
- [x] ➕ the lookup line reads `lookup: id=… status=found|created spool=… ms=…`,
      `status=not_found ms=…` or `status=error ms=… error=…`, timed with the
      session's clock; host creates (`create` / `create failed`) and a failed
      colour map load are logged too. `BambuddyClient` logs 2xx and 404 (a
      lookup miss the callers interpret) at `.info`, other statuses at
      `.error` with Bambuddy's `detail`, network failures at `.error`. The
      watch sync logs held sends at `.debug`
- [x] run tests - must pass before task 4

### Task 4: Apps: start-up line, level, Show Log File
- [x] `FliBuddyMac/App.swift`, `iOS/App.swift`, `Watch/App.swift`: on launch
      `app start: FliBuddy <version> <platform> log=<file path>
      level=<level>`; the Mac app adds a Help menu item "Show Log File"
      (reveals the file in Finder) and reads `FLIBUDDY_LOG` from the
      environment; iOS and watch keep `.info`
- [x] `Log` installs `OSLogSink` and `FileSink` once per process
      (`Log.bootstrap(appName:)`), idempotent; tests cover bootstrap
      idempotence with the memory sink
- [x] run `make test`, `make lint`, `make coverage`, `make apple-project
      apple-build` - must pass before task 5
- [x] ➕ the start line is logged at `.notice` by `Log.bootstrap`, which
      also takes the level, version, platform, file URL and sinks (tests
      pass a memory sink); `Log.fileURL` is the installed file, which Show
      Log File reveals. The apps bootstrap in `init()` before creating the
      `HostModel`, whose settings load already logs

### Task 5: Verify acceptance criteria
- [x] every seam listed in Context emits its events (grep the sources for
      `Log.` and compare with the lists in Tasks 2–3)
- [x] `git grep -n 'apiKey' host/Sources | grep -i log` shows no key reaching
      a log call; a test proves redaction of a `bb_` key
- [x] run the full test suite, lint and coverage
- [x] ➕ the grep's hits are false positives (the `Logging/` path, "catalog"
      contains "log"); settings lines log `key=present|absent`; redaction is
      proven by `LogTests` and by `BambuddyClientTests`, `SettingsTests` and
      `WatchSettingsSyncTests` asserting the key never reaches a sink

### Task 6: Documentation
- [x] `host/README.md`: a "Debugging" section: the log file paths, the
      `FLIBUDDY_LOG=debug` switch, `log stream --predicate 'subsystem ==
      "com.sokolachko.flibuddy"'`, what each category records, and that
      the API key is never logged
- [x] `AGENTS.md`: one line that new code paths log their outcome through
      `Log` and that secrets never reach it

## Technical Details
- `Log` is a static facade so view models and actors call it without
  plumbing; sinks are protocol instances behind a lock, replaced in tests.
- Messages are autoclosures so debug-only formatting (wire lines) costs
  nothing when the level is `.info`.
- Time stamps are ISO 8601 with milliseconds and the local offset, so they
  match the Bambuddy server log on the NAS.
- Wire truncation keeps the file readable: a scan line with a full MIFARE
  dump is about 3 KB.

## Post-Completion
**Owner's Mac**: run the Mac app, then `tail -f ~/Library/Logs/FliBuddy/flibuddy.log`
while scanning a tag; `FLIBUDDY_LOG=debug swift run FliBuddyMac` shows the
wire lines.
