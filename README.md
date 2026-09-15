# Flipper USB Internet Bridge

Flipper USB Internet Bridge lets a Flipper Zero make explicitly authorized,
restricted HTTPS requests through a desktop computer's existing internet
connection. The Flipper does not join Wi-Fi. A desktop host acts as a
user-space application proxy.

```text
Flipper Zero FAP
      ↕  FIBP v1 binary frames
USB CDC ACM — second CDC channel only
      ↕
desktop host
(native macOS app or Windows/Linux/macOS CLI)
      ↕  isolated URLSession or direct validated TLS
HTTPS internet destination
```

This is not a USB Ethernet adapter. CDC-ECM, RNDIS, NCM, lwIP, operating-system Internet
Sharing, system Wi-Fi changes, and Wi-Fi password transfer are intentionally
outside the MVP.

## Features

- Standard Flipper SDK/uFBT `.fap` application
- Native Swift and SwiftUI macOS 13+ menu bar helper
- Installable Windows/Linux/macOS command-line host
- Source-level client SDK that other Flipper FAPs can compile into their apps
- Versioned binary protocol with explicit frame boundaries and two CRC32 checks
- Device- and protocol-bound one-time or persistent permission
- HTTPS GET with response headers, streamed body chunks, timeout, and cancellation
- Optional POST and request-header support in the protocol/helper test path
- DNS and redirect SSRF protection
- Ephemeral URLSession without shared cookies, cache, or credentials
- Hardware-free PTY simulator
- Portable C/Python tests and Swift unit/integration tests

## Flipper menu

- **Test Connection** — sends a PING and waits for PONG
- **Get Sample Text** — fetches and displays a short message
- **Get Date and Time** — retrieves a short UTC response
- **Search Wikipedia** — displays a short English Wikipedia result
- **Weather** — searches Open-Meteo locations and shows the current forecast
- **National Today** — displays the daily National Today description
- **Where is the ISS?** — shows current coordinates, altitude, speed, and visibility
- **Internet Radio** — finds MP3 stations by country and plays them on the Flipper speaker
- **Custom URL Request** — accepts an HTTPS URL up to 384 bytes
- **Connection Info** — shows USB, helper, permission, device, and protocol state

The sample text is displayed only; it is not saved to the microSD card. Response
bodies arrive in 192-byte protocol chunks. The Flipper retains only a bounded
1,536-byte screen preview, while the total response limit is 4 MiB.

Weather searches accept ASCII input. Open-Meteo performs case- and
diacritic-insensitive matching and returns at most three choices. The result
shows temperature, apparent temperature, humidity, wind, daily high/low, and
precipitation probability. No API key is required for the public endpoint used
by this feature.

The National Today page is too large for the Flipper. Each desktop host therefore
downloads it into a bounded temporary buffer, extracts only the
`single-date-header-content` paragraph, removes HTML, and sends the compact text
to the Flipper. Bold holiday names are rendered as bold heading lines. If the
site changes its HTML structure, the app reports a parsing error instead of
returning unrelated page content.

## Repository layout

```text
.
├── application.fam              Flipper FAP manifest
├── usb_internet_bridge.c        Flipper UI and application lifecycle
├── usb_transport.[ch]           dual-CDC channel 1 transport
├── bridge_session.[ch]          Flipper session/request state machine
├── bridge_protocol.[ch]         portable C frame codec and parser
├── config.h                     Flipper and wire limits
├── macos/                       SwiftPM helper, core, resources, and tests
├── host/                        cross-platform Python host and CLI
├── sdk/flipper/                 reusable source SDK for other FAPs
├── examples/                    minimal SDK integration example
├── protocol/                    normative protocol documentation and vectors
├── scripts/                     simulator and packaging utilities
├── tests/                       portable C/Python tests
└── docs/                        architecture, threat model, and research notes
```

Additional documentation:

- [Protocol specification](protocol/protocol-spec.md)
- [Architecture and module plan](docs/architecture.md)
- [Threat model](docs/threat-model.md)
- [Test matrix](docs/test-matrix.md)
- [Development status](docs/development-status.md)
- [Future research](docs/future-research.md)

