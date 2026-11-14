# FliBuddy: design

Date: 2026-10-25
Renamed: 2026-10-26, from "Flipper SpoolBuddy" to FliBuddy (plan
`20261026-flibuddy-rename.md`). SpoolBuddy is Bambuddy's own kiosk
device; references to it below are unchanged. This file keeps its old
name because completed plans link to it.
Status: Phase 0 tooling and Phase 1 (Flipper + macOS over USB) are
implemented (`docs/plans/20261025-flipper-spoolbuddy-phase0-1.md`); the
Phase 0 hardware checks ran on 2026-10-26 (see "Phase 0 findings"), the
remaining ones are listed in README, "Phase 0 results". Phase 2 (BLE,
iPhone, iPad and Apple Watch apps) is implemented
(`docs/plans/20261026-phase2-ble-ios.md`); its hardware checks are listed
in that plan's Post-Completion.
The Flipper UI rework and Auto-save (`docs/plans/20261026-flipper-ui-autosave.md`)
replaced the Flipper screens, the Settings and the protocol (now v2): where
"The Flipper app" and the sections below describe the Phase 1 screens (Idle,
the Idle OK menu, Enter weight), `docs/design/flipper-ui-spec.md` and
`protocol/PROTOCOL.md` are current.

## Intent

Keep the filament inventory in Bambuddy accurate without buying the
SpoolBuddy hardware (Raspberry Pi, PN5180, touchscreen, load cell). The
owner already has a Flipper Zero and a Mac, iPad and iPhone. The Flipper
reads the spool's NFC tag; a host app on one of the Apple devices talks to
Bambuddy. Weight is read off an ordinary kitchen scale and typed in, on
either the Flipper or the host.

The owner's Bambuddy runs in Docker containers on a Synology NAS (see the
`bambuddy-deploy` repository). Nothing in that deployment changes: the host
app is an ordinary API client of Bambuddy over the LAN.

### Why not the stock SpoolBuddy protocol

SpoolBuddy is a thin daemon that registers as a "device", heartbeats, and
posts tag scans; the kiosk web page then reacts to WebSocket broadcasts. That
protocol exists to drive the kiosk screen and the attached load cell. This
project needs neither: Bambuddy's plain inventory API already offers tag
lookup, weight update, spool creation and tag linking. So the host app uses
the inventory API directly and never registers as a SpoolBuddy device.

### Inventory mode

Bambuddy can keep its inventory locally (internal mode) or in Spoolman. The
owner's deployment has no Spoolman container and this design targets
**internal mode only**. At the deployed version (1.2.5.2) the
`update-spool-weight` call routes to Spoolman when that mode is on, but
`by-tag`, spool creation and `link-tag` are internal‑only; Spoolman mode has
its own route set under `/spoolman/…` (`PATCH /spools/{id}/weight`,
`PATCH /spools/{id}/tag`). Supporting it would mean a second Bambuddy client,
and it is out of scope. The host refuses to run against a Bambuddy with
`spoolman_enabled` set and says why.

## Goals

- Scan a Bambu Lab spool (MIFARE Classic, keys derived from the UID) or a
  third‑party NTAG spool and identify it in Bambuddy with no duplicates.
- Update the remaining weight of an existing spool from a typed value,
  entered as net (filament only) or gross (spool included).
- Register a new spool or refill from a scan with one action, **New**. A
  new item is always full, so no weight is typed: label weight comes from
  the tag, usage is zero, and the empty‑spool weight is the configured
  Bambu reusable spool, because any refill that is no longer full is by
  then sitting on one.
- Link a scan to a spool that already exists in Bambuddy without a tag
  (added by hand, imported, never seen by an AMS), instead of creating a
  duplicate.
- Do all of the above from the Flipper alone, with the host acting as a
  bridge; and also from the host, which shows a session feed of scans
  where any weight can be entered or corrected.
- Work on macOS over USB and on iPadOS and iOS over Bluetooth LE.

## Non‑goals

- A Bluetooth scale, a load cell, or any new hardware.
- Writing tags (OpenTag3D / OpenSpool). Read only.
- Replacing the SpoolBuddy kiosk or registering as a SpoolBuddy device.
- Changing Bambuddy or the Synology deployment.
- Spoolman inventory mode (see "Inventory mode").
- A persistent history on the host. Bambuddy keeps the weighing history.
- An offline queue on the Flipper in the first version. The design keeps
  room for one (see "Intents"), but it is not built now.

## Anti‑goals (what would make this a failure)

- Creating duplicate spools because a tag was matched by the wrong
  identifier. A Bambu spool carries two tags with different UIDs and one
  tray UUID; matching must go through the tray UUID first.
