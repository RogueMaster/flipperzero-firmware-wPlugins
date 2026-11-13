# FliBuddy host (Swift package)

The package builds with SwiftPM only; the iPhone/iPad and Apple Watch apps
in `Apple/` are thin XcodeGen shells over it (see below). No UI tests.

- `FliBuddyCore`: platform-neutral library (protocol models, tag parser,
  Bambuddy client, transport, session, view models). `TagParser` takes the
  tray UUID from block 9 of a Bambu tag, the block whose value matched the
  AMS-reported `tray_uuid` in Phase 0. Lookups try it first, then the UID,
  and new items and tag links store it. The UID goes to Bambuddy in the
  form an AMS writes (`ParsedTag.bambuddyTagUid`): for Bambu tags the UID
  followed by `00000100` (`7EF1AB04` → `7EF1AB0400000100`), NTAG UIDs as
  read; see `protocol/PROTOCOL.md`.
- `FliBuddyUI`: SwiftUI views shared by the macOS and iOS apps (the feed,
  the iPhone connection banner, the iPad sidebar and menu commands, the
  settings form with the Transport section, the New, Link and Tag details
  sheets, the iOS QR scanner); macOS-only parts (the Settings link, the
  serial port picker) sit behind `#if os(macOS)`. `Watch/` holds the
  compact watch views (`#if os(watchOS)`): one navigation stack with the
  feed list, a row detail with the weight on the Digital Crown, the
  read-only settings and Scan for Flipper. `FeedView`, `SettingsForm` and
  `RowActions` are not built for watchOS.
- `FliBuddyMac`: SwiftUI executable for macOS 14+: the app and the
  Settings window around `SettingsForm`.

The package supports macOS 14+, iOS 26+ and watchOS 26+. Off macOS
`SerialTransport` is a stub (no ports, connecting fails) and the `hello`
names the platform through `PlatformInfo` (`macOS 26.0`, `iOS 26.1`,
`iPadOS 26.1`, `watchOS 26.1`). To build the shared targets or run the
tests on the iOS simulator:

```
xcodebuild -scheme FliBuddyUI -destination 'generic/platform=iOS Simulator' build
xcodebuild test -scheme FliBuddy-Package -destination 'platform=iOS Simulator,name=iPhone 17'
```
- `FliBuddyCoreTests`: swift-testing suites (`import Testing`). The Command
  Line Tools have no XCTest, so the tests do not use it.

## Commands

```
cd host
swift build
swift test
swift run FliBuddyMac
```

If Xcode.app is installed but its license is not accepted, `swift` from
`/usr/bin` exits with 69. Use the Command Line Tools instead:

```
DEVELOPER_DIR=/Library/Developer/CommandLineTools swift test
```

## Running the Mac app

```
cd host
DEVELOPER_DIR=/Library/Developer/CommandLineTools swift run FliBuddyMac
```

`swift run` starts a plain executable, not an `.app` bundle; the app makes
itself a regular app (Dock icon, menu bar) at launch. Settings (⌘,) hold the
Bambuddy URL and API key (stored in the login Keychain, service
`FliBuddy`), the empty-spool catalog entry and the editable core weight
used for new items, the default weight mode and the transport: USB with
the Flipper port ("Automatic" opens the first `/dev/cu.usbmodemflip_*`), the
default on the Mac, or Bluetooth (see below). Use **Test connection** after
entering the key; it also loads the catalog. Nothing applies until **Save**
(Return); **Revert** drops the edits, and the line beside them reads
"Unsaved changes" or "All changes saved".

`swift run` has no app bundle, so the Bluetooth usage description macOS asks
for lives in `Sources/FliBuddyMac/Info.plist`, linked into the executable's
`__TEXT,__info_plist` section with `-sectcreate` (`Package.swift`, macOS
only). It has no `CFBundleIdentifier` on purpose: one would move
`UserDefaults.standard` away from the `FliBuddyMac` domain that holds the
saved settings.
`SettingsStore` keeps the settings in a `KeyValueStore` (`UserDefaults` in
the apps, `InMemoryKeyValueStore` in the tests) and the API key in a
`SecretStore` (`KeychainSecretStore` in the apps).

