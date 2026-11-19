# Internet Bridge

Internet Bridge lets a Flipper Zero make explicitly authorized,
restricted HTTPS requests through a desktop computer's existing internet
connection. The Flipper does not join Wi-Fi. A desktop host acts as a
user-space application proxy.

```text
Flipper Zero FAP
      ↕  FIBP v1 binary frames
USB CDC ACM (second channel) OR Bluetooth GATT
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
- Optional compact Windows/Linux graphical helper with USB/Bluetooth tabs
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

Find **Internet Bridge** under **Apps → Tools**. Desktop helpers retain the
**Flipper Internet Bridge** name. The category is shared
by both connection modes; the application ID and FAP filename remain unchanged.
This describes version 0.5.1. Apps Catalog updates become available after the
maintainers approve and merge the version's submission.
See [0.5.0 release checks and remaining hardware limits](docs/release-validation-0.5.0.md).

Version **0.5.1** fixes portable USB discovery and noisy-port scanning. Windows
and Linux users should update their desktop helper; no USB/network driver or
administrator helper is introduced. The FAP ID and data locations stay unchanged.
An updated FAP version may require fresh internet consent; pairing is not reset.

First choose USB Internet Bridge or Bluetooth Internet Bridge. Their shared menu:

- **Test Connection** — sends a PING and waits for PONG
- **Get Sample Text** — fetches and displays a short message
- **Get Date and Time** — retrieves a short UTC response
- **Toolbox**
  - **Search Wikipedia** — displays a short English Wikipedia result
  - **Weather** — searches Open-Meteo locations and shows the current forecast
  - **National Today** — displays the daily National Today description
  - **Where is the ISS?** — shows current coordinates, altitude, speed, and visibility
  - **Internet Radio** — finds MP3 stations by country and plays them on the Flipper speaker
  - **Markets** — searches any Binance Spot coin with a USDT pair, shows gold and
    silver in USD per troy ounce, and gets Brent BZ/USDT from Binance Futures.
    Open price cards refresh silently every two seconds.
  - **Latest Earthquakes** — shows the ten newest earthquake events reported
    by USGS, with magnitude, location, UTC time, depth, and manual refresh
  - **Currency Converter** — choose source and target currency from a list,
    enter an amount, and view the converted value and reference-rate date
  - **English Dictionary** — look up one English word and read up to three
    meanings with examples where available
- **Custom URL Request** — accepts an HTTPS URL up to 384 bytes
- **Connection Info** — shows USB, helper, permission, device, and protocol state
- **Bluetooth only: Connection Requests / Pairings** — explicitly choose a
  discovered bridge computer before Pair/Connect. Known Computers can revoke a
  bridge credential without changing USB permissions or system Bluetooth bonds.

On launch, choose **USB Internet Bridge** or **Bluetooth Internet Bridge**.
Back from either mode's main menu ends that connection and returns to this
selector; Back again exits the FAP. No bridge transport starts before selection.
USB includes all nine Toolbox tools. Bluetooth includes the same text tools but
not Internet Radio. Its radio experiment remains removed.

The native Mac menu bar panel has compact **USB / Bluetooth** tabs and independent
enable switches. Changing the selected tab does not stop either connection.
USB monitoring starts automatically; Bluetooth starts only when enabled.
Each transport owns its coordinator, HTTP client, consent prompt and active
request. USB retains Allow Once / Always Allow / Deny; Bluetooth always requires
fresh Allow Once / Deny after code-based recognition. Pairing is not consent.
See [connection architecture](docs/connection-modes.md) for setup and limits.

USB radio uses host-side MP3 decoding with the same minimp3/resampler, streaming
16-bit mono 14,493 Hz PCM to the original Flipper speaker/DSP. Both the FAP and
desktop helper must be updated together. No FFmpeg is required.
Windows/Linux terminal hosts keep their existing USB and opt-in Bluetooth
commands. The optional Qt graphical helper follows the same compact two-tab
layout; see [Windows and Linux hosts](#run-the-windowslinux-host).

The sample text is displayed only; it is not saved to the microSD card. Response
bodies arrive in 192-byte protocol chunks. Ordinary requests retain only a bounded
1,536-byte screen preview and a 4 MiB total response limit. Negotiated USB radio
uses separate bounded 64 MiB audio segments, a 16 KiB speaker ring on Flipper,
and approximately ten seconds of prebuffering on the desktop. USB endpoint
backpressure prevents dropped packets; the Mac pauses the network source when
its bounded reservoir fills. Stop and physical Back silence the speaker and
cancel the request. No response is loaded in full into Flipper RAM.

Weather searches accept ASCII input. Open-Meteo performs case- and
diacritic-insensitive matching and returns at most three choices. The result
shows temperature, apparent temperature, humidity, wind, daily high/low, and
precipitation probability. No API key is required for the public endpoint used
by this feature.

The National Today page is too large for the Flipper. Each desktop host therefore
downloads it into a bounded temporary buffer, extracts only the
`single-date-header-content` paragraph, removes HTML, and sends the compact text
to the Flipper. Holiday names retain inline bold formatting and spaces around
them. The reader shows four body lines without an extra subtitle, wraps long
names using real glyph advances, and scrolls with the arrows. OK refreshes the
live page; Back returns to Toolbox. Text is converted to printable ASCII.
Responses larger than the 1,536-byte preview explicitly show **[Text shortened]**
instead of silently appearing complete; the preview buffer is not enlarged. If the
site changes its HTML structure, the app reports a parsing error instead of
returning unrelated page content.

## Interface updates (0.5.0)

- **Wikipedia:** padded ASCII text reader with a separate search heading;
  Up/Down scrolls, Left/Right moves further through the text, and OK opens a new
  search. No encyclopedia tagline or divider consumes the reading area.
- **Weather:** a location, temperature and condition page plus a second page
  for apparent temperature, humidity, wind and rain probability. The small icon
  reflects the weather condition. Arrows switch pages and OK refreshes.
- **Where is the ISS?:** one page for latitude/longitude and another for
  altitude/speed. Arrows switch pages and OK refreshes.
- **National Today:** four-line live reader with inline bold holiday names,
  corrected spacing/wrapping and an explicit shortened-text notice when needed.
- **Desktop:** the native macOS panel and optional Windows/Linux GUI use compact
  USB/Bluetooth tabs, separate connection controls, Pairings and Diagnostics.
  CLI commands remain available. GUI previews are actual Qt widget captures on
  macOS, not proof of Windows/Linux hardware validation.

See the [UI validation report](docs/test-results-2026-10-08-ui.md) and
[National Today follow-up](docs/test-results-2026-10-08-national-today.md) for
build/test evidence and the remaining hardware checks.

## New Toolbox tools (0.5.0)

Use a matching **0.5.0 or newer desktop helper** for the new tools. Both the
native macOS app and Windows/Linux host include the transformations. An older
helper produces an update message rather than dumping raw JSON onto the screen.

**Latest Earthquakes:** open the tool and browse the ten newest event cards
in the [USGS earthquake catalog](https://earthquake.usgs.gov/fdsnws/event/1/).
Use Left/Right to change events, Up/Down to read a long location and OK to
refresh. Magnitude and depth have separate padded fields; the location sits in
a full-width card with a small scrolling indicator, and time stays below it.
Times are UTC; USGS coverage is not an
exhaustive list of every local earthquake. The query filters out non-earthquake
events and orders by occurrence time, not magnitude or the last update time.

**Currency Converter:** one screen has two currency selectors and two amount
fields. Use the arrows to highlight a field, then OK to edit it. Either amount
is editable: the last edited side drives the conversion of the opposite side.
Both selectors offer USD, EUR, GBP, TRY, JPY, CHF,
CAD, AUD, CNY, INR, BRL, MXN, SEK, NOK, PLN, KRW, HKD, SGD, NZD, DKK, and ZAR.
The 21 options cover major international currencies and selected regional
currencies; this is not every world currency or a trading-volume ranking.
Amounts support up to nine
whole digits and two decimal places. Results are displayed with two decimals;
Back from an editor or selector returns to the converter; Back from the
converter returns to Toolbox. Switching currencies clears the stale rate
before fetching the new pair. Editing an amount reuses the fetched rate.
[Frankfurter v2](https://frankfurter.dev/)
provides reference exchange rates; the result shows the observation date.
These are reference values, not live trading or bank transaction quotes.

**English Dictionary:** enter one word using ASCII letters, a hyphen, or an
apostrophe (1–48 characters). The [Free Dictionary API](https://dictionaryapi.dev/)
returns definitions; the helper sends up to three meanings, with an example
where supplied. Plain word and part-of-speech headings are separate from the
definition area in a single padded reader panel. A quiet progress rail appears
only when the text needs scrolling; there is no permanent control legend. Left/Right
changes meanings (the last page contains source and
license); Up/Down scrolls the current meaning; OK opens a new word search.
A 404 becomes **Word not found**. Text is shortened and converted
to ASCII. The screen retains the entry's source link, license name and link,
and adaptation notice. Dictionary content retains its original provider/content
license (commonly Wiktionary CC BY-SA); the project's MIT code license does not
replace the content license. No audio pronunciations are downloaded.

All three tools keep the existing permission, HTTPS, SSRF, timeout, and
cancellation rules. Their source JSON is capped at 64 KiB on the desktop and
their display payload at 1,400 bytes. Back cancels active requests. Oversized or
malformed provider data is reported as an error, not displayed as partial data.

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

On a new machine, clone the source and create an isolated uFBT environment.
The GitHub repository already contains the application at its root; no nested
project directories are needed:

```sh
git clone https://github.com/mete888/flipper_internet_bridge.git
cd flipper_internet_bridge
python3 -m venv .ufbt-venv
./.ufbt-venv/bin/python -m pip install --upgrade ufbt
./.ufbt-venv/bin/ufbt update --channel release
./.ufbt-venv/bin/ufbt
```

The output is `dist/usb_internet_bridge.fap`. With one Flipper connected, build,
install, and launch it with:

```sh
./.ufbt-venv/bin/ufbt launch
```

Switching to dual CDC causes USB re-enumeration, so `ufbt launch` may report a
late serial read error even when the FAP has reached the Flipper. The most
deterministic installation method is qFlipper's File Manager: copy the FAP to
`/ext/apps/Tools/` and launch it from the device. When upgrading an older USB
installation, move or remove its old `/ext/apps/USB/usb_internet_bridge.fap`
after the Tools copy is installed; keeping both creates duplicate menu entries.

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

The compact graphical host matches the macOS panel's USB / Bluetooth layout,
with independent connections, explicit consent, bridge Pairings and Diagnostics.
The optional CLI also runs on macOS as an alternative to the native menu bar app.
From the repository root:

```sh
python3 -m venv .host-venv
. .host-venv/bin/activate
python -m pip install ".[desktop]"
fib-bridge-desktop
```

Windows PowerShell activation and startup:

```powershell
python -m venv .host-venv
.host-venv\Scripts\Activate.ps1
python -m pip install ".[desktop]"
fib-bridge-desktop
```

The GUI opens explicit code/consent dialogs; USB and Bluetooth never share
internet grants. Use `fib-bridge --list-ports` / `fib-bridge --port PORT` for the
retained CLI. See [host/README.md](host/README.md) for platform requirements,
preview images, security, packaging commands and current validation limits.

## Use the bridge from another Flipper app

Flipper OS does not keep one FAP running as a background service while another
FAP is open. Consumer apps therefore compile the small source SDK into their own
FAP and talk directly to the desktop host:

1. Export the SDK with
   `python3 scripts/vendor_bridge_sdk.py --destination /path/to/your_fap/vendor/internet_bridge`.
2. Append `vendor/internet_bridge/*.c` and
   `vendor/internet_bridge/sdk/flipper/*.c` to your manifest's sources.
3. Use the ready-made USB/Bluetooth connection screen, then send a GET:

```c
#include "vendor/internet_bridge/sdk/flipper/fib_bridge_setup.h"

FibBridgeClientConfig config = {.app_version = "1.0"};
FibBridgeClientCallbacks callbacks = {
    .on_status = on_bridge_status,
    .on_body = on_response_chunk,
    .context = app,
};
FibBridgeClient* client = fib_bridge_client_alloc(&config, &callbacks);
if(client && fib_bridge_client_connect_ui(client)) {
    fib_bridge_client_get(client, "https://api.github.com/zen", 15000);
}
```

The setup screen handles transport choice, explicit computer selection, code
display and waiting for separate desktop internet consent; Back cancels setup.
Call it from your application thread before registering your fullscreen UI.
After it returns, call `fib_bridge_client_tick()` from your app event loop and
`fib_bridge_client_free()` on shutdown. Responses arrive in
bounded chunks through `on_body`; the SDK never allocates a complete response.
Core APIs for custom pairing/revocation screens, a complete buildable example
and lifecycle constraints are in
[sdk/flipper/README.md](sdk/flipper/README.md).

## Build the `.app` and `.dmg`

On a development Mac with full Xcode installed:

```sh
chmod +x scripts/package_macos.sh
./scripts/package_macos.sh
```

Outputs:

```text
dist/macos/Build.noindex/Flipper Internet Bridge.app
dist/macos/Flipper-Internet-Bridge.dmg
```

The staging `.app` is kept outside Spotlight indexing so it does not appear as
another installed application. Install only one copy in `/Applications`.
The packaging script includes the project app icon, creates an ad-hoc signature,
and places an Applications shortcut in the DMG. This MVP is not Developer ID
signed or notarized. On another Mac, the first launch may require Control-click
or right-click → **Open**. The package is built for the architecture of the Mac
that runs the script.

## Install and connect

1. Copy `dist/usb_internet_bridge.fap` to `/ext/apps/Tools/` with qFlipper.
2. Install and start either the native macOS helper or the cross-platform host.
3. Open **Apps → Tools → Internet Bridge** on the Flipper, then choose
   **USB Internet Bridge**. For wireless setup choose **Bluetooth Internet Bridge**
   and follow [the Bluetooth connection steps](docs/connection-modes.md).
4. In USB mode the FAP saves the current USB configuration, enables `usb_cdc_dual`,
   and owns only the second CDC channel. The first remains available to the CLI.
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

clang -std=c11 -Wall -Wextra -Werror -pedantic -I. \
  markets.c tests/test_markets.c \
  -o .build-tests/test_markets
./.build-tests/test_markets

clang -std=c11 -Wall -Wextra -Werror -pedantic -I. \
  toolbox_tools.c tests/test_toolbox_tools.c \
  -o .build-tests/test_toolbox_tools
./.build-tests/test_toolbox_tools

clang -std=c11 -Wall -Wextra -Werror -pedantic -I. \
  toolbox_tools.c toolbox_cards.c tests/test_toolbox_cards.c \
  -o .build-tests/test_toolbox_cards
./.build-tests/test_toolbox_cards
clang -std=c11 -Wall -Wextra -Werror -pedantic -Itests/ui_stubs -I. \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  toolbox_tools.c toolbox_cards.c toolbox_ui.c tests/test_toolbox_ui.c \
  -o .build-tests/test_toolbox_ui
./.build-tests/test_toolbox_ui
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
./.ufbt-venv/bin/ufbt
```

The 2026-10-08 SDK follow-up ran 91 Python tests successfully and 111 Swift tests
(105 passed, six opt-in live/hardware tests skipped), plus portable C protocol,
market and Toolbox parser/UI tests. It covers frame encoding, fragmentation and
resynchronization, CRC,
invalid lengths, handshake/version negotiation, permission decisions, request
IDs and sequences, SSRF policy, redirects, timeout, response limits,
cancellation, USB loss, and National Today extraction.

See [Toolbox validation](docs/toolbox-testing.md) for the new tools' live-provider
commands and the physical-device checklist. Offline success is not a physical
Flipper test.
See [SDK validation](docs/test-results-2026-10-08-sdk.md) for exported consumer
builds, setup/nonce/consent tests and the USB/Bluetooth hardware boundary.

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
Manager to copy `dist/usb_internet_bridge.fap` to `/ext/apps/Tools/`, then launch it
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