- Writing a weight to the wrong spool, or interpreting net as gross. A
  partly used refill is always mounted on a reusable spool, so its gross
  weight includes that spool; the design never treats a refill as bare.
- Flipper firmware forks. The Flipper app must build with the stock SDK
  (`ufbt`) against the official firmware.

## Constraints and facts established

### Bambuddy API (verified in maziggy/bambuddy `main`, 2026-10-25)

All calls carry the header `X-API-Key: bb_…`. The key is created in
Bambuddy under Settings → API Keys and needs inventory read and update
permissions. Base path is `/api/v1`.

| Purpose | Call | Notes |
|---|---|---|
| Find spool by tag | `GET /inventory/spools/by-tag?tray_uuid=…&tag_uid=…` | Tries `tray_uuid` first, then `tag_uid`; 404 when none. Documented as the dedupe call for NFC integrations. |
| Update weight | `POST /spoolbuddy/scale/update-spool-weight` body `{spool_id, weight_grams}` | `weight_grams` is **gross**. Bambuddy subtracts `core_weight`, sets `weight_used`, `last_scale_weight`, `last_weighed_at`. In Spoolman mode it writes `remaining_weight` to Spoolman instead. Permission `inventory:update`. |
| Create spool | `POST /inventory/spools` | Body is `SpoolCreate`; relevant fields: `material`, `subtype`, `color_name`, `rgba`, `brand`, `label_weight` (default 1000), `core_weight` (default 250), `core_weight_catalog_id`, `weight_used`, `tag_uid`, `tray_uuid`, `tag_type`, `data_origin`, `note`. |
| Link tag | `PATCH /inventory/spools/{id}/link-tag` body `{tag_uid, tray_uuid, tag_type, data_origin}` | 409 `tag_already_linked` names the other spool. `tray_uuid` must be exactly 32 hex chars. |
| Read spool | `GET /inventory/spools/{id}` | Response includes `label_weight`, `core_weight`, `weight_used`, `last_scale_weight`, `rgba`, `color_name`, `material`, `subtype`. |
| List spools | `GET /inventory/spools` | For the "link to existing" picker; filter client‑side by material, colour name, brand, note. |
| Empty‑spool catalog | `GET /inventory/catalog` | Entries `{id, name, weight}`; defaults include "Bambu Lab - Plastic Low Temp" 250 g and "Bambu Lab - Plastic High Temp" 216 g. |
| Colour map | `GET /inventory/colors/map` | Colour names for new items; see "Phase 0 findings". Optional. |
| Settings | `GET /settings` (or equivalent) | Read `spoolman_enabled` once at connect to refuse Spoolman mode. Builder confirms the exact route. |

All of these exist at the deployed version 1.2.5.2 (checked by the builder
review against that tag).

### Bambu tag reading

- Tags are MIFARE Classic 1K. Sector keys (Key A) are derived per tag with
  HKDF‑SHA256: salt = Bambu master key (16 bytes, present in the SpoolBuddy
  daemon `spoolbuddy/daemon/pn5180.py`), IKM = 4‑byte UID, info =
  `"RFID-A\0"`, 96 bytes of output = 16 keys × 6 bytes.
- The SpoolBuddy daemon reads blocks 1, 2, 4, 5 and takes the tray UUID from
  blocks 4+5 as ASCII hex. The public Bambu RFID tag guide
  (Bambu‑Research‑Group/RFID‑Tag‑Guide) documents the tray UID in block 9
  (16 raw bytes) and block 4 as the detailed filament type string. These
  disagree. **The value that matters is the one the AMS reports over MQTT**
  (`tray_uuid`, 32 upper‑case hex chars), because that is what Bambuddy
  stores for spools it has seen in an AMS and what `by-tag` matches first.
  Phase 0 settles which block it is by comparing a dump with the
  `tray_uuid` Bambuddy already holds for the same spool. The hypothesis is
  block 9. If the daemon turns out wrong, that is a bug report upstream, not
  a reason to copy it.
- Until then the tag parsers (Swift `TagParser`, Python `bambu_tag`) emit
  **tray UUID candidates in lookup order**: block 9 as raw hex, then
  blocks 4+5 the way the daemon reads them (the first 32 hex characters of
  their ASCII text, else block 4 as raw hex). All-zero values are skipped
  and duplicates collapsed. The host tries `by-tag` with each candidate in
  turn, then with the UID alone, and a new item stores the first candidate
  as its `tray_uuid`. Phase 0 drops the losing candidate from both parsers.