## Security model

For the first valid HELLO, the desktop host checks available USB identity
metadata, protocol version range, bounded identity fields, Flipper Zero model,
and hardware UID. A valid HELLO is not permission to access the internet.
Before any network request, the user must choose:

- Deny
- Allow Once
- Always Allow

Persistent permission is bound to the device identity, protocol version, and
permission schema. It becomes invalid when the identity or protocol changes,
the user revokes it, or application data is reset. The UID is an association
key, not cryptographic attestation; a malicious physical USB device may imitate
it.

Every desktop host:

- accepts only `https` URLs;
- rejects embedded usernames and passwords;
- blocks `localhost`, `.local`, loopback, link-local, private, unique-local,
  CGNAT, multicast, reserved, and other non-global destinations;
- validates every DNS answer and revalidates every redirect destination;
- rejects `file:`, `ftp:`, `smb:`, and every non-HTTPS scheme;
- uses an isolated request implementation without shared cookies, cache, or credentials;
- drops authentication, cookie, host, and hop-by-hop headers;
- never disables normal TLS certificate validation;
- cancels active network work immediately when USB disconnects.

The protocol never transfers the Wi-Fi password, Mac username, Safari history,
Keychain contents, filesystem data, or a local-network device list. Diagnostics
do not record response bodies, query strings, header values, or the full UID.

## Requirements

### Flipper

- Flipper Zero with a microSD card
- Official Flipper firmware compatible with the selected SDK
- uFBT (verified locally with release 1.4.3 / API 87.1)
- Data-capable USB-C cable

### Mac

- macOS 13 or later
- Xcode 16.x and Xcode Command Line Tools for development
- Swift 5.9 or later
- No root, privileged helper, system extension, or kernel extension

The packaged `.app` and `.dmg` do not require Xcode or Swift on the destination
Mac. The Swift code uses Apple Foundation, SwiftUI, AppKit, IOKit, CryptoKit,
and POSIX APIs. The simulator uses only the Python standard library.

### Windows/Linux cross-platform host

- Windows 10/11 or a current Linux distribution
- Python 3.10 or later when installing from source
- No administrator/root access
- `pyserial`, installed automatically with the host package

## Build the Flipper FAP

From the repository directory:

```sh
cd usbtowifi_flipperapp/Flipper-USB-Internet-Bridge
../../venv/bin/ufbt update --channel release
../../venv/bin/ufbt
```

On a new machine, create an isolated uFBT environment:

```sh
cd usbtowifi_flipperapp/Flipper-USB-Internet-Bridge
python3 -m venv .ufbt-venv
./.ufbt-venv/bin/python -m pip install --upgrade ufbt
./.ufbt-venv/bin/ufbt update --channel release
./.ufbt-venv/bin/ufbt
```

The output is `dist/usb_internet_bridge.fap`. With one Flipper connected, build,
install, and launch it with:

```sh
../../venv/bin/ufbt launch
```

Switching to dual CDC causes USB re-enumeration, so `ufbt launch` may report a
late serial read error even when the FAP has reached the Flipper. The most
deterministic installation method is qFlipper's File Manager: copy the FAP to
`/ext/apps/USB/` and launch it from the device.

## Build the macOS helper

Using SwiftPM:

```sh
cd macos
swift test
swift build -c release
```

Using Xcode:

```sh
cd macos
open Package.swift
```

Select the `FlipperInternetBridge` scheme and `My Mac`, then Run. To start the
helper directly from Terminal:

```sh
cd macos
swift run FlipperInternetBridge
```

The helper appears in the menu bar. Closing the diagnostics window does not stop
the bridge. Choosing **Quit Application** deliberately stops the helper and all
Flipper internet access.

## Run the Windows/Linux host

The same CLI also runs on macOS as an alternative to the native menu bar app.
From the repository root:

```sh
python3 -m venv .host-venv
. .host-venv/bin/activate
python -m pip install .
fib-bridge
```

Windows PowerShell activation and startup:

```powershell
python -m venv .host-venv
.host-venv\Scripts\Activate.ps1
python -m pip install .
fib-bridge
```

