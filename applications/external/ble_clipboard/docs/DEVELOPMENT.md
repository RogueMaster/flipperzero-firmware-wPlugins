# Development and Release

Version 0.5.4 includes matching Mac, Windows x64 beta and Flipper apps. There is no vendor submodule or
third-party typing backend. Keep the running app and connected device untouched
unless installation or live testing is separately requested.

## Source

| Path | Responsibility |
| --- | --- |
| `BLEClipboardMac/Sources` | Mac UI, library, compiler, binary codec and BLE client |
| `BluetoothClipboard/clipboard_app.c` | Flipper SDK, BLE/USB, GUI and RAM/SD adapters |
| `BluetoothClipboard/clipboard_hid.h` | Button-up movement and keyboard/mouse cleanup adapter |
| `BLEClipboardMac/Sources/BLEClipboardMac/Companion*` | Info UI, pinned release metadata and availability checks |
| `BluetoothClipboard/transfer_protocol.*` | Independent framed upload/job state machine |
| `BluetoothClipboard/keyboard_*` | Layout allowlist, on-device menu and durable keyboard preferences |
| `BLEClipboardMac/Sources/BLEClipboardMac/KeyboardSettings*` | Shared Mac defaults, settings codec and keyboard panel |
| `BluetoothClipboard/macro_engine.*` | Typed-program validation and nonblocking execution |
| `scripts/tests` | Independent fake-USB, lifecycle, sanitizer and fixture checks |
| `Store/` | Same Mac features with App Sandbox and separate Store preparation gates |

## Build and Check

macOS 14+, Swift 5.9+/Xcode and uFBT 0.2.6 configured for official firmware 1.5.1 /
API 88.2 are required. No submodule initialization is necessary.

```sh
swift test --package-path BLEClipboardMac
sh scripts/check_clipboard.sh
UFBT=ufbt sh scripts/build_flipper.sh
BLEClipboardMac/build_app.sh --universal
```

The host checks run the actual C engine/server with AddressSanitizer and UBSan.
The Swift tests emit complete wire fixtures; C consumes them and captures fake
HID reports, including a 64 KiB job, literals, Turkish letters and structured
macros. On macOS, Carbon checks the captured Turkish map against installed
keyboard translations. No USB device is opened, no input source is changed and
no events are injected. These checks are not hardware certification.

The build outputs are `BLEClipboardMac/dist/BLE Clipboard.app` and
`BluetoothClipboard/dist/ble_clipboard.fap`. The Flipper build also creates
corresponding source and source fingerprints. It never calls `ufbt launch`.
Installation requires normal USB serial mode after the owner exits the old app.

For read-only Info inspection or qFlipper screenshots, launch the FAP with the
`--info` argument through the firmware loader. This opens the same Info/QR views
and returns before any Bluetooth, USB HID or saved-library setup. OK opens the QR;
Left/Back returns, then exits. It cannot receive, save or execute a job. Normal
launch is unchanged. Host-rendered screen previews are also available with
`scripts/render_catalog_screens.py` and official firmware 1.5.1 source; those
previews go to ignored `dist/catalog-preview`, never over genuine screenshots.

## Package 0.5.4

Use Python 3.11+ for release tooling. Windows packages must be built on Windows
x64 with pinned Python 3.12.10 and `BLEClipboardWindows/requirements-build.txt`.
The Windows CI job performs offline UI checks at 100/150/200% scaling, checks
PE architecture/version, builds both distributions, and tests clean installation,
packaged execution without Python on PATH, reinstall and library-preserving
uninstall. See [Windows build instructions](../BLEClipboardWindows/README.md).
Download its artifact into `BLEClipboardWindows/dist` without changing the
`SOURCE-COMMIT.txt` receipt. It must match the reviewed HEAD, not merely VERSION.

```sh
scripts/package_release.sh 0.5.4
```

The version must match `VERSION`; the script rejects rebuilding older versions.
It checks the built FAP's source fingerprints, builds a universal Mac app, uses
`ditto` for the ZIP, includes both corresponding source archives, README, NOTICE,
LICENSE, Windows installer/portable ZIP and runtime notices, fingerprints and
one authoritative checksum file under `dist/v0.5.4`. Source archives snapshot
the allowlisted current working tree, including local development changes,
while excluding build products, private keys and local account configuration.

The default signature is ad-hoc. It verifies integrity, not Developer ID identity
or notarization. No packaging command publishes, submits to Apple, installs the
apps or changes the user's running processes/device.

For an authorized signed release, configure the existing Developer ID identity
locally and run `scripts/notarize_release.sh 0.5.4` deliberately from the clean
reviewed commit. Its resumable driver records candidate hashes/source, preserves
the pre-staple ZIP and signed bundle, then creates a durable submission guard.
The **same command on retry only polls/finalizes that submission**, never rebuilds
or resubmits. Exit 3 means In Progress. A missing receipt after an attempted
submission requires manual ID recovery, not another submission. Accepted builds
are stapled, deep/strict verified, checked for team/version/build/architectures
and Gatekeeper's Notarized Developer ID, then repacked without re-signing.
Final hashes are frozen; an interrupted final archive/checksum promotion is
resumed without regenerating the archive. Never export signing keys or put credentials in
CI, shell arguments, source or chat. The old 0.1.1 submission/artifacts are not
part of the 0.5.4 release and must remain unchanged. Public 0.5.2/0.5.3 assets are also
immutable; a patch release uses a new tag and matching asset set.

## Companion Metadata

After changing VERSION, run `swift scripts/generate_companion_assets.swift`.
This uses Core Image to generate the version headers, Swift build version and
the Flipper QR bitmap, then decodes the QR to verify the exact release URL.
The PNG/C array include a four-module quiet zone. Generated headers are included
in the FAP source archive, so a Flipper-only build does not require a QR library
or Swift at runtime. Local checks and Mac packaging reject stale generated files.

The Info panel's public GitHub request is explicit (panel open/refresh), ephemeral,
unauthenticated and version-pinned. Store builds need network-client entitlement
for this check; no other runtime HTTP service is introduced. Preview identities
ending in `.InfoPreview`, like `--ui-testing`, use a temporary library, no BLE and
an offline unpublished-release result. This does not certify the sandboxed target.

## Publish

Review the hardware/release checklist and disclose remaining validation gaps.
Commit the actual tested source, push and tag that revision. The tag workflow
creates an ad-hoc draft, not an automatic public release. A draft can be prepared
while notarization processes, but must stay unpublished until Apple's submission
is Accepted and the final ZIP passes stapler validation, deep/strict codesign,
team/version/architecture checks and Gatekeeper's Notarized Developer ID assessment.

After the tag workflow succeeds, replace every draft asset as one matching set:
Mac ZIP, Windows installer/portable ZIP, Windows runtime notices, FAP,
SHA256SUMS.txt, README, NOTICE, LICENSE, both source archives and FAP source
fingerprints. `scripts/release_assets.py` defines the eleven payloads; the
checksum file is the twelfth asset. In particular, never retain a workflow FAP beside checksums
for a different local FAP. Refresh documentation/source assets without rebuilding
the accepted Mac bundle. Verify the draft's downloaded set before publishing and
the public downloaded set afterward. Never overwrite an already public version.

Generate the cask with `scripts/generate_cask.py` from the final ZIP only after
the public URL/checksum match. Do not update the installed app or tap as part of
a development build. Store signing, licensing review, metadata, beta testing and
App Review remain a separate gated path in `Store/README.md`.
