# Phase 2: Bluetooth LE transport, shared SwiftUI, iPhone, iPad and Apple Watch apps

## Overview
Phase 1 links the Flipper to a Mac over USB. Phase 2 adds Bluetooth LE on
both sides, an iOS/iPadOS app and an Apple Watch app, so the owner can weigh
spools with a phone, a tablet or a watch in hand. The design is in
`docs/superpowers/specs/2026-10-25-flipper-spoolbuddy-design.md`, section
"Phase 2: BLE transport, iPad and iPhone", whose verified facts bind this
plan. The stock Flipper serial profile is NOT used: it needs passkey pairing,
which watchOS cannot complete. The FAP publishes its own GATT service without
pairing instead.

What changes:
- the Flipper app gets a BLE backend (own profile and GATT service) behind
  the existing `Transport` interface and a persisted transport setting (USB
  or Bluetooth);
- `SpoolBuddyCore` gets a CoreBluetooth `BLETransport` behind the existing
  Swift `Transport` protocol, usable on macOS, iOS and watchOS, and settings
  for the transport kind and the remembered peripheral; macOS gains BLE too;
- the SwiftUI views live in the platform-neutral `SpoolBuddyUI` target
  (Task 1, done);
- iOS and watchOS app targets are generated from an XcodeGen `project.yml`;
  the iPhone pushes the settings to the watch over WatchConnectivity;
- iOS Settings can scan Bambuddy's API-key QR code;
- CI builds both apps for the simulators.

The wire protocol (`protocol/PROTOCOL.md`, the schema, the fixtures) does
not change. BLE carries the same NDJSON lines.

## Context (from discovery)
- Flipper transport interface: `flipper/src/transport.h` (`open`, `close`,
  `send_line`, `set_rx_callback`, `is_connected`); USB backend
  `flipper/src/transport_usb.c` with a worker thread, `line_buffer`,
  `line_sender` (packetised sends with a completion wait); the app wires the
  transport in `flipper/src/spoolbuddy_app.c`, hello handling in
  `flipper/src/host_link.c`, Idle screen in `flipper/src/scene_idle.c`.
- SDK 1.5.1 (in `flipper/.ufbt/current/sdk_headers`), all exported:
  `bt_profile_start`, `bt_profile_restore_default`, `bt_disconnect`,
  `bt_set_status_changed_callback` (`BtStatus{Unavailable, Off, Advertising,
  Connected}`, record `RECORD_BT`), `ble_gatt_service_add`,
  `ble_gatt_service_delete`, `ble_gatt_characteristic_init`,
  `ble_gatt_characteristic_update`, `ble_gatt_characteristic_delete`,
  `ble_event_dispatcher_register_svc_handler` /
  `…_unregister_svc_handler`, `furi_hal_bt_is_active`,
  `furi_hal_version_get_ble_mac`, `furi_hal_version_get_ble_local_device_name_ptr`,
  `saved_struct_load/save`, `storage_common_mkdir`. Headers:
  `targets/f7/ble_glue/furi_ble/profile_interface.h`
  (`FuriHalBleProfileTemplate{start, stop, get_gap_config}`,
  `FuriHalBleProfileBase`), `targets/f7/ble_glue/furi_ble/gatt.h`
  (`BleGattCharacteristicParams`, `BleGattCharacteristicInstance`),
  `targets/f7/ble_glue/furi_ble/event_dispatcher.h`
  (`BleSvcEventHandlerCb(void* event, void* context)` receives raw ACI
  events: `hci_event_pckt` → `evt_blecore_aci` with
  `ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE` and
  `ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE`), `targets/f7/ble_glue/gap.h`
  (`GapConfig{adv_service, appearance_char, bonding_mode, pairing_method,
  mac_address, adv_name, conn_param}`, `GapPairingNone` = "just works" in
  `gap.c`, no security request from the Flipper),
  `lib/stm32wb_copro/wpan/ble/core/ble_defs.h` (`CHAR_PROP_*`,
  `ATTR_PERMISSION_NONE`, `ATTR_PERMISSION_ENCRY_READ/WRITE`,
  `GATT_NOTIFY_ATTRIBUTE_WRITE`). Reference implementation to read:
  `targets/f7/ble_glue/services/serial_service.c` and
  `profiles/serial_profile.c` in the firmware repository (tag 1.5.1).
- Host transport: `host/Sources/SpoolBuddyCore/Transport/Transport.swift`
  (protocol with `state`, `messages`, `connect`, `send`, `disconnect`),
  `SerialTransport.swift` (POSIX serial, `discoverFlipperPorts`),
  `TransportSession.swift`, `ViewModels/FlipperLink.swift` (reconnect loop,
  `makeTransport: (String) -> any Transport`, `discoverPorts`),
  `ViewModels/HostModel.swift`, `Session/Settings.swift` (`port`,
  `SettingsStore`, `KeychainSecretStore`), `ViewModels/SettingsViewModel.swift`.
- macOS views: `host/Sources/SpoolBuddyMac/{App,FeedView,SettingsView,UnknownRowActions,ViewState}.swift`.
- `host/Package.swift`: swift-tools 6.0, `platforms: [.macOS(.v14)]`,
  targets `SpoolBuddyCore`, `SpoolBuddyMac`, `SpoolBuddyCoreTests`
  (fixtures via a symlink resource), CLT plugin workaround.
