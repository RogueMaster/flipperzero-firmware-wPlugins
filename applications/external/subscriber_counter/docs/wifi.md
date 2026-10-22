# Wi-Fi board (beta)

Instead of the desktop helper, Subscriber Counter can go online through a Wi-Fi board on the
Flipper's GPIO header that runs the open-source
[FlipperHTTP](https://github.com/jblanked/FlipperHTTP) firmware. No computer is needed then.

> **Beta:** this mode has been tested against a simulated board and on a Flipper without a board
> attached, but not yet with real Wi-Fi hardware. If you try it, please
> [open an issue](https://github.com/TheProductAgency/flipper-subscriber-counter/issues) and tell us
> how it went – also when it works.

## Supported boards

Any board flashed with FlipperHTTP, for example the official **Flipper Wi-Fi Developer Board
(ESP32-S2)**. The FlipperHTTP project lists further ESP32, Raspberry Pi Pico W and other boards and
how to wire them: <https://github.com/jblanked/FlipperHTTP>.

Most of these boards only join **2.4 GHz** networks.

## Setup

1. **Flash FlipperHTTP** onto the board, following the guide in the FlipperHTTP repository.
2. **Attach the board** to the Flipper's GPIO header.
3. **Tell the board your Wi-Fi network**, one of:
   - with a FlipperHTTP companion app from the Flipper app catalog, e.g. *FlipWiFi*, which saves
     networks on the board, or
   - in `SD card/apps_data/sub_counter/config.txt`:

     ```ini
     wifi_ssid=YourNetwork
     wifi_password=YourPassword
     ```

     The app first tries the network saved on the board and only sends these credentials when that
     fails, so the board's memory is not rewritten on every start.
4. **Switch the connection**: in the app, *Settings → Connection → Wi-Fi* (or `transport=wifi` in
   `config.txt`). The app switches as soon as you close the settings.

Your API key and channel stay the same as with the desktop helper.

## How it works

The app talks to the board over the GPIO UART (115200 baud) with FlipperHTTP's text protocol:
`[PING]` → `[PONG]`, `[WIFI/CONNECT]`, and for each update `[GET]https://www.googleapis.com/…`,
answered by `[GET/SUCCESS]{"Status-Code":…}`, the JSON body and `[GET/END]`. While the app is open it
pauses Flipper's *Expansion modules* service, which listens on the same UART, and resumes it on exit.

## Troubleshooting

| On screen | What to do |
|---|---|
| *Wi-Fi board not found* | Board seated correctly? FlipperHTTP flashed? The app retries automatically every 30 s, or press OK |
| *Wi-Fi connection failed* | Check SSID and password, 2.4 GHz network in range |
| *Wi-Fi: no " or \\* | Quotes and backslashes are not supported in `wifi_ssid` / `wifi_password` |
| *GPIO UART is busy* | Another app or service uses the UART – close it and press OK |

## Security notes

- `wifi_password` is stored in plain text in `config.txt`, like the API key. The board stores the
  network it joins as well.
- If the board cannot verify a server certificate, the FlipperHTTP firmware retries **without**
  verification. Your API key could then travel over an unverified connection – another reason to
  restrict the key to the YouTube Data API v3 ([Get an API key](api-key.md)).