- Other block fields used for prefill (per the tag guide, to be confirmed
  on real dumps in Phase 0): block 1 = material variant id + material id
  (ASCII), block 2 = filament type (ASCII, e.g. `PLA`), block 4 = detailed
  type (ASCII, e.g. `PLA Basic`), block 5 = colour RGBA (4 bytes) + spool
  label weight (uint16 LE, grams) + filament diameter (float32), block 6 =
  drying and nozzle temperatures, block 16 = colour count and second colour.
- NTAG213/215/216: no keys, pages read directly. Bambuddy matches these by
  UID only. The NDEF payload (OpenTag3D or OpenSpool JSON) is optional
  prefill data for a new spool.

### Flipper Zero SDK (verified against `targets/f7/api_symbols.csv`, dev branch)

Exported and usable from an external app (FAP):
`mf_classic_poller_sync_detect_type`, `mf_classic_poller_sync_auth`,
`mf_classic_poller_sync_read_block`, `mf_ultralight_poller_sync_read_page`,
`nfc_scanner_*`, `nfc_poller_start`, `furi_hal_usb_set_config` with
`usb_cdc_single`, `furi_hal_cdc_send`/`furi_hal_cdc_receive`/
`furi_hal_cdc_set_callbacks`, `bt_profile_start` with `ble_profile_serial`,
`ble_profile_serial_tx`, `ble_profile_serial_set_event_callback`,
`bt_profile_restore_default`, GUI modules `number_input`, `submenu`,
`dialog_ex`, `widget`, `text_input`.

Not exported: `mbedtls_sha256_*`, `mbedtls_md_hmac_*`. The FAP ships its own
SHA‑256 + HMAC + HKDF (small, public‑domain implementation, with a host‑side
unit test against known vectors and against the Python derivation in the
SpoolBuddy daemon).

### Apple platforms

- macOS: the Flipper enumerates as a USB CDC serial device
  (`/dev/cu.usbmodemflip_…`). Plain POSIX termios is enough; no third‑party
  serial library.
- iPadOS, iOS and watchOS: third‑party apps have no USB serial access.
  Bluetooth LE via CoreBluetooth is the only transport. The Flipper's stock
  serial profile demands passkey pairing and authenticated characteristics,
  which watchOS cannot complete (no passkey prompt on the watch), so the FAP
  publishes its own GATT service without pairing (see Phase 2).
- While the FAP owns the BLE profile, the official Flipper mobile app
  cannot connect. That is acceptable.

### Repository conventions