- Tooling on the owner's Mac: Xcode 27.1 (license accepted), iOS 27.1 and
  watchOS SDKs and simulators, Homebrew, XcodeGen 2.46 installed. CI:
  `.github/workflows/host.yml` runs `swift test` on macOS.
- watchOS CoreBluetooth: central role only, two connections, 30 ms minimum
  connection interval, no state preservation/restoration, background only
  with the `bluetooth-central` mode; no passkey prompt for third-party
  peripherals.
- Bambuddy QR payload: `bambuddy://config?v=1&url=<encodeURIComponent(baseUrl)>&key=<encodeURIComponent(apiKey)>`
  (frontend `utils/apiKeyQr.ts`, version 1).

## Development Approach
- **Testing approach**: TDD. Write the failing test, then the code.
- Complete each task fully before moving to the next
- Make small, focused changes
- **CRITICAL: every task MUST include new/updated tests** for code changes in that task
  - tests are not optional - they are a required part of the checklist
  - write unit tests for new functions/methods
  - write unit tests for modified functions/methods
  - add new test cases for new code paths
  - update existing test cases if behavior changes
  - tests cover both success and error scenarios
- **CRITICAL: all tests must pass before starting next task** - no exceptions
- **CRITICAL: update this plan file when scope changes during implementation**
- Run tests after each change
- Keep the wire protocol unchanged
- Hardware-only code (CoreBluetooth calls, Flipper BLE calls) sits behind
  small protocols or function tables so the logic around it is unit-tested
  with fakes; the thin adapters are read-reviewed and checked on hardware in
  Post-Completion
- Follow `AGENTS.md`: English everywhere in the repository, one commit per task
- Minimum OS versions: macOS 14 stays; iOS, iPadOS and watchOS 26 (current
  major minus one, per the design)
- The BLE link is unencrypted by design (no pairing). One macro,
  `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION` (default 0), switches the
  characteristics to `ATTR_PERMISSION_ENCRY_*` so the owner can try
  "just works" pairing on real devices without code changes

## Testing Strategy
- **Unit tests**: required for every task (see Development Approach above)
- Suites: `make -C flipper/tests test`, `cd host && swift test`,
  `cd tools && uv run pytest`; `make test` runs all; `make lint`,
  `make coverage` gate the end; the iOS and watchOS targets are built for
  the simulators with `make apple-build`
- No e2e suite; pairing and BLE on real devices are Post-Completion

## Progress Tracking
- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix
- Update plan if implementation deviates from original scope
- Keep plan in sync with actual work done

## What Goes Where
- **Implementation Steps** (`[ ]` checkboxes): tasks achievable within this codebase - code changes, tests, documentation updates
- **Post-Completion** (no checkboxes): items requiring external action - manual testing, changes in consuming projects, deployment configs, third-party verifications
- **Checkbox placement**: Checkboxes belong only in Task sections (`### Task N:` or `### Iteration N:`). Do not put checkboxes in Success criteria, Overview, or Context — they cause extra loop iterations.

## Implementation Steps

