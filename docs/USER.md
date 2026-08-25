# User Guide

This guide covers what you need to run **Flipper API Caller** on your
Flipper Zero, how to set up the Wi-Fi board, and how to use the app today.

## Requirements

- A [Flipper Zero](https://flipperzero.one/) with recent official/custom firmware
- A Wi-Fi board that speaks the FlipperHTTP protocol:
  - **ESP32** (official Wi-Fi Devboard or a bare WROOM/S2 devkit) running
    [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) **v2.2.0**, or
  - **BW16 - to be tested** (RTL8720DN) running
    [BlackMagic](https://github.com/SkeletonMan03/FlipperZeroBlackMagic)
- The latest `.fap` from the
  [Releases](https://github.com/todotge/Flipper-api-caller/releases) page,
  copied to `apps/Tools` on the Flipper (e.g. with qFlipper)

### Board wiring (bare devkits)

The official Wi-Fi Devboard plugs directly into the GPIO header. For a bare ESP32 devkit, wire it like this:

| ESP32 pin | Flipper pin |
| --------- | ----------- |
| TX        | 14 (RX)     |
| RX        | 13 (TX)     |
| 3v3       | 9           |
| GND       | 11          |

Power: the Devboard runs from the Flipper 3v3 rail; a bare devkit is best
powered from its own USB port.

## FlipperHTTP firmware

- Repository: <https://github.com/jblanked/FlipperHTTP> (build the release
  **v2.2.0**, the version this app is tested against)
- Web flasher: <https://flipperhttp.jblanked.com/> (targets the official
  Wi-Fi Devboard)

Make sure the build you flash matches your chip (ESP32-S2 Devboard build vs ESP32 WROOM build). BW16 users must flash BlackMagic instead.

## Using the app

### 1. Connect to Wi-Fi

1. Open the app → **Connessione**.
2. **Ricerca reti**: wait for the scan, pick a network from the live list.
3. Type the password → the result screen shows the outcome.
4. **Reti salvate** lists known networks for quick reconnection;
   **Disconnetti** disconnects the board.

### 2. Save an API call

1. Main menu → **Aggiungi chiamata**.
2. Fill the fields:
   - **URL**: the request target (see limitations below)
   - **Tipo**: `HTTP` / `HTTPS` (used to prepend the scheme automatically)
   - **Metodo**: GET, POST, PUT, DELETE, PATCH or HEAD
   - **Query**, **Headers (JSON)**, **Body (JSON)**: optional
3. **Salva** → the call appears in the list.

### 3. Run a call

1. Main menu → **Lista chiamate**.
2. Pick the call → **Esegui**.
3. The result screen shows method, URL, status code and the response body.
   - **UP / DOWN**: scroll one line
   - **OK**: next page
   - **BACK**: back to the menu

**Modifica** opens the same form pre-filled, where you can also delete the
call.

### 4. Logs

Every run is appended to `debug.log` in the app data folder on the SD card
(`apps_data/api_caller/debug.log`), with SEND / RESULT / ERROR entries. The
log self-truncates at 32 KB.

## Current limitations

- The Flipper's built-in keyboard has only letters, digits and `_`: URLs
  containing `:`, `/`, `.` cannot be typed yet. Type the host and the app
  prepends `http://` / `https://` automatically. An extended keyboard is
  planned for version 0.1.5.
- Headers and Body must be valid JSON; `{}` is sent when the field is empty.
- Body input is single-line.
- Responses are shown up to 32 KB (a notice appears if truncated).
- One request at a time; requests time out after 30 s.
- HEAD is executed as GET (the board protocol has no HEAD).