The core weight is everything on the scale that is not filament (`net =
gross − core_weight`). It defaults to 242 g, a Bambu reusable spool (215 g)
plus a refill core (27 g), which fits sealed spools and mounted refills alike.
Choosing a catalog entry copies its weight once; the number stays editable
(1–5000 g) and is saved as entered; a value saved before (e.g. 250 g)
is kept, not replaced by the new default. The entry itself is sent only as
`core_weight_catalog_id`. AMS-added spools get their core weight from the
first "Bambu Lab" catalog entry in Bambuddy, so set that entry to 242 g as
well to keep both kinds of spool consistent.

After the startup check the session loads Bambuddy's colour map
(`GET /inventory/colors/map`, inventory read) once and keeps it; another
URL or API key reloads it (other settings changes leave the feed as it is). New items get `color_name` from it: the detailed type
and RGB first (`by_material`, e.g. `pla basic|ff9016`), then the RGB alone
(`colors`); with no match `color_name` is left out rather than set to the
hex. A failed load shows in the status bar, leaves names empty and never
blocks a scan; the next startup check retries it.

The view models (`FliBuddyCore/ViewModels`) are platform-neutral and
tested with `swift test`; the SwiftUI views in `FliBuddyUI` and
`FliBuddyMac` have no tests.

## The Mac window

The layout and copy follow `docs/design/macos-ui-spec.md`; its last section
lists where the app differs and why.

The window (default 1000 × 600, minimum 760 × 420) shows the session feed,
newest scan first, one line per tag: swatch, title, colour (with Bambu's
colour code, `Jade White · 10501`) and remaining, status, then the weight
entry (field, Net/Gross, **Save**) for rows that wait for a weight. A scan
of a tag already in the feed moves its row to the top and reopens it; no
second row. The newest row that waits
takes focus with the suggestion selected (remaining + core weight in gross,
remaining in net), so a correct suggestion costs one keystroke. While a
field holds a number you typed, a new scan does not take focus: the new row
gets a tinted outline and waits. After a save, focus moves to the newest
row still waiting. Switching Net/Gross converts the number in the field,
typed or not (gross − core, not below 0, or net + core). The readout under
the field shows what Save sends ("Saves 590 g left · 832 − 242 g core").
The row's context menu (or its VoiceOver action) opens **Tag details**: UID,
tray UUID, filament id, colour code, RGBA, label weight, diameter and
nozzle range (`TagDetails`).

### Keyboard

| Action | Keys |
|---|---|
| Save the focused row | ⌘S, or Return in the field |
| Next / previous row | ⌘↓ / ⌘↑ |
| Field → mode → Save | ⇥ (⇧⇥ back; the picker and Save need keyboard navigation on) |
| Switch Net/Gross when the picker has focus | ← → |
| Clear the feed (asks first; nothing changes in Bambuddy) | ⌘⌫ |
| Settings | ⌘, |

### Row states