- Repository content (code, comments, docs, commit messages) in English.
  Communication with the owner in Russian. (Same rule as `bambuddy-deploy`;
  put it in the repo's `AGENTS.md`.)
- No Flipper firmware forks; stock `ufbt` build.

## Architecture

Three parts in one repository:

```
flipper-flibuddy/
  AGENTS.md
  README.md
  docs/superpowers/specs/      design docs
  protocol/                    PROTOCOL.md, JSON schema, fixtures (tag dumps,
                               sample messages) shared by both sides' tests
  flipper/                     the FAP: application.fam, src/, host-compiled
                               unit tests (hkdf, intent model, line framing)
  host/                        Swift package + Xcode project: the bridge app
                               for macOS, iPadOS, iOS
  tools/                       Python dev bridge (Phase 0 spike, kept as a
                               debugging tool), dump inspection scripts
  .github/workflows/           flipper.yml (ufbt build + unit tests, Linux),
                               host.yml (xcodebuild test, macOS runner)
```

Why one repository: the two halves are useless apart and share one
protocol; changing a message touches both sides in one commit, and the
protocol fixtures are the contract both test suites run against. Why hard
boundaries: each half has its own toolchain, CI job (path‑filtered) and
test suite, so a Swift change never triggers a `ufbt` build and vice versa.

### Flow

```
Flipper                      Host (Mac / iPad / iPhone)              Bambuddy
  |-- scan {tag dump} ------>|                                          |
  |                          |-- GET by-tag ---------------------------->|
  |<-- lookup {found|unknown, display strings, defaults} --------------|
  |  user: weigh / new / link on host (on Flipper)                      |
  |-- intent {id, action, tag, grams?, mode?} -->|                      |
  |                          |-- POST update-spool-weight / spools ---->|
  |<-- result {id, ok|error, display strings} ---|                      |
  |                          |  or: user edits weight in the feed and    |
  |                          |  the host sends the same call itself      |
```

### Flipper app (`flipper/`)

Screens:

1. **Idle.** "Hold a spool to the back." Status line shows transport state:
   `USB`, `BLE`, or `no host`.
2. **Scanning / reading.** Progress while sectors are read.
3. **Result.** Two variants, filled from the host's `lookup` reply:
   - *Found:* material and subtype, colour name, remaining grams, empty‑spool
     grams. Submenu: **Enter weight**, **Close**.
   - *Unknown, Bambu tag with prefill:* what the host parsed from the tag
     (material, colour, label weight). Submenu: **New**, **Link on host**,
     **Close**. **New** creates the item as full; no weight is asked.
   - *Unknown, no prefill* (NTAG, or a Bambu tag with unreadable sectors):
     UID and tag type. Submenu: **Create on host**, **Link on host**,
     **Close**. The Flipper never creates a spool with empty material and
     colour; that needs the host's form. "Link on host" and "Create on
     host" send an intent that only flags the feed row on the host, so the
     owner finds it waiting there.
   - *No host:* UID, tag type and locally visible facts (tray UUID, raw
     material strings if a Bambu tag). No actions.
4. **Enter weight.** `number_input`, pre‑filled with the spool's last gross
   weight when known, otherwise the label weight plus empty‑spool weight. A
   submenu item toggles **net / gross**; the choice persists across the
   session. Confirming sends a `weigh` intent.
5. **Done / error.** "Saved. 484 g left." or the host's error string. Back
   returns to Idle.

Internals:

- **NFC reader.** Detect ISO14443‑3A; classify by SAK (`0x08`/`0x18` MIFARE
  Classic, `0x00` NTAG). For Classic: derive keys, authenticate each sector
  with Key A, read data blocks 0–63 except sector trailers, tolerate
  per‑sector failures (send what was read, flag missing sectors). For NTAG:
  read pages 0 up to the end of the tag's user memory, as reported by the
  tag type. The Flipper does not interpret the content beyond what the "no
  host" screen shows.
- **Intents.** Every user action becomes an intent record: client‑generated
  id, action, the full tag dump it refers to, grams and net/gross mode when
  relevant, timestamp. Intents are the unit the host acknowledges by id.
  The transport layer and the intent store are separate modules; the first
  version's store is "the one in‑memory intent currently in flight". A
  future offline queue replaces that store with an SD‑backed list that is
  replayed on connect; the protocol and the host need no change for that,
  because the host already treats intents as idempotent by id.
- **Transport.** One line‑oriented byte stream abstraction with two
  backends: USB CDC (`usb_cdc_single`) and BLE serial (`ble_profile_serial`).
  Which backend is active is a setting on the Flipper (USB by default when
  USB is connected, BLE otherwise; the builder decides the exact switching
  rule). Messages are newline‑terminated JSON, see Protocol.

### Host app (`host/`)

One SwiftUI code base targeting macOS, iPadOS and iOS.

- **Session feed.** Newest first. Each row: colour swatch, material and
  subtype, colour name, remaining per Bambuddy, the entered weight, status
  (`scanned`, `awaiting weight`, `saved`, `error`). Weight is editable in
  the row with the net/gross toggle; saving from the host sends the same
  Bambuddy call the Flipper intent would. Unknown rows have **New** and
  **Link to existing** buttons. **New** opens a small prefilled form before
  creating (for NTAG the form starts empty except the UID). **Link to
  existing** opens a picker over `GET /inventory/spools`
  with a text filter and calls `link-tag`; a 409 shows which spool already
  carries the tag and offers to move it. A **Clear** action empties the
  feed; it is in‑memory only.
- **Flipper and host are peers.** Whichever side enters a weight first sends
  it; a later edit on the other side sends another update; last write wins.
  Each row shows which side saved last.
- **Settings.** Bambuddy base URL, API key (Keychain), the core weight
  used for new items as an editable number (default 325 g, see "Phase 0
  findings"), the catalog entry recorded as `core_weight_catalog_id`
  (picking one copies its weight into the number; editing the number
  keeps the entry), net/gross default, transport (USB device or BLE
  peripheral). A "Test connection" button calls `GET /inventory/catalog`.
- **Tag parser.** Turns a dump into: tag type, UID, tray UUID (Bambu,
  block 9), material, subtype (detailed type without the material word,
  as the AMS stores it), colour RGBA, label weight, temperatures (Bambu
  block layout); or UID plus optional NDEF prefill (NTAG). Pure
  functions, fixture‑tested.
- **Bambuddy client.** The six calls in the table above, with typed
  responses and error mapping (404 → unknown, 409 → tag already linked with
  the other spool named, network errors → feed row error).
- **Rules for new items.** `label_weight` from the tag (fallback 1000),
  `core_weight` and `core_weight_catalog_id` from the settings,
  `weight_used` 0, `tag_uid` (AMS form, see "Phase 0 findings"),
  `tray_uuid`, `tag_type`, `data_origin: "flipper"`, material and colour
  from the tag; `color_name` from Bambuddy's colour map when the RGBA is
  known there, otherwise left empty. Spools and
  refills are created the same way: a refill is weighed only once it is on
  a reusable spool, so the spool's weight is the right tare for both. Net
  to gross: `gross = net + core_weight` of the spool in question.
- **Idempotency.** The host remembers intent ids it has completed during
  the session and answers a repeat with the stored result instead of
  calling Bambuddy again. This is what the future Flipper queue relies on.

### Protocol (`protocol/`)

Newline‑delimited JSON, UTF‑8, one object per line, same on USB and BLE.
Every message has `t` (type) and `v` (protocol version, `1`). The first
line each side sends after connecting is `hello`.

| From | `t` | Fields |
|---|---|---|
| both | `hello` | `v`, `app`, `fw` or `os` |
| Flipper | `scan` | `scan_id`, `tag`: `{type: "mfc"|"ntag", uid (hex), sak, blocks: {"0": hex16, …}` or `pages: {"0": hex4, …}`, `missing: [sector numbers]`} |
| Host | `lookup` | `scan_id`, `status: "found"|"unknown"|"error"`, `spool_id?`, `lines: [up to 4 display strings]`, `defaults: {gross?, label?, core?}`, `error?` |
| Flipper | `intent` | `id`, `scan_id`, `action: "weigh"|"new"|"create_on_host"|"link_on_host"`, `grams?`, `mode?: "net"|"gross"`, `tag` (repeated, so an intent is self‑contained for a future queue) |
| Host | `result` | `id`, `ok: bool`, `lines: [display strings]`, `error?` |

The host owns all wording; the Flipper renders `lines` verbatim. A JSON
Schema in `protocol/schema/` and sample messages in `protocol/fixtures/`
are the contract both test suites check. `protocol/PROTOCOL.md` is the
authoritative description as implemented (it adds `atqa` to the tag, the
`prefill` flag on `lookup`, the handshake timing and the line limit). Tag dumps captured in Phase 0 (with
UIDs intact; they are not secrets) live in `protocol/fixtures/tags/`.

Line length: a full Bambu dump is 48 data blocks × 32 hex = about 1.6 KB
plus framing; fine for USB CDC and for BLE with the serial profile's
chunking. The builder picks a sane maximum (8 KB) and the Flipper rejects
longer lines.

## Phases

### Phase 0: spike (throwaway, Mac + USB)

Minimal FAP: scan, derive keys, dump, print the `scan` line over USB. Python
bridge in `tools/`: read the line, parse, call `by-tag`, print the result,
optionally post a weight. Questions this answers:

1. Do stock Bambu spools read with the derived keys on the Flipper?
2. Which block holds the tray UUID that matches what Bambuddy already has
   (compare with a spool that an AMS has reported)?
3. Does a sealed Bambu refill read through its box, and where must the
   Flipper be held?
4. Does the `update-spool-weight` call behave as read from the source on the
   owner's Bambuddy version (1.2.5.2 deployed) and inventory mode?

Findings go into `protocol/fixtures/` and back into this document. The
Python bridge stays as a debugging tool.

### Phase 1: Flipper app + macOS host over USB

Everything in "Flipper app" and "Host app" above, USB transport only, Mac
host. Unit tests: HKDF vectors (C, host‑compiled), intent model, line
framing; Swift tag parser and Bambuddy client against fixtures and a stub
server. One end‑to‑end check with the real Bambuddy on the NAS.

### Phase 2: BLE transport, iPad and iPhone

BLE serial backend on the Flipper, CoreBluetooth client on the host,
pairing flow, the same feed UI on touch devices. Transport selection
settings on both sides. Distribution: during development the iPad build is
run from Xcode with free provisioning (expires weekly); the owner opens a
paid Apple Developer account once the app is ready, and TestFlight takes
over then. Minimum OS versions: the current major release minus one, set
by the builder at project creation.

As built, this paragraph is superseded by the facts and decisions below:
the FAP runs its own GATT service with no pairing instead of the BLE
serial profile, an Apple Watch app joins the iPhone and iPad, and the
minimum OS versions are macOS 14 and iOS, iPadOS and watchOS 26.

Facts verified on 2026-10-26 against firmware 1.5.1, Apple's developer
forums and the deployed Bambuddy; they bind Phase 2:

- **Why not the stock serial profile.** `ble_profile_serial` pairs with
  `GapPairingPinCodeShow` (the Flipper shows a six‑digit passkey, the
  central types it) and every characteristic requires authentication.
  macOS and iOS show the passkey prompt; watchOS does not, so a watch can
  never finish pairing. The project therefore publishes its own profile.
- **FliBuddy BLE profile (FAP‑defined).** A `FuriHalBleProfileTemplate`
  whose `get_gap_config` sets `bonding_mode = false`,
  `pairing_method = GapPairingNone`, the Flipper's BLE MAC
  (`furi_hal_version_get_ble_mac`), the GAP device name and the
  scan-response manufacturer data (`GapConfig.adv_name`, `mfg_data`, from
  `furi_hal_version_get_name_ptr`, see `src/ble_wire.h`) and advertises
  the service UUID; `start` adds the service with
  `ble_gatt_service_add` / `ble_gatt_characteristic_init`, `stop` deletes
  it. Started with `bt_profile_start(bt, &flibuddy_ble_profile, NULL)`
  (restarts the second core), ended with `bt_profile_restore_default`.
  `bt_set_status_changed_callback` reports `Advertising`; `Connected`
  never comes without pairing (see the hardware findings below).
  Events reach the FAP through
  `ble_event_dispatcher_register_svc_handler`: attribute‑modified events
  for RX writes and for the TX client‑configuration descriptor (the host
  subscribing), and the ATT MTU exchange response for the chunk size.
- **GATT.** Service `51C07E84-E37E-4D52-AC6D-B2003116F506`.
  RX (host → Flipper) `D250A7B1-D301-43F6-928E-BF2CA5D115BD`: write with response and write without
  response, variable length up to 243 bytes, `ATTR_PERMISSION_NONE`.
  TX (Flipper → host) `C3AB2366-401D-482C-9002-B0A046C2A4E4`: notify, variable length up to 243 bytes,
  `ATTR_PERMISSION_NONE`. Chunks never exceed ATT MTU − 3 (20 bytes until
  the MTU exchange says more). Flow control: the host sends one write at
  a time and waits for the response; the Flipper sends one notification
  at a time; a stack still busy after the SDK's own wait ends the line. Lines stay ≤ 8192
  bytes as in PROTOCOL.md. No encryption on the link: it carries tag dumps
  and grams; the API key never crosses it.
- **Transport is a setting on both sides.** Flipper: USB or Bluetooth,
  chosen from the Idle screen and persisted under
  `/ext/apps_data/flibuddy/`; the app starts the chosen backend only.
  Host: USB (macOS only) or Bluetooth (macOS, iOS, watchOS), plus the
  remembered peripheral; no pairing step anywhere.
- **One SwiftUI code base.** Views live in a platform‑neutral
  `FliBuddyUI` target used by the macOS app and the iOS app; macOS‑only
  pieces (serial port picker) are conditional. The watch has its own
  compact views over the same `HostModel`.
- **Apple Watch app.** A watchOS target in the same project, companion of
  the iOS app. The iPhone app pushes the Bambuddy URL, API key, core weight
  and catalog entry to the watch over WatchConnectivity whenever they are
  saved; the watch keeps them in its own Keychain and defaults and talks to
  Bambuddy itself (Wi‑Fi or through the phone). The watch connects to the
  Flipper over BLE like the phone does (CoreBluetooth on watchOS: central
  only, two connections, 30 ms minimum interval, no state restoration,
  `bluetooth-central` background mode). Screens: the feed, a row with the
  weight entered by the Digital Crown and the net/gross toggle, **New**.
- **iOS and watchOS projects.** No hand‑maintained `.xcodeproj`: an
  XcodeGen `project.yml` is committed, the project is generated locally and
  in CI (`brew install xcodegen`). Signing comes from an untracked xcconfig
  with the owner's team id; CI builds for the simulators with signing
  disabled.
- **API key by QR.** Bambuddy's QR encodes
  `bambuddy://config?v=1&url=<base URL>&key=<API key>` (percent‑encoded).
  iOS Settings scan it with the camera and fill both fields; typing stays
  as the fallback. Unknown versions or schemes are rejected with a message.

Findings on the owner's Flipper (firmware 1.5.1, 2026-10-26). The first
Bluetooth build never connected; a test build with the three fixes below
did (`hello` both ways, link connected), and they supersede the profile
bullet above where it differs:

- **The advertisement did not fit.** The stack adds the flags (3 bytes)
  and the TX power level (3) to the 128-bit service UUID AD (18) and the
  name AD (up to 10): 34 of 31 bytes. The log showed
  `[E][BleGap] set_discoverable failed 146` (`BLE_STATUS_INVALID_PARAMS`)
  and nothing was advertised. Fix: the advertisement carries no name (24
  bytes); the name travels in the scan response as manufacturer data
  (company id `0xFFFF`, reserved for tests, then the name) and stays the
  GAP device name. The host falls back to the manufacturer data when the
  peripheral and the advertisement give no name; the scan filters on the
  service UUID, so the name is display only.
- **"Connected" never came.** In 1.5.1 `gap.c` raises
  `GapEventTypeConnected` only on `ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE`. A
  FliBuddy host never pairs, so the Bt service never reports
  `BtStatusConnected` and the Flipper took the host's `hello` without
  answering. Fix: a host that has subscribed to TX is a connected host;
  a disconnect clears the subscription.
- **The subscription event.** TX's `gatt_evt_mask` must be
  `GATT_NOTIFY_ATTRIBUTE_WRITE` (it was `GATT_DONT_NOTIFY_EVENTS`) for the
  host's CCCD write to reach the service as an attribute‑modified event.
- **"BLE adv" is not proof of advertising.** `gap_advertise_start` raises
  `GapEventTypeStartAdvertising` even when `aci_gap_set_discoverable`
  fails, so the Idle screen says "BLE adv" either way. The Flipper log
  tells: with the app on Bluetooth the USB port stays with the CLI; run
  `log debug` there (`ufbt cli`) and watch the `BleGap`, `FliBuddyBle` and
  `FliBuddy` lines.

### Later (not designed here)

- Offline queue on the Flipper (SD‑backed intent store, replay on connect).
- NDEF prefill from OpenTag3D / OpenSpool tags.

## Phase 0 findings (2026-10-26)

Checked with the owner's Flipper (firmware 1.5.1), real Bambu spools and
the Bambuddy on the NAS (1.2.5.x, internal inventory, 62 AMS‑added
spools). Everything below is settled and supersedes earlier wording.

- **Tray UUID is block 9**, raw hex. The AMS‑reported `tray_uuid` of a
  known spool matched block 9 byte for byte. Blocks 4+5 hold the detailed
  type as ASCII ("PLA Pure"), so the SpoolBuddy daemon's reading of those
  blocks yields a name, not a UUID. The parsers keep a single tray UUID;
  the candidate list goes. Filing an upstream issue on maziggy/bambuddy is
  the owner's call.
- **AMS tag UID form.** Bambuddy stores the AMS‑reported `tag_uid` as the
  4‑byte MIFARE UID followed by `00000100` (`ECA8FEDE00000100` for UID
  `ECA8FEDE`). Bambuddy normalises to the last 16 hex characters and does
  not pad. The host sends Bambu tag UIDs in that form for lookup, create
  and link so records look the same whichever side made them. NTAG UIDs
  are sent as read.
- **Weights.** Measured: empty reusable Bambu spool 293 g, refill core
  32 g. Bambuddy has one knob, `core_weight` per spool, meaning everything
  on the scale that is not filament; `net = gross − core_weight`. For
  Bambu PLA/PETG that is **325 g** for a sealed spool and for a refill on a
  reusable spool alike, so **New** always uses it; refills in their box are
  not modelled. Default core weight in the host and the bridge is 325 g,
  editable. The owner set the three "Bambu Lab" catalog entries in
  Bambuddy to 325 g (they were 1000 g), because AMS‑added spools take
  their core weight from the first "Bambu Lab" entry, and all existing
  spools were updated to 325 g.
- **Colour names.** `GET /inventory/colors/map` returns
  `{colors: {hex6: name}, by_material: {"<material>|<hex6>": name}}`,
  keys lower‑case. New items look up `by_material` with the detailed type,
  then `colors`; when neither matches `color_name` stays empty rather than
  carrying the hex string.
- **Subtype.** AMS‑added spools carry the detailed type without the
  material word ("Basic", not "PLA Basic"). New items do the same.
- **Reading.** Both tags of a spool resolve to the same tray UUID; a sealed
  refill reads through its box; a tag moved away early yields a dump with
  all sectors missing, which leaves the host only the UID (an unknown tag
  without tray UUID or prefill). Leaving the app with Back on the Idle
  screen returns the USB port to the Flipper CLI.
- **Why weighing matters.** Bambuddy's AMS remain‑percent fallback charged
  a full label (100 %) to one spool when it was swapped during a print;
  the scale weight overrode it. The project's premise holds.

## Decisions made by the designer (owner may override)

- No SpoolBuddy device registration; inventory API only.
- Flipper is a thin client: raw dumps out, display strings in. Parsing and
  all Bambuddy calls live on the host.
- Weight entry exists on both sides; last write wins.
- Feed is in‑memory per session on the host.
- Repository layout, CI split, and the dev bridge in Python.
- First version has no offline queue, but intents are self‑contained and
  idempotent by id so one can be added without a protocol change.
- Swift/SwiftUI for the host, POSIX serial on macOS, CoreBluetooth on iOS.
- Transport is a per‑device setting rather than auto‑negotiated.
- Internal inventory mode only; the host refuses Spoolman mode.
- "Link to existing" lives on the host; the Flipper only flags the row.
- The Flipper offers **New** only when the tag gave prefill data;
  otherwise it defers to the host's form.
- Settings on iPad and iPhone can take the API key from the QR code
  Bambuddy shows when a key is created (Phase 2).
- Flipper firmware: the latest stable release at the time of building, and
  `ufbt` pinned to that release channel in CI. The owner updates the
  Flipper if it is older; no support for older API versions.

Phase 2, decided by the builder (2026-10-26; the plan is
`docs/plans/20261026-phase2-ble-ios.md`):

- Minimum OS versions: macOS 14 stays; iOS, iPadOS and watchOS 26.
- No pairing and no encryption on the BLE link by default. One macro,
  `FLIBUDDY_BLE_REQUIRE_ENCRYPTION` (default 0), makes the
  characteristics require encryption so "just works" pairing can be tried
  on real devices; if the Mac, the iPhone and the watch all pair without a
  prompt, it may become the default.
- The profile advertises the 128‑bit service UUID and no name (flags 3 +
  TX power 3 + UUID 18 = 24 of 31 bytes; a name AD does not fit and made
  the stack refuse the advertisement). The name (8 chars at most,
  "Flipper" when none) travels in the scan response as manufacturer data,
  company id 0xFFFF, and is the GAP device name. The Flipper's MAC is
  changed in one byte (byte 2 XOR 0x5B, as the HID profile does) so a
  central never mixes it up with the default profile's bonds and cached
  GATT table.
- Flow control: the host sends one write with response at a time, chunks
  of `maximumWriteValueLength` up to 243 bytes; the Flipper sends one
  notification at a time in chunks of ATT MTU − 3; a stack still busy
  after the SDK's own wait (about a second) ends the line, and so does a
  line that took longer than 2 s in all (the SDK waits for every chunk),
  so a slow link holds the Flipper's GUI thread for about 3 s at most. The Flipper's `hello` goes out when the host subscribes
  to TX, as on USB DTR.
- The Flipper's transport menu opens with OK on the Idle screen; the
  status line reads `USB`, `BLE adv`, `BLE` or `no host` and sits under
  the title. A BLE backend that cannot start falls back to one that never
  connects; the app keeps running.
- Host transport settings: USB is the default on the Mac, Bluetooth
  elsewhere (USB is not offered there). The remembered peripheral is
  reconnected to directly when the system still knows it, otherwise
  scanned for; with none chosen the host takes the first Flipper found. Scan and connect each time out after 10 s; the settings
  scan runs 15 s. The Mac executable embeds an Info.plist with the
  Bluetooth usage description and no bundle id, so the Phase 1 settings
  stay where they are.
- Apple projects: one XcodeGen `project.yml` in `host/Apple` with the
  iPhone/iPad app and the watch app embedded as its companion; generated
  Info.plists; signing from a git‑ignored `Signing.xcconfig` included by
  the tracked `Base.xcconfig`; placeholder bundle id
  `com.sokolachko.flibuddy`. iPad gets a sidebar and one-line table when
  the window leaves 900 pt beside the sidebar (`FeedLayout`), else the
  iPhone's cards in a tab view.
- Watch sync: the application context (`v: 1`) carries the Bambuddy URL,
  API key, core weight, catalog entry and default mode, sent 0.5 s after
  the last change applied on the iPhone and once at launch; the watch keeps
  its own transport and Flipper. The watch offers New with the tag's
  prefill; Link to existing stays on the phone and the Mac. Crown steps:
  1 g per detent, 5 g from about 6 detents/s.
- QR scanning uses VisionKit's `DataScannerViewController`; the button
  only shows where the scanner is available. Repeated query fields take
  the first value; the URL keeps any path prefix and drops credentials,
  query and fragment.
- CI builds both Apple apps for the simulators on `macos-latest` without
  signing; the device builds and BLE on hardware are checked by hand.

## Answered by the owner

- New items are always full: a sealed spool or a sealed refill. One action,
  **New**, no weight entry. A refill that is not full is always on a
  reusable spool, so one tare serves both.
- Inventory is internal; there is no Spoolman. Only Bambuddy's inventory
  API is used.
- No paid Apple Developer account yet; one will be opened when the app is
  ready. Development runs from Xcode until then.

## Left to the builder

- Exact Flipper screen layouts and key bindings.
- SHA‑256/HMAC implementation choice for the FAP and its licence.
- BLE chunking and flow control details, pairing UX.
- JSON encoding/decoding library on the Flipper (hand‑rolled writer plus a
  minimal reader is acceptable; `lines` arrays are short).
- Stub server shape for host tests.
- Whether the "no host" screen shows parsed Bambu strings or only hex.
