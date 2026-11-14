# Hardware Validation

## 0.5.4 - Matching Desktops and Keyboard Settings

On 2026-11-09, local checks passed all 53 Swift tests, 22 Python compiler/library/
fake-BLE tests, AddressSanitizer/UBSan C checks, identical shared Swift/Python
compiler golden bytes and complete wire fixtures executed by the C engine.
New timing checks cover bounds, capability rejection before upload, cancellation
during pauses, USB/BLE loss and timer rollover. Existing suites cover slot limits,
SD rollback/recovery, keyboard revisions and unchanged snapshot metadata.
The Mac universal and API 88.2 FAP builds pass without device access.

Windows UI checks run on a genuine Windows x64 CI runner with synthetic libraries,
no BLE worker, no clipboard read and no input output. Retained screenshots cover
editor, review, error and minimum-size states at 100/150/200% Tk scaling. These
are application renders, not physical monitor/Bluetooth certification. On the
runner's 1024x768 display, larger windows use overlapping, unedited viewport
captures with pixel-offset receipts; they are not composited or presented as a
single full-screen capture. Installer
and frozen-app validation are separate CI gates; only a successful final run
certifies the corresponding source/artifact set. A previous shipped Windows
binary does not exist, so reinstall plus legacy-library migration are the upgrade
proxies, not a claimed physical upgrade from an older Windows release.

Local checks on 2026-11-06 passed: all 50 Swift tests, AddressSanitizer/UBSan C
suites, Swift-to-C fixtures and generated version/QR validation. The Flipper FAP
build passes target 7 / API 88.2 checks and its source fingerprint verification.
The direct Mac build contains arm64/x86_64 and passes deep/strict ad-hoc signature
verification. The unsigned sandbox archive validates; neither build is a new
notarized public release or a Store-uploadable signed archive.

Host checks cover all 28 on-device choices, explicit Save/Back navigation,
persistent settings and restart recovery, failed SD writes/sync/renames, corrupt
records, and unchanged saved snapshot headers. C protocol tests verify no HID or
job-storage activity on settings sync, no unsolicited response to older clients,
revision conflicts, missing maps, disconnect retention and busy-run rejection
without changing keys, mouse reports, cursor or deadline. Swift tests cover the
optional handshake, settings codec, stale/malformed replies, conflicts, timeouts,
Mac defaults persistence and no BLE access for local changes. Swift-generated
settings request/reply fixtures are checked byte-for-byte by the actual C server.

No live BLE/USB test, input automation, app installation, firmware change or
pairing-key modification is part of these checks. Physical keyboard menu use,
disconnected local runs, persistence across a real restart and Mac reconnect
sync still require separate owner-approved validation. The existing 0.5.3 live
capture below does not certify 0.5.4 or resolve its earlier input/focus interference.

## 0.5.3 - Live Text and Delay Run

On 2026-11-05, with explicit owner approval, the installed 0.5.3 Mac and Flipper
pair completed a real Bluetooth LE to USB run into a dedicated plain text
receiver on the source Mac. Info & Companions reported matching 0.5.3 versions
and API 88.2 / target 7. The target's existing Apple Turkish-QWERTY-PC input
source was matched with macOS / Turkish Q; no input-source settings were changed.

The temporary README Demo macro had three steps: a 10,000 ms delay, one literal
text step and a 25,000 ms delay. Its exact output, including a final newline, was:

```text
usb macro demo

source mac
target usb text capture

text, delays and repeat steps.

demo complete.
```

The receiver recorded all 99 ASCII bytes, byte-for-byte equal to the macro text,
with 99 new text changes. The local capture guard did not detect focus loss;
the receipt at the end of typing reports active application, key window and text
focus. Payload SHA-256:
`366c3689ecfe6ae15cf278fe3885828709ee1acfbce3af6b5a8e99a3c1582a08`.
The production Mac Turkish-Q table maps every character in this payload to a
key with zero modifier bits. This is a deliberately limited text subset.