| Status | Meaning | What the row offers |
|---|---|---|
| Looking up | the host is asking Bambuddy about the tag, or creating it | nothing yet |
| Enter weight | a known spool | the weight entry |
| Saving… | the weight is on its way | the entry, disabled |
| Saved 590 g left · by host / by Flipper | the last save and who made it; after a host save that came later than the Flipper's 10 s, a note says the Flipper showed a timeout but the save went through | click the row (or the VoiceOver "Re-weigh" action) to weigh again |
| Not in Bambuddy | a tag with material and no spool: an unknown Bambu tag with Auto-save new off, or a spool that went away (the link after deleting the added spool failed); the row does not wait for a weight | **New** adds the spool from the tag at its label weight; **Link to existing…** |
| Added to Bambuddy | after **New**, or an unknown Bambu tag created on scan (the Flipper's **Auto-save new** is on) with the tag's material, colour and label weight and the core weight from Settings; it does not wait for a weight | click the row to weigh it (the entry opens pre-filled with label + core); when the Flipper asks to relink it, **Link to existing…** deletes the spool just added and links the tag to the chosen one |
| Not in Bambuddy; tag has no material | an NTAG or blank tag | **New…** (the New spool form) and **Link to existing…** |
| Updating… | Mark as new / empty or Delete from the Flipper on its way | the entry, disabled |
| Marked as new · 1000 g left · by Flipper | the spool is full again (restored first when it was archived) | click the row to weigh |
| Marked as empty · archived · by Flipper | remaining 0 and archived; the colour column reads `… · archived` | click the row to weigh; a Save restores the spool |
| Deleted from Bambuddy · by Flipper | the Flipper deleted the spool | **New** (**New…** on a tag without material) and **Link to existing…** |
| Flipper asks to create / link it here | the Flipper sent a request for this tag | the requested action is the prominent button |
| Error: … | the last Bambuddy call failed | on a spool, the entry keeps its value for a retry; after a failed New, Link or create on scan, **New** and **Link to existing…** |

Whether an unknown Bambu tag is created on scan is up to the Flipper: its
Auto-save new setting travels in every `scan` (`autosave`), and the host has
no switch of its own. Lookups include archived spools, so a spool marked
empty and scanned again reads `Enter weight` with `… · archived` in the
colour column, and its Save restores it first. After New, Link, or a Save
that created the spool, the host pushes a fresh `lookup` for the row's scan
so a Flipper still showing that tag shows the spool.

A rescan of a tag created on scan finds that spool by tag and never creates
a second one. If the delete behind **Link to existing…** fails, nothing
changes and the row says so; if the link fails after the delete, the row
has no spool and offers New and Link again.

### Status bar

Two items, the Flipper link and the Bambuddy check, each a phrase with a
symbol and a tint; the tooltip holds the detail (port, firmware, error).
The link reconnects on its own when the Flipper app is restarted or the
cable is replugged. Some items carry a button:

| Item | Button |
|---|---|
| Flipper nearby, not connected (the link is stopped) | Connect |
| Bambuddy · not checked | Check |
| Bambuddy unreachable | Retry |
| Bambuddy is in Spoolman mode (also replaces the feed until it is fixed) | Retry |
| API key lacks inventory permission, Bambuddy rejected the API key, Bambuddy not set up | Settings |

Connect starts the link with the saved transport; Check and Retry re-run
the startup check (`HostModel.perform(_:)`); Settings opens the Settings
window. The other items have no button: "No Flipper · searching" (spinner),
"Flipper · USB", "Flipper · Bluetooth", "Open FliBuddy on the Flipper" (the
port is open but no `hello` came within 2 s), "Flipper speaks protocol 1,
host speaks 2" (the Flipper answered in another protocol version; the
tooltip says which side to update), "Checking Bambuddy…",
"Bambuddy ready" and "Bambuddy ready · colour names unavailable".

## Bluetooth (`BLETransport`)

`Transport/BLE/BLETransport.swift` implements `Transport` over the FAP's
GATT service (UUIDs in `FliBuddyGATT.swift`, the table in
`flipper/README.md`). `connect()` waits for Bluetooth to power on,
connects to the remembered peripheral if the system still knows it, else
scans for the service (a scan that finds nothing gives up after 10 s with
`peripheralNotFound`, "No Flipper found over Bluetooth", and so does a
remembered Flipper that never connects: the system knows it even when it
is off or away; a Flipper this scan found that does not connect, or one
that connected and does not open the service, within 10 s fails with
`connectTimedOut`, "Flipper did not answer"), discovers RX and TX, subscribes to TX and only then
reports `.open`, so the `hello` the Flipper sends on subscription is kept.
`BLEChunker` cuts outgoing lines into writes of
`maximumWriteValueLength(for: .withoutResponse)`, which is ATT MTU - 3 (at
most 243 bytes), sends one write with response at a time, and reassembles TX notifications into lines
with `NDJSONFramer`. A disconnect, a failed write or Bluetooth turning off
finishes the message stream, and `FlipperLink` reconnects as it does on
USB. Errors: `bluetoothOff`, `bluetoothUnauthorized`,
`bluetoothUnsupported`, `peripheralNotFound`, `connectTimedOut`,
`serviceNotFound`.

