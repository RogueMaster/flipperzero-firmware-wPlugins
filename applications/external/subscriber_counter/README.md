<p align="center">
  <img src="docs/images/banner.png" alt="Subscriber Counter for Flipper Zero" width="840">
</p>

<p align="center">
  <b>A live YouTube subscriber counter for your Flipper Zero.</b><br>
  Big, calm numbers on the desk – refreshed automatically over USB or Bluetooth.
</p>

<p align="center">
  <a href="https://github.com/TheProductAgency/flipper-subscriber-counter/actions/workflows/build.yml"><img src="https://github.com/TheProductAgency/flipper-subscriber-counter/actions/workflows/build.yml/badge.svg" alt="Build"></a>
  <img src="https://img.shields.io/badge/Flipper%20Zero-official%20firmware-ff8200" alt="Official firmware">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT license">
  <a href="docs/README.de.md"><img src="https://img.shields.io/badge/Deutsch-Anleitung-555" alt="Deutsche Anleitung"></a>
</p>

<p align="center">
  <img src="docs/images/demo.gif" alt="Subscriber Counter running on a Flipper Zero" width="460">
</p>

## Features

- **Subscribers, views and videos** on three swipeable cards, with numbers that count up when new data arrives.
- **New-subscriber alert** – a short two-note chime, a haptic tick and a green LED glow. Each one can be switched off.
- **Badge** showing how many subscribers you gained since the app was opened.
- **Info sheet** with the last and next update and the connection type.
- **Settings on the device** – sound, vibration, LED, backlight, update interval (1 min – 1 h), language and font style.
- **English and German** user interface.
- **USB or Bluetooth** via the [Flipper Internet Bridge](https://github.com/mete888/flipper_internet_bridge), or **Wi-Fi** through a [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) board – no computer needed (beta, see [Wi-Fi board](docs/wifi.md)).
- **Three font styles** – [Inter](https://rsms.me/inter/) rasterized pixel by pixel for the 128 × 64 display, the Flipper's own built-in fonts, or a mix of both.

<p align="center">
  <img src="docs/images/gallery.png" alt="Screens: subscribers, views, new subscribers, info sheet, settings, German" width="840">
</p>

## Font styles

Pick one in **Settings → Font**; the screen switches at once.

<p align="center">
  <img src="docs/images/fonts.png" alt="Inter, Flipper and Mix font styles" width="840">
</p>

| Style | Number | Text |
|---|---|---|
| **Inter** | Inter Display Bold | Inter |
| **Flipper** (default) | Built-in big-number font | Built-in fonts – crisp on every screen |
| **Mix** | Inter Display Bold | Built-in fonts |

## How it works

<p align="center">
  <img src="docs/images/architecture.svg" alt="Flipper Zero ⇄ USB/Bluetooth ⇄ desktop helper ⇄ HTTPS ⇄ YouTube Data API" width="840">
</p>

The Flipper has no internet connection of its own. Subscriber Counter embeds the client SDK of the
[Flipper Internet Bridge](https://github.com/mete888/flipper_internet_bridge): the
**Flipper Internet Bridge** desktop helper on your computer forwards one HTTPS request to the
[YouTube Data API v3](https://developers.google.com/youtube/v3/docs/channels/list)
(`channels.list`) every few minutes and passes the answer back. The Internet Bridge app on the Flipper
itself does not need to be open.

Alternatively a Wi-Fi board running [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) on the GPIO
header makes the same request on its own – see [Wi-Fi board (beta)](docs/wifi.md).

## What you need

| | |
|---|---|
| **Flipper Zero** | Official firmware (release channel) |
| **Connection** | A computer running the [Flipper Internet Bridge](https://github.com/mete888/flipper_internet_bridge) desktop helper (macOS, Windows, Linux) – **or** a Wi-Fi board with [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) ([beta](docs/wifi.md)) |
| **YouTube Data API key** | Free, takes about five minutes – see [Get an API key](docs/api-key.md) |
| **Channel** | Your `@handle` or channel ID (`UC…`) |

## Setup

1. **Install the app** from the Flipper Apps catalog (Flipper Lab or the Flipper mobile app, category *Tools*),
   or copy `sub_counter.fap` from the [latest release](https://github.com/TheProductAgency/flipper-subscriber-counter/releases) to `SD card/apps/Tools/`.
2. **Start it once.** It creates `SD card/apps_data/sub_counter/config.txt` and tells you to edit it.
3. **Edit the config** with qFlipper (File manager) and enter your key and channel:

   ```ini
   api_key=AIza...
   channel=@yourchannel
   ```

4. **Start the Flipper Internet Bridge helper** on your computer, open Subscriber Counter, choose USB or Bluetooth and approve the connection.
   Tip: choose *Allow always* in the helper so you are not asked again after every reconnect.

## Controls

| Button | Action |
|---|---|
| ◀ ▶ | Switch between subscribers, views and videos |
| ▲ | Info sheet (last update, next update, connection, errors) |
| ▼ | Settings – also from the info sheet |
| OK | Refresh now (reconnect when offline) |
| Back | Close sheet or settings, then exit |

In **Settings**, ▲ ▼ select a row, OK or ◀ ▶ change it, and Back saves to `config.txt`.

## Configuration

Everything except `api_key` and `channel` can also be changed in the app.

| Key | Default | Meaning |
|---|---|---|
| `api_key` | – | YouTube Data API v3 key |
| `channel` | – | `@handle` or channel ID (`UC…`, 24 characters) |
| `interval` | `300` | Seconds between updates, at least `60` |
| `backlight` | `1` | Keep the display lit while the app is open |
| `sound` | `1` | Chime for new subscribers |
| `vibro` | `1` | Haptic tick for new subscribers |
| `led` | `1` | Green LED for new subscribers |
| `language` | `en` | `en` or `de` |
| `font` | `flipper` | `flipper`, `inter` or `mix` |
| `transport` | `bridge` | `bridge` (desktop helper) or `wifi` ([Wi-Fi board](docs/wifi.md)) |
| `wifi_ssid` | – | Optional network for the Wi-Fi board |
| `wifi_password` | – | Its password |

See [`config.example.txt`](config.example.txt).

## Good to know

- **YouTube rounds public subscriber counts** to three significant figures once a channel passes 1,000
  (for example 12,345 → 12,300), so the number moves in steps. Exact numbers are only available
  through YouTube Analytics with a Google sign-in, which is not possible through the bridge.
- **Quota:** the API allows 10,000 units per day for free; one update costs 1 unit. Every 5 minutes is 288 units per day.
- **Your API key** is stored in plain text on the SD card and sent to Google through the desktop helper.
  Restrict the key to the *YouTube Data API v3* in the Google Cloud Console (see [Get an API key](docs/api-key.md)).
- The sound follows the Flipper's volume and silent mode.

## Troubleshooting

| On screen | What to do |
|---|---|
| *API key missing* / *Channel missing* | Fill in `api_key` and `channel` in `apps_data/sub_counter/config.txt` |
| *Offline* | Start the desktop helper, then press OK to reconnect |
| *HTTP 400: API key not valid* | Check the key and that the YouTube Data API v3 is enabled for its project |
| *HTTP 403: … quota …* | Daily quota used up – choose a longer interval |
| *Channel not found* | Check the handle (with or without `@`) or use the channel ID |
| *Not up to date* | The last update failed; the info sheet (▲) shows why |
| *Wi-Fi board not found* / *Wi-Fi connection failed* | See [Wi-Fi board](docs/wifi.md#troubleshooting) |

## Building from source

```sh
python3 -m pip install --upgrade ufbt
ufbt update --channel=release
ufbt                # dist/sub_counter.fap
ufbt launch         # build, install and start over USB
```

Tests run on any computer – the parser, and the Wi-Fi transport against a simulated FlipperHTTP board:

```sh
cc -std=c11 -Wall -Wextra yt_parse.c tests/test_yt_parse.c -o /tmp/test_yt_parse && /tmp/test_yt_parse
tests/wifi_sim/run.sh
```

### Project layout

| Path | Contents |
|---|---|
| `subscriber_counter.c` | App: UI, animations, settings, data flow |
| `yt_parse.c/.h` | YouTube API response parser and number formatting (portable, tested) |
| `yt_strings.h` | English and German UI text |
| `yt_fonts.c/.h` | Inter rasterized to 1-bit u8g2 fonts, font styles |
| `yt_wifi.c/.h` | Wi-Fi transport: non-blocking FlipperHTTP handshake and GET |
| `vendor/internet_bridge/` | Client SDK of the Flipper Internet Bridge (only reformatted) |
| `vendor/flipper_http/` | FlipperHTTP Flipper library (include path adjusted, reformatted) |
| `docs/` | Catalog description, changelog, guides and images |

## Credits

- [Flipper Internet Bridge](https://github.com/mete888/flipper_internet_bridge) by mete888 – transport and client SDK (MIT)
- [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) by JBlanked – Wi-Fi board library (MIT)
- [Inter](https://rsms.me/inter/) by Rasmus Andersson – typeface (SIL Open Font License 1.1)

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Not affiliated with or endorsed by YouTube or Google. YouTube is a trademark of Google LLC.

## License

[MIT](LICENSE) © 2026 TheProductAgency