### Task 1: Package for two platforms and a shared SwiftUI target
- [x] `host/Package.swift`: `platforms: [.macOS(.v14), .iOS(.v26)]`; new library target `SpoolBuddyUI` (depends on `SpoolBuddyCore`) and product; `SpoolBuddyMac` depends on both
- [x] move `FeedView.swift`, `SettingsView.swift` (the `SettingsForm` part), `UnknownRowActions.swift` and `ViewState.swift` into `host/Sources/SpoolBuddyUI/`, make them `public` where the apps need them, and wrap macOS-only parts (`SwiftUI.Settings` scene, serial port picker, `.keyboardShortcut`, `NSApp`) in `#if os(macOS)`; `SpoolBuddyMac` keeps `App.swift` and the Settings scene wrapper only
- [x] `SerialTransport.discoverFlipperPorts()` returns `[]` and `SerialTransport.init` is unavailable off macOS (`#if os(macOS)` around the termios code, a stub that throws `TransportError.notConnected` elsewhere)
- [x] `HostModel.hostHello` reports the platform (`"macOS …"` / `"iOS …"` / `"iPadOS …"`) through a small `PlatformInfo` helper with a test
- [x] write tests: `PlatformInfo`, port discovery off macOS is empty (compile-time guarded test), existing suites still pass; `swift build` of `SpoolBuddyUI` for macOS succeeds
- [x] run all three suites - must pass before task 2
- ➕ deviations: the manifest uses `.iOS("26.0")` because `.v26` needs tools version 6.2; `.keyboardShortcut` stays unguarded (SwiftUI has it on iOS too); off macOS `SerialTransport.init` stays available (HostModel's default factory uses it) and the stub's `connect`/`send` throw `notConnected`; `SpoolBuddyMac/App.swift` has an `#else` stub `main` so the whole package (tests included) builds for iOS
- ➕ `xcodebuild test -scheme SpoolBuddy-Package -destination 'platform=iOS Simulator,…'` runs the suite on the iOS simulator (299 tests pass, the pty tests are macOS-only); `xcodebuild -scheme SpoolBuddyUI -destination 'generic/platform=iOS Simulator' build` succeeds

### Task 2: Chunking and line reassembly for BLE (pure Swift)
- [x] `host/Sources/SpoolBuddyCore/Transport/BLEChunker.swift`: `BLEChunker` splits outgoing lines (newline appended) into writes no longer than `writeSize` (set from `maximumWriteValueLength(for: .withoutResponse)`, which is ATT MTU - 3, capped at 243), hands out one write at a time (`nextWrite()` returns `nil` while one is in flight or the queue is empty, `writeConfirmed()` releases the next), and reassembles inbound notifications into messages through `NDJSONFramer`; lines over 8192 bytes are rejected as in PROTOCOL.md
- [x] write tests: write sizes at the cap and below; a 2000-byte line crosses several writes in order; nothing is handed out while a write is in flight; inbound notifications split mid-line reassemble; empty and oversize inputs; the write size changing mid-stream
- [x] run all three suites - must pass before task 3
- ➕ `BLEChunker` also has `enqueue(_ message:)`, `isWriteInFlight`, `hasPendingWrites` and `reset()` (drops queued writes and a partial inbound line, keeps `writeSize`); writes are cut from one byte queue when handed out, so a write may span two lines and a `writeSize` change applies from the next write; an empty line queues nothing; `writeSize` is clamped to `1...243`, default 20

### Task 3: `BLETransport` on CoreBluetooth behind testable protocols
- [x] `host/Sources/SpoolBuddyCore/Transport/BLE/SpoolBuddyGATT.swift`: the UUID constants (service `51C07E84-E37E-4D52-AC6D-B2003116F506`, RX `D250A7B1-D301-43F6-928E-BF2CA5D115BD`, TX `C3AB2366-401D-482C-9002-B0A046C2A4E4`)
- [x] `host/Sources/SpoolBuddyCore/Transport/BLE/Central.swift`: protocols `BLECentral` (power state, scan for the service UUID, retrieve a remembered peripheral by identifier, connect, cancel) and `BLEPeripheralLink` (discover the service and the two characteristics, set notify on TX, `maximumWriteValueLength`, write RX with response, an event stream: ready, notification data, write confirmed, disconnected); `CoreBluetoothCentral` adapter on `CBCentralManager`/`CBPeripheral` compiled for macOS, iOS and watchOS (not unit-tested); `FakeCentral` in the test target
- [x] `host/Sources/SpoolBuddyCore/Transport/BLE/BLETransport.swift`: implements `Transport` with `BLEChunker`: `connect()` reconnects to the remembered identifier or scans, discovers, subscribes to TX, then reports `.open`; `send` enqueues and drains one write at a time; disconnect or power-off finishes the `messages` stream; new `TransportError` cases `bluetoothOff`, `bluetoothUnauthorized`, `peripheralNotFound`, `serviceNotFound`
- [x] `FlipperLink`: a `TransportChoice` (`.usb(port:)` / `.bluetooth(peripheral:)`) replaces the port string; the reconnect loop and `isActive` behave the same for both; BLE discovery exposed for the settings picker (name, identifier, RSSI)
- [x] write tests with `FakeCentral`: connect/subscribe → `.open` and the Flipper hello reaches `messages`; a long `intent` goes out as several writes, each after the previous confirmation; disconnect finishes the stream and `FlipperLink` retries; power off → `bluetoothOff`; unknown identifier → scan; remembered identifier → direct connect; a peripheral without the service → `serviceNotFound`
- [x] run all three suites - must pass before task 4
- ➕ deviations: `Transport.connect()` is now `async throws` (BLE waits for power, scan, connect and subscription; `SerialTransport` and the fakes stay synchronous, which satisfies the requirement); `BLEPeripheralLink.open(service:rx:tx:events:)` does discovery and the TX subscription in one async call and delivers notification / write confirmed / write failed / disconnected through the `events` callback, which is installed before subscribing so the FAP's `hello` on subscribe is kept; one more error case `bluetoothUnsupported`; scan and connect each time out after 10 s with `peripheralNotFound` (the pending connection is cancelled); a failed RX write drops the connection; `BLECentral.disconnect(_ link:)` names the connection to drop and cancelling the `connect` task abandons the attempt, and `CoreBluetoothCentral` starts a connect only after the `didDisconnect` of a connection or pending attempt it cancelled, and a late `didConnect` of a cancelled attempt answers no one (review fix: every connection to a peripheral shares its identifier, so a late cancel or disconnect of the old one hit the new one); `FlipperLink.connect(_ choice:)` replaces `connect(port:)`, `choice` replaces `requestedPort`, `port` stays for USB, `discoverFlippers()` streams scan results through an injectable `scanPeripherals`; `HostModel` still always picks `.usb(port:)` until Task 4 adds the setting; `BLEPromise` (one-shot callback → async bridge, cancellable) is shared by the adapter and unit-tested
- ⚠️ for Task 4: touching `CoreBluetoothCentral.shared` asks for Bluetooth permission; the `swift run` macOS executable has no Info.plist, so it needs `NSBluetoothAlwaysUsageDescription` embedded (e.g. linker `-sectcreate __TEXT __info_plist`) before Bluetooth is selectable on the Mac; tests never create the real central

### Task 4: Transport settings on the host
- [x] `Settings`: `transport: TransportKind` (`usb` default on macOS, `bluetooth` elsewhere), `peripheralId: UUID?`, `peripheralName: String?`; `SettingsStore` persists them; `HostModel.apply` reconnects when the choice changes
- [x] `SettingsViewModel`: transport picker, a list of discovered Flippers while scanning ("Scan for Flipper"), "Forget Flipper"; USB section only on macOS
- [x] `SettingsForm` in `SpoolBuddyUI`: the Transport section for macOS and iOS; the macOS window keeps its minimum size
- [x] write tests: defaults per platform; persistence round trip; choosing a peripheral stores id and name; forget clears them; `HostModel` reconnects on a change of kind or peripheral and not otherwise
- [x] run all three suites - must pass before task 5
- ➕ deviations: `TransportKind` (`usb`, `bluetooth`) has `platformDefault` and `available` (`[.usb, .bluetooth]` on macOS, `[.bluetooth]` elsewhere); `Settings.transportChoice` maps the settings to the link's `TransportChoice`, and `HostModel` reconnects a running link when that choice changes (kind, USB port while on USB, peripheral while on Bluetooth), not on a name change; unknown stored kinds and malformed peripheral ids load as defaults; `SettingsViewModel` gets `discoverFlippers` (HostModel passes `link.discoverFlippers()`) and `scanDuration` (15 s, the scan stops by itself), lists each Flipper once with the latest RSSI, `choosePeripheral` also selects Bluetooth and stops the scan; the form's Transport picker only shows with more than one kind, and the form stops scanning on disappear; a scan with Bluetooth off, not allowed or unsupported stops at once and sets `scanProblem` (review fix), read through `FlipperLink.bluetoothPowerState()`
- ➕ the ⚠️ from Task 3 is resolved: `host/Sources/SpoolBuddyMac/Info.plist` (with `NSBluetoothAlwaysUsageDescription`) is linked into the macOS executable's `__TEXT,__info_plist` section with `-sectcreate` (macOS only, `unsafeFlags` in the root package); `otool -s __TEXT __info_plist` shows it; the plist deliberately has no `CFBundleIdentifier`, which would move `UserDefaults.standard` away from the `SpoolBuddyMac` domain holding the saved Phase 1 settings; `tools/tests/test_mac_info_plist.py` guards the plist and the manifest
- ➕ on the iOS simulator the Settings, SettingsViewModel, HostModel and FlipperLink suites pass (73 tests, `xcodebuild test -scheme SpoolBuddy-Package -destination 'platform=iOS Simulator,name=iPhone 17' -only-testing:…`)
- ⚠️ in the full iOS simulator run two untouched FlipperLink tests (`aPortThatWillNotOpenIsReported`, `bluetoothThatIsOffIsReportedAndRetried`) missed their 2 s `eventually` window under load (8 s) and pass when run alone; worth a longer timeout if it recurs in CI

### Task 5: Flipper settings and transport choice on the Idle screen
- [x] `flipper/src/app_settings.{c,h}`: `AppSettings { uint8_t version; uint8_t transport; }` saved with `saved_struct_save`/`load` at `/ext/apps_data/spoolbuddy/settings.bin` (directory created with `storage_common_mkdir`); unreadable or wrong-version files load the defaults (USB); the file functions are isolated so the choice logic is host-testable
- [x] Idle screen: OK opens a submenu "Transport: USB / Bluetooth"; choosing restarts the transport; the status line shows `USB`, `BLE adv`, `BLE` (connected, hello seen) or `no host`
- [x] `app_state` reducer: transport kind and BLE status in the state, with the status-line text derived in `screen_text.c`
- [x] write C tests: settings defaults and version handling (with a fake storage), reducer transitions for BLE statuses, status-line text for each state
- [x] run all three suites - must pass before task 6
- ➕ the saved_struct backend lives in `flipper/src/app_settings_file.{c,h}` (SDK) so `app_settings.c` stays host-compiled; the transport menu is its own reducer screen `AppScreenTransport` (scene `scene_transport.c`), so the NFC reader is off and host events do not close the menu; the Idle status line moved under the title because the OK button takes the bottom; `host_link_status_text` is replaced by `screen_status_text`
- ➕ until Task 6, Bluetooth runs `transport_none` (never opens, never connected, `send_line` → not sent), so the USB port stays with the CLI; Task 6 swaps in `transport_ble` and keeps `transport_none` as the fallback when Bluetooth is unavailable

### Task 6: Flipper BLE profile, GATT service and transport
- [x] `flipper/src/ble_lines.{c,h}` (host-tested, no SDK): outbound chunking at `mtu - 3` (20 until an MTU exchange says more, never above 243), one notification in flight until the stack accepts the next, retry bookkeeping when the stack reports busy; inbound bytes into `line_buffer`
- [x] `flipper/src/ble_spoolbuddy_svc.{c,h}`: the profile template (`get_gap_config`: `bonding_mode = false`, `pairing_method = GapPairingNone`, MAC and name from `furi_hal_version_*`, `adv_service` set to the service UUID; `start`: `ble_gatt_service_add` plus RX (write with and without response, variable, 243) and TX (notify, variable, 243) with `ATTR_PERMISSION_NONE`, or `ATTR_PERMISSION_ENCRY_READ | ATTR_PERMISSION_ENCRY_WRITE` when `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION` is 1; `stop`: delete them); an event handler registered with `ble_event_dispatcher_register_svc_handler` that turns RX attribute-modified events into received bytes, the TX CCCD write into "host subscribed", and the MTU exchange response into the chunk size
- [x] `flipper/src/transport_ble.c` implementing `Transport`: `open` records the Bt service, sets the status callback, `bt_profile_start(bt, &spoolbuddy_ble_profile, NULL)`; `send_line` uses `ble_gatt_characteristic_update` on TX through `line_sender` and `ble_lines`; `is_connected` is `BtStatusConnected` and subscribed; `close` disconnects, restores the default profile and closes the record
- [x] `spoolbuddy_app.c`: pick the transport from `app_settings`; the hello is sent when the host subscribes to TX, as it is on USB DTR; the USB CLI stays untouched while BLE is selected
- [x] write C tests for `ble_lines` (chunk boundaries at 20 and 182 bytes, in-flight gating, busy retry, partial lines) and for the event decoding that is pure (attribute-modified payload → bytes, CCCD value → subscribed flag, MTU response → chunk size); `ufbt` builds the FAP
- [x] run all three suites - must pass before task 7
- ➕ the pure parts not named above live in `flipper/src/ble_wire.{c,h}` (host-tested): the UUIDs LSB first (a test checks them against the GATT table), the advertised name and address, and the decoding of raw ACI events by offset (the stack's own structs and codes are not in the SDK; codes copied from the STM32WB copro headers firmware 1.5.1 pins: attribute modified 0x0C01, MTU response 0x0C03, TX pool available 0x0C16, HCI disconnection complete 0x05)
- ➕ the advertisement is 31 bytes: flags (3) and the 128-bit UUID (18) leave room for an 8-char name only, so the profile advertises the bare Flipper name (fallback "Flipper"), not "Flipper <name>"; GAP uses the same as the device name
- ➕ the profile advertises with the Flipper's MAC with byte 2 XOR 0x5B, as the HID profile changes its MAC, so a central never mixes it up with the default profile's bonds and cached GATT table
- ➕ `bt_profile_start` gets the service callbacks as its params (not NULL), so the event handler has its context from the first event
- ➕ TX uses the SDK's callback data type: with the fixed type `ble_gatt_characteristic_update` always sends the full 243 bytes. In SDK 1.5.1 that function returns true on failure (`result != BLE_STATUS_SUCCESS`) after retrying a busy stack for up to a second; `spoolbuddy_ble_notify` inverts it. A busy answer ends the line at once (review fix: the 3 extra retries, each waiting for ACI_GATT_TX_POOL_AVAILABLE, could hold the GUI thread for about 4 s); the SDK waits for each chunk, so a line also stops after a 2 s budget (`BLE_LINES_LINE_BUDGET_MS`, review fix: a stack freeing room slowly but in time never answered busy and held the GUI thread for the whole line), and a line holds the sender for its budget and one SDK wait, about 3 s, at most
- ➕ received bytes go from the BLE event thread through a stream buffer to a worker thread (as on USB), which frames them and decodes the messages; a CCCD write that turns notifications off or a disconnection ends the session (partial line dropped, chunk back to 20). The stream carries length-prefixed frames and a session-end frame written by the same BLE event thread (review fix: a separate session-end flag let the worker drop the next session's `hello` on a fast reconnect, and neither side resends its `hello`)
- ➕ `line_sender` gets `line_sender_set_packet_size` and a 243-byte packet buffer: the BLE transport sends each line in chunks of the current `mtu - 3`; the zero-length packet `line_sender` adds after a full last packet is a no-op in `ble_lines`
- ➕ the app polls `transport_ble_status()` on every tick and reduces `AppEventBleStatus`, so the status line shows `BLE adv`; a backend that does not open (the radio stack did not start or does not support the profile) is freed and `transport_none` takes its place; the reducer records `AppEventTransportFailed`, and choosing the same kind in the menu then retries it (review fix: before, only switching to the other kind and back did). With Bluetooth switched off in the Flipper's Settings the profile still starts but does not advertise: the status line shows `no host`; `application.fam` requires `bt`
- ➕ the profile instance is static, not allocated: the Bt service reads the stopped profile's config while it switches to the next one
- ➕ `ufbt` builds the FAP with `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION` 0 and 1 (APPCHK passes, API 88.2)

### Task 7: iOS app project with XcodeGen
- [x] `host/Apple/project.yml`: target `SpoolBuddyiOS` (iOS 26, iPhone and iPad), depends on the local package products `SpoolBuddyCore` and `SpoolBuddyUI`; `Info.plist` keys `NSBluetoothAlwaysUsageDescription`, `NSCameraUsageDescription`; bundle id `cool.bambuddy.spoolbuddy` placeholder; `Signing.xcconfig` is git-ignored and `Signing.example.xcconfig` carries `DEVELOPMENT_TEAM = `; CI builds with `CODE_SIGNING_ALLOWED=NO`
- [x] `host/Apple/iOS/App.swift`: `@main` SwiftUI app with a `NavigationSplitView` (iPad) / `TabView` (iPhone) over `FeedView` and the settings form from `SpoolBuddyUI`; `HostModel` constructed with the BLE transport factory
- [x] root `Makefile`: `apple-project` (`xcodegen generate` in `host/Apple`), `apple-build` (builds `SpoolBuddyiOS` for `generic/platform=iOS Simulator` with `CODE_SIGNING_ALLOWED=NO`; Task 8 adds the watch scheme); `.gitignore` for `*.xcodeproj` under `host/Apple`
- [x] write tests: the app target has no logic of its own; verify instead that `make apple-project apple-build` succeeds locally (record the command output in the plan) and that `swift test` still passes
- [x] run all three suites and `make apple-build` - must pass before task 8
- ➕ deviations: the Info.plist is generated (`GENERATE_INFOPLIST_FILE` with `INFOPLIST_KEY_*` build settings), so no plist file is checked in; the project's config file is the tracked `host/Apple/Base.xcconfig`, which does `#include? "Signing.xcconfig"`, so generation and unsigned builds work without the git-ignored file; `apple-build` uses `-derivedDataPath host/Apple/.build` (git-ignored); the iPad/iPhone switch is the horizontal size class (regular → `NavigationSplitView` with a Feed/Settings sidebar, compact → `TabView`); the iOS Settings screen builds a fresh `SettingsViewModel` on appear like the Mac Settings window; `tools/tests/test_apple_project.py` guards the spec (target, platform, device family, package products, usage descriptions, bundle id), the signing files and the Makefile targets
- ➕ recorded locally (Xcode 27.1, XcodeGen 2.46): `make apple-project` → "Created project at …/host/Apple/SpoolBuddy.xcodeproj"; `make apple-build` → "** BUILD SUCCEEDED **" (the built `SpoolBuddy.app` carries `cool.bambuddy.spoolbuddy`, both usage descriptions and `UIDeviceFamily` 1, 2); `make test` (Swift 374 tests, pytest 313) and `make lint` pass

### Task 8: Apple Watch app and settings sync from the iPhone
- [x] `host/Package.swift`: add `.watchOS("26.0")` to the platforms; `SpoolBuddyCore` compiles for watchOS (`SerialTransport` already guarded; `KeychainSecretStore` and `URLSession` work unchanged)
- [x] `host/Sources/SpoolBuddyCore/Session/SettingsSync.swift`: `SettingsSyncPayload` (base URL, API key, core weight, catalog entry id, default mode) with a versioned dictionary codec for WatchConnectivity (`v: 1`); unknown versions rejected
- [x] `host/Sources/SpoolBuddyCore/Session/WatchSettingsSync.swift`: on iOS, a `WatchSettingsSender` behind a `SessionMessaging` protocol (`WCSession` adapter) that sends the payload with `updateApplicationContext` whenever settings are saved and the watch app is installed; on watchOS, a `WatchSettingsReceiver` that stores the payload through `SettingsStore` and reconfigures `HostModel`
- [x] `host/Apple/project.yml`: target `SpoolBuddyWatch` (watchOS 26, companion embedded in `SpoolBuddyiOS`, `NSBluetoothAlwaysUsageDescription`, `WKBackgroundModes` not needed); `host/Apple/Watch/App.swift` plus compact views in `host/Sources/SpoolBuddyUI/Watch/` under `#if os(watchOS)`: a feed list (colour swatch, material, remaining, status), a row detail with the weight bound to the Digital Crown (`.digitalCrownRotation`, 1 g steps, 5 g with faster turns) and the net/gross toggle, **Save** and **New**, a settings page that only shows what arrived from the phone and a "Scan for Flipper" button
- [x] `make apple-build` also builds `SpoolBuddyWatch` for `generic/platform=watchOS Simulator`
- [x] write tests: payload codec round trip, version check, missing fields; the sender sends on save and not otherwise (fake session); the receiver stores and reconfigures; crown weight model clamps to 0…9999 and rounds; `make apple-project apple-build` succeeds
- [x] run all three suites and `make apple-build` - must pass before task 9
- ➕ `PlatformInfo` gained the `watchOS` family (the watch says `watchOS 26.x` in its `hello`); the weight parsing helpers on `FeedViewModel` are `nonisolated` so the crown model can use them
- ➕ the crown weight model is `host/Sources/SpoolBuddyCore/ViewModels/CrownWeight.swift` (`clamp`, `turn(to:velocity:)`, `seed(typed:suggested:)`; 5 g steps from 20 g/s); the view binds it with `digitalCrownRotation(detent:…onChange:)` and writes the grams through `FeedViewModel.setWeightText`, so Save and `canSave` are unchanged
- ➕ deviations: the sender holds the latest save until the session is supported, activated and the watch app installed, and sends it on the next state change (activation, `sessionWatchStateDidChange`); the iPhone also hands it the saved settings once at launch when they are configured, so a watch app installed after the last save still gets them. iPad has no WatchConnectivity, so nothing is sent there. `HostModel.onSettingsSaved` runs after every successful `apply`. The receiver merges the payload onto the watch's settings (transport and Flipper stay), skips identical settings (the context is delivered again on every activation) and also reads `receivedApplicationContext` on activation. `catalogEntryId` is optional in the payload (left out when none is chosen); `coreWeight` must be in 1…5000 and booleans are not numbers. The watch's Flipper choice goes through the new `HostModel.useFlipper(_:)`, which keeps the other settings as saved now. `FeedView`, `SettingsForm` and `UnknownRowActions` are `#if !os(watchOS)`; `SwatchView` moved to its own file for both. Watch New edits the tag's prefill in a form; Link to existing stays on the phone and the Mac
- ➕ recorded locally (Xcode 27.1, XcodeGen 2.46): `make apple-project` → "Created project at …/host/Apple/SpoolBuddy.xcodeproj" (the iOS target has an "Embed Watch Content" phase into `$(CONTENTS_FOLDER_PATH)/Watch`); `make apple-build` → "** BUILD SUCCEEDED **" twice; the built `SpoolBuddy.app/Watch/SpoolBuddyWatch.app` carries `cool.bambuddy.spoolbuddy.watchkitapp`, `WKApplication`, `WKCompanionAppBundleIdentifier` `cool.bambuddy.spoolbuddy` and the Bluetooth usage description; `make test` (Swift 413 tests, pytest 315), `make lint` pass; SpoolBuddyCore line coverage 89.6 %

### Task 9: API key from Bambuddy's QR code
- [x] `host/Sources/SpoolBuddyCore/Session/APIKeyQRPayload.swift`: `parse(_ string:) throws -> (baseURL: String, apiKey: String)` for `bambuddy://config?v=1&url=…&key=…`; errors for wrong scheme or host, unsupported `v`, missing or empty fields, invalid percent-encoding; the URL is normalised the way Settings expects (scheme and host, optional port, no trailing slash)
- [x] `SettingsViewModel.applyQRPayload(_:)` fills `baseURL` and `apiKey` and sets a status line; a `QRScanning` protocol with a `scan() async throws -> String` entry so the view model is testable
- [x] iOS only: "Scan QR code" button in the Settings form presenting a VisionKit `DataScannerViewController` (`.barcode(symbologies: [.qr])`) through a `UIViewControllerRepresentable`; unavailable device → the button is hidden
- [x] write tests: valid payload; `bambuddy://config?v=2…` rejected; missing key rejected; percent-decoding of `http%3A%2F%2F192.168.1.10%3A8000`; view model applies and reports; a failing scanner leaves the fields untouched
- [x] run all three suites - must pass before task 10
- ➕ deviations: `parse` returns an `APIKeyQRPayload` struct (`baseURL`, `apiKey`) instead of a tuple, with `ParseError` cases `notBambuddyConfig` (scheme or host), `unsupportedVersion`, `missingField`, `invalidEncoding`, `invalidBaseURL`; fields are split on `&`/`=` before percent-decoding so escaped `&`, `=`, `+` in the key survive, the first of a repeated field wins, values are trimmed; the URL keeps scheme, host, port and any path prefix (Bambuddy sends the origin) and drops credentials, query, fragment and trailing slashes. `SettingsViewModel.applyQRPayload(_:)` takes the scanned text, sets `qrStatus` (`.applied` / `.failed`) and clears the stale `connection` result; `scanQRCode(with:)` runs a `QRScanning` scanner (a `@MainActor` protocol) and treats `CancellationError` as "nothing happened". The iOS scanner is `host/Sources/SpoolBuddyUI/QRScanner.swift` (`CameraQRScanner` bridging the sheet to `scan()`, `QRScannerView` over `DataScannerViewController`, `QRScannerSheet` with Cancel); the button shows only when `DataScannerViewController.isSupported && isAvailable`, so it never appears in the simulator
- ➕ recorded locally (Xcode 27.1, XcodeGen 2.46): `make apple-project apple-build` → "** BUILD SUCCEEDED **" twice (no warnings in the changed files); `make test` (C suites 0 failures, Swift 430 tests, pytest 315), `make lint` pass; SpoolBuddyCore line coverage 89.8 %

### Task 10: CI for the Apple builds
- [x] `.github/workflows/host.yml`: add a job on `macos-latest` that installs `xcodegen` with Homebrew and runs `make apple-project apple-build`; path filters include `host/Apple/**`
- [x] `.github/workflows/README.md` documents the job and the signing policy
- [x] write tests: `tools/tests/test_ci.py` checks the new job and the path filters as it does for the others
- [x] run all three suites - must pass before task 11
- ➕ deviations: the job is `apple` next to `test` in `host.yml`; the path filters also gain the root `Makefile` (the `apple-*` recipes live there), and `host/Apple/**` is listed explicitly although `host/**` covers it. No Xcode selection step: `macos-latest` is the macOS 26 arm64 image, whose default Xcode 26.6 ships the iOS and watchOS 26.5 SDKs and simulators (actions/runner-images, checked 2026-10-26). `run_lines`/`step_dirs` in `test_ci.py` now take one job, since `host.yml` has two; the unsigned build itself is guarded by `test_apple_project.py`
- ⚠️ the Apple build was verified locally only with Xcode 27.1 (`make apple-project apple-build` → "** BUILD SUCCEEDED **" twice); the first CI run is the check against Xcode 26.6. `make test` (C suites 0 failures, Swift 430 tests, pytest 316) and `make lint` pass

### Task 11: Verify acceptance criteria
- [x] verify every bullet of the Overview is implemented
- [x] `protocol/` unchanged (`git diff --stat main...HEAD -- protocol` is empty)
- [x] `make test`, `make lint`, `make coverage` (80 %) pass
- [x] `cd flipper && ufbt` builds the FAP
- [x] `make apple-project apple-build` succeeds for both targets
- ➕ results (2026-10-26): Overview bullets traced to `transport_ble.c`, `app_settings.c`, `scene_transport.c` and `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION` (Flipper); `BLETransport` via `FlipperLink.defaultTransport`, `Settings.transport`/`peripheralId` (host, macOS included); `SpoolBuddyUI`; `project.yml` iOS and watchOS 26 targets, `WatchSettingsSender`/`WatchSettingsReceiver`; `QRScannerSheet` in `SettingsForm`; the `apple` job in `host.yml`. `make test`: C suites 0 failures, Swift 430 tests in 33 suites, pytest 316; `make lint` passes; coverage SpoolBuddyCore 89.8 %, tools 96.5 %; `ufbt -c && ufbt` builds `spoolbuddy.fap` (API 88.2) without warnings; `make apple-project apple-build` → "** BUILD SUCCEEDED **" twice (Xcode 27.1, only the appintentsmetadataprocessor notice)

### Task 12: Documentation
- [x] root `README.md`: Phase 2 section (transport setting on both sides, no pairing, the encryption macro, iPhone/iPad/Watch install from Xcode with free provisioning, settings reach the watch from the phone), the `host/Apple` layout, `make apple-*` targets
- [x] `flipper/README.md`: the transport submenu, status-line values, the note that the official Flipper app cannot connect while SpoolBuddy holds the BLE profile, the GATT table
- [x] `host/README.md`: `SpoolBuddyUI`, `BLETransport`, watch sync, QR scanning, signing files
- [x] `docs/superpowers/specs/…design.md`: record the builder's Phase 2 decisions in "Decisions made by the designer"
- ➕ the encryption macro is documented as `cdefines=["SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION=1"]` in `application.fam` (ufbt passes an app's `cdefines` to its sources); the decisions went under a "Phase 2, decided by the builder" heading in the designer's list; docs only, `make test` (C 0 failures, Swift 430 tests, pytest 316) and `make lint` pass

## Technical Details

### GATT (SpoolBuddy service, defined by the FAP)
| Role | UUID | Properties |
|---|---|---|
| Service | `51C07E84-E37E-4D52-AC6D-B2003116F506` | advertised |
| RX (host → Flipper) | `D250A7B1-D301-43F6-928E-BF2CA5D115BD` | write with response, write without response; variable, ≤ 243 bytes |
| TX (Flipper → host) | `C3AB2366-401D-482C-9002-B0A046C2A4E4` | notify; variable, ≤ 243 bytes |

Permissions `ATTR_PERMISSION_NONE` (or `ENCRY_READ | ENCRY_WRITE` with the
macro). Chunks ≤ ATT MTU − 3 on both sides (20 until the MTU exchange).
Flow control: one write in flight from the host, one notification in flight
from the Flipper. Lines ≤ 8192 bytes.

### Flipper BLE lifecycle
```
open:  furi_record_open(RECORD_BT) → bt_set_status_changed_callback →
       bt_profile_start(bt, &spoolbuddy_ble_profile, NULL)
       (start: ble_gatt_service_add + 2 × ble_gatt_characteristic_init,
        ble_event_dispatcher_register_svc_handler)
rx:    ACI_GATT_ATTRIBUTE_MODIFIED on RX handle → ble_lines → line_buffer → rx callback
       ACI_GATT_ATTRIBUTE_MODIFIED on TX CCCD → subscribed → hello
       ACI_ATT_EXCHANGE_MTU_RESP → chunk size = mtu - 3
tx:    ble_gatt_characteristic_update(TX, chunk) → next chunk (busy ends the line)
close: bt_disconnect → bt_profile_restore_default (stop: delete service,
       unregister handler) → furi_record_close
```

### Settings on the Flipper
`/ext/apps_data/spoolbuddy/settings.bin`, `saved_struct` magic `0x5B`,
version 1: `{ transport: 0 = USB, 1 = Bluetooth }`.

### Watch settings payload (WatchConnectivity application context)
`{"v": 1, "baseURL": "...", "apiKey": "...", "coreWeight": 325, "catalogEntryId": 24, "defaultMode": "gross"}`

### QR payload
`bambuddy://config?v=1&url=<percent-encoded base URL>&key=<percent-encoded key>`.
Only `v=1` is accepted.

### Repository additions
```
host/Sources/SpoolBuddyCore/Transport/BLEChunker.swift
host/Sources/SpoolBuddyCore/Transport/BLE/{SpoolBuddyGATT,Central,BLETransport,CoreBluetoothCentral}.swift
host/Sources/SpoolBuddyCore/Session/{SettingsSync,WatchSettingsSync,APIKeyQRPayload}.swift
host/Sources/SpoolBuddyUI/Watch/            compact watch views
host/Sources/SpoolBuddyUI/QRScanner.swift   iOS camera QR scanner
host/Apple/project.yml, host/Apple/iOS/App.swift, host/Apple/Watch/App.swift,
host/Apple/Signing.example.xcconfig
flipper/src/app_settings.{c,h}, ble_lines.{c,h}, ble_wire.{c,h}, ble_spoolbuddy_svc.{c,h},
transport_ble.{c,h}, scene_transport.c
```

## Post-Completion
*Items requiring manual intervention or external systems - no checkboxes, informational only*

**Manual verification** (owner, with the Flipper, the Mac, an iPhone, an iPad and the watch):
- Flipper: Idle → OK → Transport: Bluetooth; the status line shows `BLE adv`.
- Flipper with Bluetooth switched off in Settings → Bluetooth: the FAP on
  Bluetooth shows `no host` (nothing advertises); switching it on there
  and starting the FAP again brings `BLE adv`.
- Mac: Settings → Transport Bluetooth → Scan → pick the Flipper by its name (the advertisement has room for the bare name only); no
  pairing prompt; status `BLE`; scan a spool and weigh it; compare with USB.
- iPhone/iPad: `make apple-project`, open `host/Apple/SpoolBuddy.xcodeproj`
  in Xcode, sign with the personal team (free provisioning, 7-day
  certificate), run on the device; scan the API-key QR from Bambuddy's key
  dialog, Test connection, weigh a spool.
- Watch: install the watch app from the iPhone, check the settings arrived,
  Scan for Flipper, weigh a spool with the crown.
- Encryption trial: build the FAP with `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION=1`
  and see whether the watch, the phone and the Mac pair "just works"
  without a prompt; if all three do, make it the default.
- Long lines: a `scan` with 64 blocks (~2.8 KB) must arrive intact over BLE
  in both directions; watch for stalls and reordering.
- Leaving the FAP restores the default BLE profile: the official Flipper
  app connects again afterwards.

**External systems**:
- Apple Developer account and TestFlight when the apps are ready (later).
- App icons and bundle ids are placeholders until then.