CoreBluetooth sits behind two protocols in `Central.swift`: `BLECentral`
(power, scan, retrieve, connect, cancel) and `BLEPeripheralLink`
(discovery and subscription, write, events). `CoreBluetoothCentral` is the
real adapter for macOS, iOS and watchOS; the tests drive `BLETransport`
with a fake central and never create the real one, which would ask for
Bluetooth permission. `CoreBluetoothCentral` itself talks to the manager
through a thin `CentralManager` protocol (`SystemCentralManager` wraps
`CBCentralManager`), so its connection bookkeeping is tested on a fake
manager: a connect waits for the disconnect of an attempt it cancelled,
unless the peripheral already reads disconnected (iOS may never send that
callback for a cancelled pending connect); power off, and also resetting
or unknown, drop every connection and attempt.

On iOS the central carries a restore identifier. When iOS relaunches the
app in the background for a Bluetooth event, SwiftUI may never run the
views' tasks, so the app delegate (`Apple/iOS/App.swift`) creates the
central and starts the link in `didFinishLaunching`, before it returns,
when the launch is such a relaunch or a Bluetooth Flipper is remembered
(`HostModel.startsAtLaunch`). A fresh install creates the central later,
from the views. The delegate also reads the API key again when protected
data becomes available.

`Settings` holds `transport` (`usb` default on macOS, `bluetooth`
elsewhere, where USB is not offered), `peripheralId` and `peripheralName`.
**Scan for Flipper** in the settings lists the Flippers found for 15 s
with their RSSI (the name from the peripheral, the advertised local name or
the scan response's manufacturer data, `FliBuddyGATT.displayName`; the FAP
advertises no name); choosing one stores it and selects Bluetooth. **Forget**
clears the remembered Flipper of the chosen transport (the USB port back to
Automatic, the Bluetooth peripheral to the first found); like every other
field it takes effect on **Save** on the Mac and at once on the iPhone and
iPad. `HostModel` reconnects when the transport choice changes (kind, port
or peripheral), not on a rename. No pairing: the link
is unencrypted and carries no secrets.

## iPhone, iPad and Apple Watch apps (`Apple/`)

```
Apple/project.yml                XcodeGen spec: FliBuddyiOS (iPhone, iPad)
                                 with the FliBuddyWatch companion embedded
Apple/iOS/App.swift              NavigationSplitView with the sidebar in
                                 a landscape iPad window, else TabView;
                                 Feed, Spools and Settings; tiles on the
                                 iPad, cards at a compact width (FeedLayout)
Apple/Watch/App.swift            the watch app over the Watch/ views
Apple/Base.xcconfig              tracked; includes Signing.xcconfig if present
Apple/Signing.example.xcconfig   copy to Signing.xcconfig (git-ignored) and
                                 set DEVELOPMENT_TEAM
```

The iPhone, iPad and watch accept only `https://` Bambuddy addresses
(`Settings.requiresHTTPS`; no App Transport Security exception, an `http://`
address is refused with a message); the Mac also accepts plain http.

The apps have no logic of their own: everything is in `FliBuddyCore`
and `FliBuddyUI`, used as local package products. The Info.plists are
generated from `INFOPLIST_KEY_*` settings (Bluetooth on both, camera on the
iPhone app). `make apple-project` runs `xcodegen generate` and
`make apple-build` builds both schemes for the simulators with
`CODE_SIGNING_ALLOWED=NO` into `Apple/.build`; both need Xcode, not the
Command Line Tools. To run on a device, fill in `Signing.xcconfig` with a
team (a free personal team works; the install lasts 7 days), regenerate,
and run `FliBuddyiOS` from Xcode; the watch app installs with it.