The running-state screenshot was taken during the final delay, after all text
had arrived and with no further HID input queued. The real sender subsequently
reported USB output complete; total observed run time was about 39.1 seconds.
The output screenshot shows the actual receiver text, not pasted or generated
content. See [screenshot provenance](ARTWORK.md#live-run-and-usb-output).

The temporary macro was deleted through the app UI, the original seven macros
remained, and the scratch receiver was closed. The installed Mac app was left
connected and idle. No existing macro was run or edited, no snapshot was saved,
and no installed binary, firmware or pairing key was changed.

This pass covers only live literal text, delays and completion for this payload.
It does not validate the Repeat step mentioned in the sample text, native Turkish
letters, all ASCII, other layouts, shortcuts, mouse behavior, physical SD
save/restart/run or multi-hour stability.

### Target Shortcut Compatibility

Two earlier capture attempts were interrupted and are not passing tests. The
first lost target focus after 20 correct bytes around a capture operation; its
cause was not established. The second lost focus without a screenshot or sender
inspection during typing, and left partial/corrupted output. Both were stopped
by the local capture guard before the successful attempt, and the receiver was
cleared.

The earlier payload used `>` followed shortly by `i`. The production
`keyboard_turkish_mac.h` maps those characters to Right Option combinations
(`0x401e` and `0x400c`). Claude Desktop documents
[double-tap Option as its default quick-entry shortcut](https://support.claude.com/en/articles/12626668-use-quick-entry-with-claude-desktop-on-mac),
which can open from another app. This is a plausible conflict consistent with
the owner's earlier report of Claude opening, not a proven sole cause: the
foreground app identity and the owner's shortcut setting were not recorded.
No Claude settings or system shortcuts were changed. On affected targets,
choose a non-conflicting global shortcut before using Option-based characters.
The successful modifier-free subset does not resolve or certify this case.

## Current 0.5.3 - SD Path Fix

On 2026-11-04, read-only USB CLI checks found about 59 GiB free and no saved
snapshot directory. The existing app-data directory resolves without a trailing
slash; the same path with a trailing slash reports invalid name/path. No files,
pairing keys, snapshots or firmware were changed by those diagnostics.

The host storage stub now models this firmware restriction. The saved-library
test fails on the original root path and passes after using the unsuffixed
application-data prefix. Both the snapshot and large-transfer initialization
sites are fixed. All 44 Mac tests, sanitizer-enabled C suites, Swift-to-C fixtures,
keyboard translation checks and the target-7/API-88.2 FAP build pass.
Save failures are now identified as saves before resetting the Mac job state.

These checks are not a physical save/run, keyboard/mouse test or multi-hour soak.
Installation/publication receipts must be checked separately; earlier 0.5.2
screenshots and notarization do not certify a different 0.5.3 binary.

Genuine qFlipper 1.3.3 Info/Downloads exports were refreshed on 2026-11-04 from
a temporary 0.5.3 FAP launched only with --info. They are copied byte-for-byte,
512x256 with exactly the required orange/black palette. The exported QR decodes
to the exact v0.5.3 release URL. Info, OK to QR and Back/exit were checked without
Bluetooth/HID/library setup. The loader was idle after capture and the installed
0.5.2 FAP still matched its pre-capture checksum. No normal app run was performed
during that Info capture.

## Previous 0.5.2 - Companion Info and Installation

All 42 Swift tests pass, including exact-version asset validation, draft/missing/
incomplete/offline release states, anonymous pinned requests through a URLSession
stub, optional version queries and unchanged transfer state. The C sanitizer
checks query metadata during an autonomous run and verify unchanged cursor,
deadline, key/mouse output counts and release count. Old clients receive no
unsolicited new frame, and older servers are not sent an unsupported query.
The 0.5.1 HID adapter regression checks still pass. The FAP builds for target 7 /
API 88.2 without launching or communicating with a physical device.

An isolated `.InfoPreview` app was inspected with a temporary macro library,
Bluetooth disabled and a fixed offline unpublished-release result. Both toolbar
and Help open the Info panel; version 0.5.2, companion labels and pinned resources
fit without overlap. Mac/FAP download buttons and version-specific resource links
are disabled, including their accessibility actions, while the release is not
published. At that preview checkpoint the installed 0.5.1 app was not replaced,
quit or connected.

After explicit owner readiness on 2026-11-03, both 0.5.2 apps were installed with
0.5.1 backups locally and a hidden non-FAP backup on Flipper SD. Staged and final
full FAP readbacks match SHA-256
`643e7cddcb44154f8ff2e9d5896aee2063eb3eee9924dd259452d3a12d917b2a`.
The loader was idle before/after installation; the Flipper app was not launched.
SDK metadata matches API 88.2/target 7; the package checksum identifies this exact
patch, since the SDK metadata stores only major/minor 0.5.

The installed Mac bundle matches the reviewed universal ZIP and passes signature,
arm64/x86_64 and version checks. Its real Info panel reports 0.5.2 and its anonymous
matching-release check returns Not published yet, with both downloads disabled.
It remains disconnected, with no job sent or executed. The Mac macro library is
byte-for-byte unchanged; firmware, bonds and saved SD jobs were not modified.

The generated 41x41 QR bitmap has a four-module quiet zone; Core Image decoded it
to the exact v0.5.2 release URL. This is an offline bitmap check, not a physical
camera/display scan. On-device Info navigation, physical QR scanning, live version
reporting and sandboxed network checks remain release gates. No live input was
generated, no existing snapshots were changed, and no public release was created
at that installation checkpoint.

## 0.5.2 Catalog Capture

With explicit owner approval on 2026-11-03, a temporary FAP was uploaded to an
app-owned SD subdirectory and fully read back before launch with --info. That
entry path returns before all Bluetooth, USB HID and saved-library setup. It
opens the same shared Info/QR renderer as the normal app. Info, OK to QR, Back
to Info and Back to exit were inspected through qFlipper 1.3.3. Save Screenshot
exports are committed unchanged at 512x256 with exactly the required orange/black
palette. Core Image decodes the exported QR to the exact v0.5.2 release URL.
This is not a camera scan or a normal Bluetooth/HID session.

The temporary FAP/directory was removed afterward. The loader was idle and the
original installed 0.5.2 FAP still matched the checksum above. Pairing keys, saved
snapshots and the Mac app/library were not changed. The final release candidate
adds only the read-only Info path/shared renderer and compact QR button since
that installed FAP. Its automated GUI-only test links no Bluetooth/HID/storage
symbols, exercises Info/QR/Back/exit and verifies balanced resource cleanup.

## Previous 0.5.1 - Mouse Button Fix and Installation

The owner reported drag behavior in 0.5.0. Firmware 1.5.1's mouse-move function
retains button state, while this app previously only cleared keyboard state.
The new shared HID adapter clears all eight button bits before every move and
refuses movement if the button-up report fails. Startup, Stop, completion/failure
and exit also clear mouse state. This is a concrete stale-state defect consistent
with the report, not a live capture proving the cause on the owner's target.
See the [firmware 1.5.1 HID implementation](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c).

Sanitizer-enabled adapter tests model the firmware's persistent state, reproduce
the old held-button move and verify neutral button-up reports before exact X/Y
movement, all button masks, failure gating and disconnected cleanup. They send
no real USB input. All 37 Swift tests and the C sanitizer-enabled
keyboard/layout/engine/storage/adapter checks pass locally. The FAP builds for
target 7/API 88.2. These checks do not certify physical target behavior.
Physical drag-lock/another mouse's held button and target-side behavior are not
controlled by this fix; no live 0.5.1 input test has been run.

After explicit owner readiness on 2026-11-03, the matching 0.5.1 pair was installed
with 0.5.0 app backups locally and a hidden non-FAP backup on Flipper SD. The new
FAP was staged and read back before replacement, then fully read back again;
SHA-256 `38fc1547a15db284be9b73d5eed54807a2b1cac359e74bdaa8011b0d1edc8c78`
matches the reviewed package. Target 7/API 88.2 metadata checks pass; the SDK
stores only major/minor 0.5, so the checksum identifies this patch build. The
loader was idle before/after installation, and the Flipper app was not launched.

The Mac bundle matches the reviewed ZIP and passes deep/strict ad-hoc signature
and arm64/x86_64 checks. Its installed About panel shows Version 0.5.1 (1).
The Mac library is byte-for-byte unchanged; saved SD jobs, firmware and bonds
were not modified. The Mac was reopened from /Applications, disconnected, with
the existing eight-hour ranged preset selected. No job was sent or run. Existing
saved jobs gain this movement guard from the FAP update without needing a resend.

## Previous 0.5.0 - Installation and Local Checks

After explicit owner readiness on 2026-11-03, both 0.5.0 apps were installed with
the previous 0.4.0 apps backed up locally and the Flipper FAP also backed up on SD.
The installed Mac bundle matches the reviewed ZIP, has both arm64/x86_64 slices,
and passes deep/strict ad-hoc signature verification (not notarization). The
Flipper FAP was staged, fully read back, renamed into place and fully read back
again; its SHA-256 matches the package. The loader was idle before and after;
the Flipper app was not launched. Firmware, bonds and saved SD jobs were untouched.

The Mac was opened from /Applications and left disconnected. Its existing Mouse
Jiggle preset was updated through MacroLibrary with a JSON backup: ID, target,
layout and 480-minute duration preserved; Repeat interval 45...95 seconds and
Move Mouse X/Y -2...2 HID units. The installed editor and summary show these
values. No job was sent or executed. An older saved Flipper snapshot must be
replaced explicitly by resending the preset.

Move Mouse/Repeat tests cover bounded range sampling, variable intervals, excluded
zero vectors, entropy failure, generic body validation and byte lengths, old saved
binary compatibility, schema migration, monotonic progress, and deadline key release.
Eight-hour clock simulations, timer rollover, offline Stop and USB-loss/no-restart
pass with fake USB. The matching sources build for the official 1.5.1/API 88.2 SDK.
No hardware input was generated by those installation/automated checks. The owner
subsequently connected to Rapigak and reported dragging during use. Physical focus,
drift, idle-policy behavior and long-duration stability remain unverified;
variability is not undetectability.

## Previous 0.4.0 Installation and Local Checks

The current Mac and Flipper apps use an independent v4 transport and typed
binary execution engine. None of the older device passes below certify this
update. The 0.3.0 pair was previously installed with file verification and its
28-layout Mac menu inspected, not a typing test. After explicit owner readiness,
the 0.4.0 pair was installed with backups: Mac universal signature/version/bundle
comparison and full Flipper file read-back/SHA-256/manifest checks passed. Flipper
was idle before/after file installation; its 0.4.0 app was not launched or executed.
The Mac was opened from /Applications and a stopped eight-hour Mouse Jiggle
definition was saved in its library. No live HID or pairing changes were made.

Local checks cover fragmented frames, complete CRC-verified uploads, explicit
execution, all 28 layout IDs and three target IDs, native Turkish UTF-8,
literal command-looking text, 64 KiB programs, RAM/large-job paths, Stop during
upload/execution, queue expiry, USB/BLE loss, storage/TX failures and malformed
programs. The host tests compile the actual C engine/server with sanitizers and
feed wire fixtures emitted by the Swift compiler/session. No USB device is opened.

Standalone tests simulate eight hours, millisecond rollover, offline physical Stop,
reconnect/remote Stop and USB failure with no restart. Filesystem stubs compile
the actual SD library and exercise persistence, bounded 64 KiB copy, slot limits,
corrupt files, failed writes/sync/renames and interrupted replacement/recovery.
These checks do not establish physical multi-hour stability or OS idle behavior.

An isolated 0.4.0 direct-build preview (`--ui-testing`, separate app identity and
temporary library, Bluetooth disabled) was inspected: mouse preset creation,
direct numeric duration entry and persistence, first-click Send opening Save mode,
plaintext snapshot acknowledgement, and normal/mouse standalone defaults. Run and
Save stayed disabled while offline. This does not close the sandboxed Store UI or
physical hardware gates.

The engine's captured reports for 95 printable ASCII characters and 12 native
Turkish letters match Apple's Turkish-QWERTY-PC translation for three keyboard
types. Those translation checks neither inject events nor change input sources.
Physical focus/shortcut behavior, other layouts/OSes, actual SD/USB/BLE timing,
first pairing and interruptions remain release gates requiring separate approval.

## Historical 0.1.x / 0.2.0 Device Evidence

The following is preserved history, not the current backend or current device
state. The owner stopped the last live test; it was not a successful full-layout
pass. Old app paths and results below must not be used as current instructions.

### Original Live Hardware Record - 2026-11-01

This records observed results only. It is not a completed release certification.

## Setup

- Flipper Zero, official firmware 1.5.1, API 88.2 (read over USB CLI).
- Mac input source: Turkish QWERTY PC; match the Flipper output layout to it.
- Installed pinned Ducker at `/ext/apps/GPIO/bt_paste_ducker.fap`, leaving the
  existing BLE Clipboard app untouched. No firmware update or bond reset.
- Installed FAP read back over USB and SHA-256 matched the build:
  `1fbdb05cc1d118e96beeff857e55a6a0ebe354a49ff4b979fea0da44285a999e`.
- All 28 layout assets were present on the Flipper SD card; the Mac catalog now
  offers them. This does not claim physical typing verification of every layout.
- Capture helper instrumentation self-check passed; helper cleared afterward.

## Results

| Check | Observed result |
| --- | --- |
| Flipper USB CLI | Passed after user exited the legacy app with Back |
| Separate Ducker install/read-back | Passed |
| Launch | USB switched to HID after loader request; not proof of BLE handshake |
| Native Mac discovery | Test Flipper discovered |
| Native serial setup | First attempt reached GATT write and failed; diagnosis pending |
| Diagnostic rebuild | CoreBluetooth authorization request pending macOS approval; system prompt requires user action |
| Handshake `READY` | Not verified yet |
| USB text/macro output | Not tested yet |
| Cancel, reconnect, cable loss, sleep | Not tested yet |
| 28-layout serialization and acknowledgement unit tests | Passed |

No test payload has been executed by the new Mac app at this checkpoint.
Do not mark physical delivery, first pairing or interruption cases complete.

The Mac app now surfaces the GATT error domain/code and allows cancelling a
connection attempt. This is diagnostic work, not a bypass of authentication.
The dedicated capture app supports ordinary Edit commands and has no global
keyboard hook. No production document was used as the USB target.

## Follow-up - 2026-11-02

- User approved macOS Bluetooth access and pairing on both devices.
- Native Mac app established the BLE link, but did not complete TTF1 setup.
- An independent Bleak probe connected, enumerated the expected RX/TX
  characteristics and enabled TX indications. No text frame or execute command
  was sent by that probe.
- User restarted Ducker Bluetooth with Hold Down, preserving bonds.
- After that restart, the handshake write was acknowledged but no `READY`
  response arrived before the probe timeout.
- Read-only diagnostics reported RX buffer capacity 1,024, consistent with the
  firmware RPC receiver rather than Ducker's configured buffer. The RPC status
  characteristic's first byte was active. This suggests a profile ownership
  conflict; it does not by itself establish the only failure cause.
- A small isolated-profile overlay was built for API 88.2. Host sanitizer tests
  verify distinct outer profile identity, original GAP configuration forwarding,
  failure cleanup and 100 repeated start/stop cycles.
- Compatibility FAP SHA-256:
  `7ac4f8449e47fe21c3fcdd767db09f6520df9179fab2eb587d69a4dc581aeac1`.
- Compatibility installation and hardware verification are pending the user
  exiting Ducker so USB serial becomes available. No device firmware or pairing
  key storage was changed by this overlay build.

The old source/app and initial pinned Ducker FAP remain available. No physical
typing pass or completed release gate is claimed at this checkpoint.

## Compatibility installation and handshake

- The owner exited Ducker, restoring USB serial access.
- Installed the compatibility app separately at
  `/ext/apps/GPIO/bt_paste_ducker_compat.fap`, displayed as **BT Paste Ducker**.
  The initial transfer was interrupted after overlapping CLI clients; both
  clients were stopped, the CLI recovered, and the transfer was redone in one
  exclusive serial session. No incomplete file was launched.
- Full USB read-back matched the local build's MD5 and SHA-256. SHA-256 remains
  `7ac4f8449e47fe21c3fcdd767db09f6520df9179fab2eb587d69a4dc581aeac1`.
- The independent BLE probe reported RPC inactive, receive buffer 486 bytes,
  `READY:1:65536` and `LAYOUT:QWERTY US`. It sent no typing payload or execute
  command. This verifies the profile-isolation fix for the observed handshake
  failure on this device/firmware.
- The native Mac preview then completed its own handshake and displayed
  **Ducker connected**. Its live picker showed all 28 layouts; Turkish Q was
  selected to match the test Mac.
- A 17-byte harmless sample was reviewed, but before the controlled run was
  started the preview's draft changed and it reported completion for different
  content. The dedicated capture file still contained zero input changes.
  No unexpected content was copied into this report.
- Controlled typing tests were paused pending exclusive control of the preview
  and capture windows. UI completion alone is not proof of target receipt.

**Passed:** install/read-back, independent TTF1 handshake, native TTF1 handshake.
**Not yet verified:** controlled USB delivery, literal brackets/whitespace,
macro parameters, cancellation, large transfers and interruption recovery.

## Controlled USB smoke test

- The owner granted exclusive use of the preview and dedicated capture windows.
- Reviewed and explicitly ran the 28-byte literal `BT Paste controlled test 123`
  using Turkish Q, with the preview hidden before output.
- The native capture recorded exactly those 28 bytes and 28 text changes. This
  verifies target delivery and focus restoration for this sample, not every
  layout or macro type.
- The Mac app subsequently timed out waiting for completion and disconnected.
  Therefore the complete run lifecycle did not pass, despite correct delivery.
- A follow-up build accepts completion after verified execution was requested
  even when the intermediate `SENDING` status is missing. The compatibility
  overlay also retries only rejected status indications, at most 32 attempts
  with 10 ms gaps; it never retries a received job or HID action.
- All 28 Swift tests and host sanitizer checks pass, including the missing
  intermediate-status and bounded status-retry cases. The updated firmware
  compiles for API 88.2. Installation and a repeated live run are pending.

## Status-reliability build installation

- The owner exited Ducker. The previous compatibility FAP was backed up and
  checked against the device checksum before replacement.
- Installed status-retry FAP SHA-256:
  `312d48253ee572be543cf447608d56a958c6b1c8691c86e166b64c0a23b45aa4`.
  Full USB read-back matched the build and device checksum.
- The native preview reconnected and completed TTF1 setup without a bond reset.
- An explicitly reviewed 28-byte sample produced native control messages
  `RECV`, `SENDING`, then `OK`. The preview displayed **USB output complete**
  and remained connected instead of reaching the prior watchdog timeout.
- The target capture remained empty on this attempt. Additional instrumentation
  showed the helper's editor selected while its application and window were
  inactive. Therefore this attempt verifies completion reporting, not target
  delivery; no complete end-to-end pass is inferred.
- The non-shipping helper now records its own active/key/focus state and has
  explicit activation in its Focus buttons. Automated activation still did not
  establish a verified active target. Further output is paused until the owner
  manually focuses the dedicated capture editor.

## Controlled delivery after manual focus

The owner focused the capture editor. Its persisted application-active,
key-window and editor-focus indicators were true before subsequent text runs.
All samples below were explicitly reviewed and executed, using Turkish Q on
the same official 1.5.1/API 88.2 device. Only the dedicated capture app received
test input; no system clipboard contents were used as payloads.

| Check | Observed result |
| --- | --- |
| 28-byte literal smoke sample | Exact match, 28 input changes; `RECV`, `SENDING`, `OK`; remained connected |
| Literal command-looking brackets | `[ENTER]`, `[WIN+r]`, `[]` and `{{clipboard}}` appeared as text, not commands/templates |
| Tab and LF in plain text | Present in the native capture |
| Full printable ASCII | Failed exact comparison; Turkish-Q punctuation/dead-key mismatch described below |
| JSON import through native file picker | Benign two-macro fixture imported into the preview library |
| Parameterized field navigation | First field exactly `Ada`, second exactly `Lovelace`; no text in the large editor |
| Stop during 30-second delay | Captured `before`; Stop disconnected; after the original delay elapsed, still six changes and no `AFTER` |
| Bonded reconnect after Stop | Native handshake succeeded; capture unchanged, no replayed output |
| 4,160-byte literal / SD-backed send | Exactly 4,160 bytes and 4,160 input changes; `OK` received; remained connected |

The large sample was `0123456789abcdef` repeated 260 times. Its receive
acknowledgement arrived at 12:27:21 UTC and completion at 12:28:57 UTC. A UI status
read timed out during continuous typing; no focus-changing recovery was attempted.
The final persisted native capture matched exactly and remained active/key/focused.

Local, ignored capture artifacts are under `Store/dist/hardware`, including
`status-retry-smoke-capture.json`, `ascii-layout-capture.json`,
`two-fields-capture.json`, `cancel-delay-capture.json`, `reconnect-capture.json`
and `large-transfer-capture.json`. They contain only the benign test samples.

### Open layout finding

The stock `tr-TR.kl` asset did not produce all printable ASCII correctly with
this Mac's Turkish QWERTY PC input source. Double quote, less-than, vertical bar
and grave accent differed; the grave accent combined with the following letter
as a dead key. Correct plain letters and brackets do not certify the whole
layout. Resolve or explicitly limit this combination before release, and test
other targets/layouts separately. No system input-source setting was changed.

At that checkpoint the preview was connected and idle with a short benign draft.
It was later terminated at the owner's request, as recorded below.
USB/cable loss, sleep/power-cycle, first-pairing rejection, other OSes,
other layouts, near-maximum payloads and other cancellation stages remain open.

## Turkish-Q correction - initial local checkpoint

The stock mapping's mismatches were reproduced using Apple's installed
`com.apple.keylayout.Turkish-QWERTY-PC` translation. The Mac now selects a distinct
`tr-TR-mac.kl` built-in compatibility profile only for macOS/Turkish Q. This keeps
the Windows/Linux stock asset and the other 27 layout choices unchanged.
The new mapping corrects quote, less-than and pipe, avoids pending caret/tilde
accents, and completes a grave with Space before processing the next character.

- All 30 Swift tests pass, including target-specific layout acknowledgement and
  preservation of literal text and saved layout identifiers.
- The patched production sender passes in-memory and SD-path checks for all 95
  printable characters, cancellation before/during a grave, failed presses,
  missing-layout failure and switching back to US/SD layouts.
- Its captured HID reports match Apple's translation for all 95 characters,
  each followed by `a`, for keyboard types 40, 41 and this Mac's type 92.
- The updated FAP compiles for official firmware 1.5.1/API 88.2. SHA-256:
  `43d79f730586617fb0b2dc1612c52d378744e2ba11c0fb9a9798c4b96fa9587a`.
- The unsigned universal Mac archive validates. This is not Store signing or a
  physical USB pass.

At this initial checkpoint the updated FAP had not yet been installed. The
full-ASCII capture and focused dead-key regressions must be repeated with the
updated Mac preview and FAP before closing the layout release gate. No test input
has been sent during this correction's local checks.

## Corrected FAP installation and owner stop

The owner later authorized installing the corrected `43d79f...` FAP above. The
previous FAP was backed up and the full USB read-back matched the new local file.
The updated preview completed the Bluetooth handshake. A reviewed full-ASCII
attempt produced incomplete target text and the capture's final application/
window focus indicators were false. The root cause of app switching/shortcuts
was not established. The owner stopped the prepared repeat; the preview process
was terminated, and no corrected live pass is claimed. Existing apps, pairing
keys and the device firmware were preserved.

## Unified 0.2.0 and native Turkish letters - local only

The standard Mac build now exposes Quick Paste and the existing macro library
together, using the same 28-layout catalog. The retained basic v2 implementation
also accepts all 28 layout IDs and experimental Turkish UTF-8, gated against
older Flipper builds. The default Ducker path instead compiles native letters to
`[TR:XXXX]` tags and requires `CAPS:TURKISH` from the updated compatibility FAP.

Host tests cover all 12 native Turkish letters, malformed input, old-peer
rejection, escaping, capacity expansion, in-memory/SD typing, layout reset and
interruption. Captured reports match Apple's Turkish-QWERTY-PC translation for
all 95 printable ASCII and 12 Turkish letters on three keyboard types. These
checks do not inject events or change the selected input source. No new FAP is
installed and no live keystrokes are sent as part of this work. The failed/
interrupted live test and all other physical release gates remain open.
