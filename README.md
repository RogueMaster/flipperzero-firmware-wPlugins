# Pack Track

A native shipment tracker for the Flipper Zero — a clean, glanceable list of
your packages on a 128×64 monochrome display. Works two ways:

- **Manual (default):** keep the list yourself in a plain text file on the SD
  card. No internet, no accounts, no backend.
- **Live (optional):** with a WiFi devboard running FlipperHTTP and *your own*
  tracking-service API key, press RIGHT to fetch real status. The app hardcodes
  no provider — **you** supply the URL and which JSON fields to read, so it works
  with any tracking service you have an account with.

> **Nothing is hosted or signed up for by this app.** In live mode, every
> credential — WiFi, API key, and the service itself — comes from you and lives
> in a config file on your own SD card.

---

## Setting up your packages

On first launch Pack Track creates a template file for you at:

```
SD card: /apps_data/package_tracker/packages.txt
```

Edit that file to list your shipments — **one package per line**, fields
separated by ` | ` (a pipe):

```
Label | Carrier | Tracking | Status | Location | Updated
```

- **Status** must be one of: `pending`, `transit`, `out`, `delivered`, `exception`
- Lines starting with `#` are comments and are ignored.
- Blank lines are ignored.
- Up to **12** packages are shown.

### Example `packages.txt`

```
# Pack Track - one package per line:
#   Label | Carrier | Tracking | Status | Location | Updated
# Status: pending, transit, out, delivered, exception
Flipper Case   | UPS   | 1Z999AA10123456784     | transit   | Memphis, TN     | Apr 17 2:14 PM
Solder Paste   | USPS  | 9400111899223596012345 | out       | Local Facility  | Apr 18 8:02 AM
Oscilloscope   | FedEx | 771234567890           | delivered | Front Door      | Apr 16 9:41 PM
PCB Order      | DHL   | 1234567890             | pending   | Shenzhen, CN    | Apr 15 5:30 AM
```

### How to edit the file

- **qFlipper** (easiest): open qFlipper → File Manager → browse to
  `apps_data/package_tracker/` → drag `packages.txt` out, edit it in any text
  editor, drag it back.
- **SD card reader:** power off the Flipper, pop the microSD into your computer,
  and edit `apps_data/package_tracker/packages.txt` directly.

Changes take effect the next time you open the app (Pack Track reads the file on
launch).

---

## Installation

### Build with ufbt (recommended)

```bash
# From the project root
ufbt
ufbt launch    # build, upload, and start on the connected Flipper
```

Or drop `dist/package_tracker.fap` into `apps/Tools/` on your Flipper's microSD
to sideload manually.

### Build inside the firmware tree

Clone Pack Track into `applications_user/package_tracker/` of your firmware
checkout (Official, Momentum, Unleashed, or RogueMaster) and run the firmware
build. It appears under **Apps → Tools → Pack Track**.

---

## Usage

| Screen | Input | Action |
|--------|-------|--------|
| List | ▲ / ▼ | Move selection (scrolls automatically beyond four rows) |
| List | OK | Open detail view for the highlighted shipment |
| List | ▶ | Refresh all (live mode; needs board + `config.txt`) |
| List | BACK (short) | Exit the application |
| Detail | ◀ / ▶ | Page between shipments without returning to the list |
| Detail | BACK (short) | Return to the list |
| Refreshing | BACK | Cancel the refresh |
| Anywhere | BACK (long) | Force-exit |

Each list row shows a status glyph, the label, and a short status code, plus a
`current/total` counter in the header. The detail view shows carrier, full
tracking number, last reported location, and the date you entered. If no
packages are configured, the app points you to the `packages.txt` file.

---

## Status glyphs

| Status | Glyph |
|--------|-------|
| Delivered | filled dot |
| Out for Delivery | ringed dot |
| In Transit | hollow ring |
| Pending | dash |
| Exception | ✕ |

Glyphs are drawn procedurally on the canvas — no bitmap assets.

---

## Project layout

```
flipper-pack-track/
├── application.fam        # FAP manifest (app id, entry point, metadata)
├── package_tracker.c      # UI, event loop, file loading, refresh worker
├── tracker_util.c/.h      # config parse, URL templating, JSON extraction
├── http.c/.h              # FlipperHTTP UART client (WiFi + GET)
└── README.md
```

Registered as an external `Tools`-category FAP with entry point
`package_tracker_app`. The live-tracking helpers in `tracker_util.c` are pure C
(host-tested); `http.c` talks to the board over the GPIO UART at 115200.

---

## Live tracking (optional)

To fetch real status instead of editing it by hand, you need three things —
**all provided by you, nothing hosted by this app:**

1. **A WiFi devboard running [FlipperHTTP](https://github.com/jblanked/FlipperHTTP).**
   Flash it onto the board from your Flipper (no computer needed). This is what
   gives the Flipper internet access.
2. **A tracking service account + API key.** Sign up with any tracking service
   or aggregator (e.g. Ship24, AfterShip, TrackingMore, 17track) and get your own
   key. Pack Track is provider-agnostic — you point it at whatever you use.
3. **A filled-in `config.txt`** (created automatically at
   `apps_data/package_tracker/config.txt`).

### `config.txt` format

```
WIFI_SSID = MyNetwork
WIFI_PASS = mypassword

# The request URL. {tracking} and {carrier} are replaced for each package.
# Put your API key wherever your service wants it (query string or a header).
URL = https://api.example.com/track?number={tracking}&carrier={carrier}

# Optional headers (repeatable) — e.g. an API key header:
HEADER = Authorization: Bearer YOUR_KEY

# Which JSON fields to read from the response. Dot notation; a number means an
# array index. Example for a response like {"data":[{"status":"...","location":"..."}]}:
FIELD_STATUS   = data.0.status
FIELD_LOCATION = data.0.location
FIELD_UPDATED  = data.0.checkpoint_time
```

### Using it

- Add packages in `packages.txt` as usual (the tracking number + carrier are what
  get sent). Status/location/date can be left as placeholders.
- Open the app and press **RIGHT** to refresh. The board connects, each package
  is looked up, and the fetched status/location/date replace what's shown.
- The header shows progress ("Refreshing 2/5…") and the result ("Updated",
  "No board found", "WiFi failed", "No URL in config.txt"). **BACK** cancels a
  refresh.
- If `config.txt` has no `URL`, the app stays in manual mode.

### Notes & limits

- Your WiFi password and API key sit in **plaintext** on the SD card. Fine for a
  personal device; don't share the card.
- The JSON field reader handles nested keys and array indices, string/number
  leaves. Very large or deeply-irregular responses may not parse — pick a service
  with a simple response, or the fields nearest the top.
- The board and the FlipperHTTP firmware must be attached during a refresh.

---

## License

Released under the MIT License.