### iPhone and iPad feed

`FeedLayout.forWindow` picks the layout from the window measured at the
root: a compact width (an iPhone, a narrow Split View or Stage Manager
window) gets `TabView` with stacked cards; a regular width gets the
sidebar (`NavigationSplitView`) when the window is wider than tall and
`TabView` (iPadOS draws its tab bar at the top) otherwise, both with the
feed as tiles in a `LazyVGrid`: at least 300 pt wide, at most three to a
row (`FeedLayout.tileColumns`), so a 13" iPad fits three and an 11" two.
The iPhone is portrait only (`project.yml`): no landscape iPhone is designed.

On a first run the feed shows an empty state per missing step (no Bambuddy
set up, no Flipper chosen). The iPhone and iPad do not start Bluetooth, so
do not show its permission prompt, until a Flipper is chosen with **Scan for
Flipper** (`linkWaitsForChosenFlipper`); the watch connects to the first
Flipper found.

The iPhone and the portrait iPad show a glass banner over the feed
(`ConnectionBanner`) in place of the Mac's status bar: one line when one
side is in trouble, two when both are (the Flipper first), each with its
action chip. A landscape iPad shows both states in the sidebar. A save plays
the success haptic and a failed host action the error one, on the iPhone and
on the watch. The iPhone and iPad get the Mac's shortcuts above as menu
commands (`FeedCommands`).

### Spools on iPhone and iPad

The Spools tab (`SpoolsViewModel`, `FliBuddyUI/SpoolsView.swift`; ios spec,
"Spools") lists Bambuddy's inventory, read-only. One request per load,
`GET /inventory/spools?include_archived=true` through
`Session.listSpools(includeArchived:)`, so it goes through the startup check
like the feed and shows the same refusals (no server, Spoolman mode, a key
that cannot read the inventory or that Bambuddy rejects); the catalog names
the empty spool in the detail, loaded once per settings. Scope (Active,
Archived, All), search (`LinkPickerModel.filter`), material and sort work
on the device. `updated_at` comes without a zone and means UTC
(`BambuddyDate`); a row reads "updated 3 d ago" from the model's clock.