Use `fib-bridge --list-ports` to inspect detection or `fib-bridge --port PORT`
to select a port explicitly. The first device connection asks for Deny, Allow
Once, or Always Allow in the terminal. See [host/README.md](host/README.md) for
security and packaging details.

## Use the bridge from another Flipper app

Flipper OS does not keep one FAP running as a background service while another
FAP is open. Consumer apps therefore compile the small source SDK into their own
FAP and talk directly to the desktop host:

```c
#include "sdk/flipper/fib_bridge_client.h"

FibBridgeClientConfig config = {.app_version = "1.0"};
FibBridgeClientCallbacks callbacks = {
    .on_status = on_bridge_status,
    .on_body = on_response_chunk,
    .context = app,
};
FibBridgeClient* client = fib_bridge_client_alloc(&config, &callbacks);
fib_bridge_client_start(client);
```

Call `fib_bridge_client_tick()` from the app event loop, wait for
`FibBridgeStateReady`, then use `fib_bridge_client_get()`. Responses arrive in
bounded chunks through `on_body`; the SDK never allocates a complete response.
Integration instructions and lifecycle constraints are in
[sdk/flipper/README.md](sdk/flipper/README.md).

## Build the `.app` and `.dmg`

On a development Mac with full Xcode installed:

```sh
chmod +x scripts/package_macos.sh
./scripts/package_macos.sh
```

Outputs:

```text
dist/macos/Flipper Internet Bridge.app
dist/macos/Flipper-Internet-Bridge.dmg
```

The packaging script includes the project app icon, creates an ad-hoc signature,
and places an Applications shortcut in the DMG. This MVP is not Developer ID
signed or notarized. On another Mac, the first launch may require Control-click
or right-click → **Open**. The package is built for the architecture of the Mac
that runs the script.

## Install and connect

1. Copy `dist/usb_internet_bridge.fap` to `/ext/apps/USB/` with qFlipper.
2. Install and start either the native macOS helper or the cross-platform host.
3. Open **Apps → USB → USB Internet Bridge** on the Flipper.
4. The FAP saves the current USB configuration, enables `usb_cdc_dual`, and owns
   only the second CDC channel. The first channel remains available to the CLI.
5. The helper discovers candidates through IOKit and waits for a valid binary
   HELLO before showing any permission prompt.
6. Choose **Allow Once** or **Always Allow** on the desktop host.
7. The Flipper displays **Internet access ready**.
8. Use **Test Connection** or **Get Sample Text** to verify the bridge.

When the cable is removed, the serial descriptor closes, the active network task
is cancelled, one-time permission is cleared, and the Flipper returns to its
disconnected state. On FAP exit, CDC callbacks are detached and the previous USB
configuration is restored.

## Permission management

The menu bar menu provides:

- Flipper connected / disconnected
- Internet access enabled / disabled
- Redacted device identity
- Allow this device
- Revoke this device's permission
- Cancel active request
- Show diagnostics
- Quit application

Permission records contain a hash of the device UID and protocol identity in
UserDefaults. They contain no Wi-Fi or user credentials. Revoking permission also
cancels the active request.

## Hardware-free simulator

Display simulator options:

```sh
python3 scripts/fibp_simulator.py --help
```

Start a simulated Flipper:

```sh
python3 scripts/fibp_simulator.py --role flipper
```

In another terminal, pass the printed PTY path to a DEBUG helper build:

```sh
cd macos
FIB_SERIAL_PORT=/dev/ttysXXX swift run FlipperInternetBridge
```

`FIB_SERIAL_PORT` is honored only in DEBUG builds. Release builds cannot bypass
hardware discovery. The simulator covers fragmentation, corrupt CRC, and
disconnect behavior; Swift tests cover DNS policy, HTTP behavior, permissions,
and coordinator state.

## Tests

Portable C codec:

```sh
mkdir -p .build-tests
clang -std=c11 -Wall -Wextra -Werror -pedantic -I. \
  bridge_protocol.c tests/test_bridge_protocol.c \
  -o .build-tests/test_bridge_protocol
./.build-tests/test_bridge_protocol
```