The tab loads each time it comes on screen and on pull to refresh; the iPad
sidebar loads it once for its count. `SpoolsViewModel.observe()` (started
by the app next to the feed's) loads again whenever
`SessionSnapshot.inventoryRevision` moves: the session counts every write
that went through to Bambuddy (a save, New, Link, Mark, Delete, a create on
scan), not lookups. The last good list stays when Bambuddy does not answer
("Couldn't refresh · showing 10:42"); a list from another URL or key is
never shown (`HostModel.apply` calls `serverChanged()`), and the other
failures replace it with their state. A spool opens in an inspector beside
the iPad sidebar (`FeedLayout.opensSpoolInInspector`, 320 pt) and in a
sheet otherwise; **Open in Bambuddy** opens `<URL>/inventory?spool=<id>`,
Bambuddy's inventory page with the spool's form. Loads log `spools: loaded
n=… archived=…` or `spools: load failed: …` in the `session` category.

### Settings on iPhone and iPad

`makeSettingsViewModel(appliesOnChange: true)` removes Save and Revert.
Picker changes, Forget and a QR read apply at once; the text fields (URL, API
key, core weight) apply on `commit()`, that is Return or leaving the field,
and leaving the Settings tab or pane commits the field being typed and stops a Flipper scan (`left()`; an iPad rotation, which builds the form again, does neither). Each change
applies on top of the last applied settings, in order. A core weight out of
range stays unapplied and sets `coreWeightError`; the field can be emptied
while it is edited. The catalog loads each time Settings comes on screen
(`SettingsViewModel.opened()`, the Mac's Settings window too); there is no
Reload on the iPhone and iPad. Forget This Flipper sits in its own section
with the app's version and build under it (`appVersionFooter`).

### Settings sync to the watch

`Session/SettingsSync.swift`: `SettingsSyncPayload` is the Bambuddy URL,
API key, core weight, catalog entry id (left out when none) and default
mode, encoded as a WatchConnectivity application context with `v: 1`;
other versions and malformed fields are rejected.
`Session/WatchSettingsSync.swift` works behind a `SessionMessaging`
protocol (`WatchConnectivitySession` wraps `WCSession`):

- iPhone: `WatchSettingsSender` sends the payload with
  `updateApplicationContext` 0.5 s after the last applied change
  (`HostModel.onSettingsSaved`, debounced so a quick run of changes sends
  once) and once at launch when the settings are complete. A save it
  cannot send yet (session not activated, no watch app) is held and sent on the next
  session change. The iPad has no WatchConnectivity and sends nothing.
- Watch: `WatchSettingsReceiver` merges the payload onto the watch's own
  settings (the transport and the Flipper stay), stores it through
  `SettingsStore` (the key in the watch's Keychain), reconfigures
  `HostModel`, and ignores a payload equal to what it has. It also reads
  `receivedApplicationContext` on activation.

The watch then talks to Bambuddy itself. Its settings page shows what
arrived, read-only (`WatchSettingsTexts`), and the Flipper, with **Scan for
Flipper** on a screen of its own. The weight model behind the Digital Crown
is `ViewModels/CrownWeight.swift` (0…9999 g, 1 g per detent, 5 g from 6
detents per second); `WatchNavigation` and `WatchRowScreen` decide which
screen shows, when a new waiting row opens and when a save pops back.

### API key from Bambuddy's QR code

`Session/APIKeyQRPayload.swift` parses
`bambuddy://config?v=1&url=<percent-encoded URL>&key=<percent-encoded key>`
(only `v=1`); the URL is normalised as Settings expects it.
`SettingsViewModel.applyQRPayload(_:)` fills the URL and key and reports
in `qrStatus`; `scanQRCode(with:)` takes any `QRScanning` scanner, so it is
tested without a camera. On iOS **Scan QR code** opens
`FliBuddyUI/QRScanner.swift`, a full-screen cover over VisionKit's
`DataScannerViewController` reading QR codes; a read applies the URL and
key and starts **Test connection**. Where the scanner is not supported or
available (always in the simulator) the row is dimmed and its footer says
to type them instead.

### Demo mode and simulator screenshots

Launched with `-FliBuddyDemo YES`, a Debug build of the iPhone/iPad and
watch apps (`make apple-build`, `tools/sim_shot.sh`,
`tools/store_shots.sh`) runs on fixtures instead of Bluetooth and Bambuddy
(`DemoFixtures.isAvailable`; a Release build, the App Store one, ignores
every demo argument). The fixtures (`FliBuddyCore/Demo`,
`DemoFixtures.makeHost()`) are a connected Flipper "Atgizo" (`DemoTransport`)
and an in-memory Bambuddy in internal mode (`DemoInventory`, twelve spools,
a catalog and colour names), with settings in in-memory stores
(`https://bambuddy.example.com`, key `demo-key`), so nothing reaches the
network, `UserDefaults` or the Keychain, and the watch settings sync stays
off. On the first connect the demo Flipper plays a script into the session
at a still clock (the rows follow `seed()` and `INV` in
`docs/design/source/flibuddy-core.js`), newest first: PLA Basic Bambu Green
waiting for a weight (590 g of 1000, 832 g gross suggested), a blank NTAG,
PLA Pure not in Bambuddy, Pumpkin Orange saved by the Flipper, PLA Silk+
added on scan, PETG HF the Flipper asks to link, and PLA Matte marked empty
and archived, the last scan at 09:41. Save, New and Link to existing work
on the in-memory inventory, which stamps `updated_at` with the demo clock,
so the Spools tab follows them; Scan for Flipper finds Atgizo.
`-FliBuddyDemoPane spools` (or `settings`) opens the demo on that pane.
`-FliBuddyDemoState fresh` starts it as a fresh install (no settings, no
Flipper), `-FliBuddyDemoState noFlipper` with Bambuddy set up and no
Flipper chosen, for the first-run screens; Scan for Flipper then connects
Atgizo and plays the feed. The
arguments are not reachable from the UI and nothing user-facing mentions
them.

`tools/sim_shot.sh` takes a screenshot of the demo in a simulator, after
`make apple-project apple-build`:

```
tools/sim_shot.sh "iPhone 18 Pro Max" /tmp/iphone.png
tools/sim_shot.sh -a dark -s extra-large "iPad Pro 13-inch (M5)" /tmp/ipad.png
tools/sim_shot.sh "Apple Watch Series 12 (46mm)" /tmp/watch.png
tools/sim_shot.sh "iPhone 18 Pro Max" /tmp/spools.png -FliBuddyDemoPane spools
```

It takes a device name or UDID, boots it if needed, sets the appearance
(`-a light|dark`), the Dynamic Type size (`-s`, a `simctl ui content_size`
category, default `large`) and the 9:41 status bar on iPhone and iPad
(watch simulators support none of these), installs the app from
`Apple/.build`, launches it with the demo argument and any arguments after
the output path, waits (`-w`, default 4 s) and saves a PNG. `simctl` cannot rotate a device, so an iPad stays in
portrait unless it was turned in the Simulator app.

### App Store packaging

Both apps carry a `PrivacyInfo.xcprivacy` (no tracking, no collected data,
`UserDefaults` with reason CA92.1, the only required-reason API in
`host/Sources`; a test fails if another one appears) and set
`ITSAppUsesNonExemptEncryption` to NO. `MARKETING_VERSION` is 1.0.0 for both.

```
make apple-archive
```

archives the `FliBuddyiOS` scheme (Release, `generic/platform=iOS`, the watch
app embedded) with `CURRENT_PROJECT_VERSION` set to `git rev-list --count
HEAD` for both apps, then exports a signed `.ipa` for App Store Connect into
`host/Apple/.build/export` (`ExportOptions.plist`; the team comes from
`DEVELOPMENT_TEAM` in `Signing.xcconfig`). It needs Xcode signed in to the
paid Apple Developer account and an App Store Connect app record, and it
uploads nothing. The release log level is `info`.

`tools/store_shots.sh [dir]` captures the App Store screenshots (iPhone 18
Pro Max 1320x2868, iPad Pro 13-inch 2064x2752 in portrait, Apple Watch
Series 12 46 mm 416x496) with `sim_shot.sh` into `host/Apple/.build/store-shots`
after `make apple-project apple-build`. The watch simulator keeps its own
clock, so the watch shot does not show 9:41.

### `@ViewState` instead of `@State`

The macOS 27 SDK declares `@State` as a macro too, implemented in the
`SwiftUIMacros` compiler plugin that only Xcode ships. With the Command Line
Tools, `@State` fails with "plugin for module 'SwiftUIMacros' not found".
`Sources/FliBuddyUI/ViewState.swift` aliases the `State` property wrapper
struct as a public `ViewState`; use `@ViewState` in both SwiftUI targets.

## Debugging

Every link, wire, session, Bambuddy and settings event is logged through
`FliBuddyCore/Logging/Log.swift`, both to the unified log and to a plain
text file:

- macOS: `~/Library/Logs/FliBuddy/flibuddy.log` (Help ▸ **Show Log File**
  reveals it in Finder)
- iOS, iPadOS and watchOS: `Logs/flibuddy.log` in the app's Application
  Support directory (download the container from Xcode's Devices window);
  on the iPhone and iPad Settings ▸ **Share Log** sends the current file.
  The `Logs` directory is excluded from backups.

The file has one event per line, `2026-10-26T13:04:05.123+0300 INFO link
state: waitingForHello -> connected`, and rotates at 5 MB to `flibuddy.log.1`,
`.2` and `.3`. The first line of each run is `app start: …` with the
version, platform, log file and level. Follow it while scanning a tag:

```
tail -f ~/Library/Logs/FliBuddy/flibuddy.log
```

The level is `info` by default. The Mac app reads `FLIBUDDY_LOG`
(`debug`, `info`, `notice`, `error`); `debug` adds the wire lines and
Bambuddy's response bodies:

```
cd host
FLIBUDDY_LOG=debug swift run FliBuddyMac
```

The iPhone, iPad and watch apps always log at `info`. The same lines go to
the unified log, subsystem `com.sokolachko.flibuddy`, with one category
per kind of event:

```
log stream --level debug --predicate 'subsystem == "com.sokolachko.flibuddy"'
log show --last 1h --info --predicate 'subsystem == "com.sokolachko.flibuddy" && category == "bambuddy"'
```

The unified log keeps `info` and `debug` lines in memory only, so `log show`
finds them for a short while at best; the file keeps them.

| Category | Records |
| --- | --- |
| `app` | the start line, written whatever the level; a log file that cannot be written (unified log only, once); on iOS whether the link starts at launch (`bluetooth_relaunch=`) and a background task whose time ran out |
| `link` | connect and disconnect, link state changes with the problem, retries, the serial port opening and closing (with `errno`), Bluetooth power (also resetting and unknown), scan, peripherals found (name, RSSI), services, MTU, disconnects, a cancelled attempt cleared without its disconnect, the hello handshake |
| `wire` | each protocol line sent (`tx`) and received (`rx`) at `debug`, cut to 300 characters; dropped lines at `error` with the reason |
| `session` | Bambuddy state changes, scans, lookups with their outcome and time, intents and results, creating, linking and deleting spools, a spool gone from Bambuddy and looked up again by tag, colour map failures |
| `bambuddy` | each request's method, path, status and time; errors with Bambuddy's `detail`; response bodies at `debug` |
| `settings` | settings load and save (`key=present`, `absent` or `unknown`: the Keychain could not be read, e.g. a launch while the iPhone is locked; the key is kept and read again on unlock; `protected data unavailable` before the first unlock since the device started, when no setting can be read: nothing is saved or sent to the watch until they are read again), each apply (`server=changed` reconfigures the session, `same` keeps the feed), Keychain failures with the `OSStatus`, **Test connection** and catalog loads |
| `sync` | watch settings sync: payloads sent, held, received, applied or rejected, by version only |
| `ui` | reserved; nothing logs to it yet |

The API key is never logged: settings lines only say whether a key is
present, and `Log.redact` replaces any `bb_…` key, any `key=`, `X-API-Key`
or `apiKey` value and the `user:password@` of a URL with `«redacted»` before
a line reaches the unified log or the file. Neither the QR payload nor the
sync payload is logged.

## Protocol fixtures

The tests read the shared contract in `protocol/fixtures/` straight from
the checkout: `Fixtures.root` in `Tests/FliBuddyCoreTests/ProtocolTests.swift`
walks up from its own `#filePath`, so added, changed and removed fixtures
count on the next run. There is no resource bundle: an earlier symlink
copied as a resource worked with Xcode 27's SwiftPM, but the one in CI
(Xcode 26.6) copied the link itself, which pointed nowhere inside the bundle.

## swift-testing with the Command Line Tools

The Command Line Tools (Swift 6.4) install the swift-testing macro plugin in
`usr/lib/swift/host/plugins/testing/`, but its rpath is `@loader_path/..`,
which points at `plugins/` instead of `host/` where the swift-syntax
libraries live. The compiler then fails with "plugin for module
'TestingMacros' not found".

`Toolchain/clt-testing-plugin/` mirrors the layout the plugin expects with
symlinks into the Command Line Tools. When `DEVELOPER_DIR` starts with
`/Library/Developer/CommandLineTools`, `Package.swift` adds it to the test
target with `-plugin-path`. With Xcode (CI) nothing changes. Remove the
workaround once a Command Line Tools release fixes the rpath.