Python codec and simulator:

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
```

macOS unit and integration tests:

```sh
cd macos
swift test
```

Flipper build validation:

```sh
../../venv/bin/ufbt
```

The current suite includes 36 Python tests, the portable C protocol test, and 63
Swift tests. It covers frame encoding, fragmentation and resynchronization, CRC,
invalid lengths, handshake/version negotiation, permission decisions, request
IDs and sequences, SSRF policy, redirects, timeout, response limits,
cancellation, USB loss, and National Today extraction.

## Central limits

Normative wire values are documented in
[protocol-spec.md](protocol/protocol-spec.md). Implementations mirror them in
`config.h` and `BridgeConfiguration.swift`.

| Limit | MVP value |
| --- | ---: |
| Frame payload / complete frame | 512 B / 544 B |
| Response chunk | 192 B |
| URL | 384 B |
| Request headers | 8, 1,024 B aggregate |
| POST body | 4 KiB |
| Response body | 4 MiB |
| Redirects | 3 |
| Request timeout | 25 s default, 30 s maximum |
| Frame / logical request assembly | 1 s / 5 s |
| HELLO timeout | 5 s |
| Idle connection / PONG | 30 s / 2 s |
| Screen preview | 1,536 B |

## Troubleshooting

### The desktop host does not detect the Flipper

```sh
ls -l /dev/cu.usbmodem* 2>/dev/null
system_profiler SPUSBDataType
```

Keep the FAP open. Ports briefly disappear and return during dual-CDC
re-enumeration. Apple Silicon Macs may also request approval for a new USB
accessory.

### qFlipper and bridge ports are confused

The helper does not trust the port name alone. It validates VID/PID, interface,
HELLO, and UID. The FAP uses CDC channel 1 while the CLI remains on channel 0.

### `ufbt launch` says more than one Flipper is attached

uFBT may see multiple serial candidates created by dual CDC. Use qFlipper File
Manager to copy `dist/usb_internet_bridge.fap` to `/ext/apps/USB/`, then launch it
on the device.

### SwiftPM reports `PackageDescription` or `SwiftBridging` errors

Select the full Xcode toolchain and retry:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -runFirstLaunch
xcodebuild -version
swift --version
```

Do not delete system module maps. Remove only the project's Swift build cache if
needed, then run `swift test` again.

### The Flipper remains at `Waiting for permission`

Confirm that the native menu bar helper or `fib-bridge` CLI is running. The
prompt appears only after a valid CRC-protected HELLO and device identity have
been received.

### A request is blocked for security

The URL must begin with `https://`, contain no credentials, and resolve only to
global addresses. Localhost, `.local`, home/office LAN addresses, and redirects
from a public host to a private address are deliberately blocked.

## Known limitations

- Only one HTTP request can be active at a time.
- The shipped FAP sends GET requests. POST and custom request headers exist in
  the protocol/helper test path but have no Flipper menu UI.
- The URL limit is 384 B; request body 4 KiB; response body 4 MiB.
- The Flipper shows a bounded preview and does not save downloads to microSD.
- No WebSocket, streaming upload, arbitrary HTTP methods, or custom TLS roots.
- UID-based permission is not cryptographic device authentication.
- A theoretical DNS rebinding TOCTOU window remains between `getaddrinfo` policy
  validation and the URLSession connection; redirects are revalidated.
- `getaddrinfo` itself cannot be cancelled by URLSession. The request deadline
  prevents a late DNS result from starting network work.
- The menu bar helper is ad-hoc signed, not Developer ID signed or notarized.
- App Sandbox behavior and all dual-CDC port naming variants require continued
  hardware testing.
- Windows and Linux binaries are CI-built and logic-tested, but still need
  physical USB testing on those operating systems before a stable release.
- Third-party demo endpoints and HTML structures may change.

## Future research

CDC-ECM/NCM, custom firmware, a lightweight on-device TCP/IP stack, concurrent
requests, WebSockets, SD-card downloads, a protocol-level application identity,
graphical Windows/Linux frontends, and signed/notarized macOS distribution
remain separate research topics. See [future-research.md](docs/future-research.md).
