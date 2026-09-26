<p align="center">
  <img src="screenshots/dashboard.png" width="420" alt="SigRoam Dash on Flipper Zero with unique count, AP and BLE, separate 2.4 GHz and 5 GHz counts, and GPS fix">
</p>

<h1 align="center">SigRoam — Flipper Zero Wardriving App</h1>

<p align="center">
  <b>A focused Flipper Zero dashboard for GPS-tagged Wi-Fi surveys.</b><br>
  Pair it with SigRoam scanner firmware on Scout Lite for passive 2.4/5 GHz Wi-Fi,
  BLE observation, sealed microSD logs and WiGLE upload after you stop a survey.
</p>

<p align="center">
  <a href="https://github.com/pingequalab/sigroam-wardriving/releases/latest"><b>Download the Flipper app</b></a>
  · <a href="https://flash.pingequa.com/devices/scout-lite"><b>Flash the Scout Lite scanner</b></a>
  · <a href="https://www.pingequa.com/products/scout-lite"><b>Get Scout Lite</b></a>
</p>

<p align="center">
  <a href="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/host-test.yml"><img src="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/host-test.yml/badge.svg" alt="Host tests"></a>
  <a href="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/fap-build.yml"><img src="https://github.com/pingequalab/sigroam-wardriving/actions/workflows/fap-build.yml/badge.svg" alt="FAP build"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="GPL-3.0 license"></a>
</p>

The Flipper Zero has no Wi-Fi radio. **The Flipper app is the control head; the
external board scans, receives GNSS data and stores the survey.** SigRoam also
speaks the ESP32 Marauder serial protocol, so the app works as a dedicated
dashboard with compatible Marauder scanners. The complete SigRoam workflow below
uses the optional [SigRoam scanner firmware](https://github.com/pingequalab/sigroam-firmware)
on the verified [Scout Lite](https://www.pingequa.com/products/scout-lite) board.

## Why use SigRoam in the field?

- **See whether the survey is healthy.** Dash, Strm, GPS and Sess show AP/BLE
  observations, GNSS fix, satellite and session information. Dash splits the
  AP count into 2.4 GHz and 5 GHz. With the SigRoam scanner, Dash also shows
  capture-quality and drop indicators; stale status does not keep looking live.
- **Hear a new network, without a buzz.** While a survey is running, a rising
  unique count plays one short sound, at most once every 2 seconds, and does
  not vibrate. Turn it off under Settings, `New net tick`.
- **Keep the record on the scanner.** Scout Lite writes a WiGLE-compatible CSV
  and session manifest to its own microSD. The Flipper stores settings, not a
  second survey log. The large unique-BSSID number is a RAM-only estimate for
  progress, not the authoritative CSV count.
- **Finish the survey and send it.** With WiGLE and home Wi-Fi credentials
  configured, stopping a SigRoam scanner session seals the CSV and automatically
  attempts to upload pending sealed files. A successful upload keeps WiGLE's
  `transId` with the file. The idle Dash keeps a one-line summary of that
  round. If sealed files are still waiting, it asks once to upload, or tells
  you to set the WiGLE key or home Wi-Fi. The Upload page shows `Key` and
  `Home`, the queue, and lets you retry. **Rank** shows the WiGLE account
  rank from the last upload status. That number is not this trip's count.
- **Set up upload Wi-Fi on the card.** Put the Wi-Fi name and password in two
  files on the Scout Lite microSD alongside the WiGLE API files. This avoids
  selecting an AP and typing its password through a Flipper `Join WiFi` flow
  when using the complete SigRoam scanner + FAP setup. Factory Marauder has
  its own saved-Wi-Fi and direct-upload workflow; the difference here is the
  first-time input method and SigRoam's after-STOP upload attempt.
- **Recover and diagnose on the move.** Probe distinguishes a missing scanner
  from a responding one; Raw log exposes the serial feed. The app detects a
  stalled feed after USB power handover and shows `Resyncing...` while it retries.

These are workflow features, not claims of faster scanning, greater radio
sensitivity or more accurate GPS than another device.

## Choose your scanner firmware

| On Scout Lite | What you get | WiGLE path |
|---|---|---|
| **SigRoam scanner firmware + SigRoam FAP** | Dedicated passive survey, BLE observation, GNSS and capture/session status on the Flipper | Sealed CSV on Scout Lite microSD; automatic upload attempt after STOP when configured home Wi-Fi is reachable; manual retry from Upload |
| **Factory ESP32 Marauder + SigRoam FAP** | The SigRoam dashboard controlling Marauder over its serial protocol | Marauder writes its own wardrive CSV; use Marauder's own upload workflow or copy its card. SigRoam's sealed-session auto upload does not apply |

Scout Lite ships with **ESP32 Marauder**. SigRoam scanner firmware is an
**optional browser flash**, and Marauder remains available in the
[Scout Lite Web Flasher](https://flash.pingequa.com/devices/scout-lite).
The dedicated scanner image is built for Scout Lite; other Marauder-compatible
boards may run the FAP, but they have not all been bench-tested here.

## Start a survey

1. For the complete workflow, install **SigRoam 0.6** from the
   [scanner firmware release](https://github.com/pingequalab/sigroam-firmware/releases/tag/v0.6).
   Disconnect Scout Lite from the Flipper first. Hold BOOT while connecting its
   USB-C data cable, release BOOT, and write `sigroam_lite.bin` at `0x20000`.
   The [Scout Lite Web Flasher](https://flash.pingequa.com/devices/scout-lite)
   lists the 0.6 image as a browser install option. Keep the
   factory Marauder image if you only want the compatible dashboard.
2. Fit a FAT32 microSD card in Scout Lite and mount it on the Flipper GPIO
   header. The Flipper provides power on pin 1 and communicates on pins 13/14.
   Do not power an ESP32 scanner from pin 9 (3.3 V).
3. Download the FAP from [Releases](https://github.com/pingequalab/sigroam-wardriving/releases/latest)
   and copy it to `SD Card/apps/GPIO/` on the Flipper. Choose the file that
   matches your Flipper firmware; see [Compatibility and limits](#compatibility-and-limits).
4. Set `Settings → System → Log Device` to `Off` so the system log does not
   occupy serial pins 13/14. Open `Apps → GPIO → SigRoam Wardriving`. Use
   **Probe firmware** if the scanner does not respond.
5. Open **Dashboard** and press OK on `Dash` to start. Watch the GPS and
   capture status during the survey. Press OK again to stop; Back only leaves
   the page and does not stop scanning.

For a first run, see the
[Flipper Zero + Scout Lite field guide](https://www.pingequa.com/blogs/guides-tutorials/how-to-first-wardrive-flipper-zero-scout-lite).

## Set up automatic WiGLE upload

**Requires the SigRoam scanner firmware on Scout Lite.** The FAP does not upload
over the Flipper's radio or store the CSV on the Flipper SD card.
The four setup files go on the **Scout Lite card**, not the Flipper card. No
Flipper-side `Join WiFi` password entry is needed for this scanner workflow.

1. Get your **API Name** and **API Token** from your
   [WiGLE account](https://wigle.net/account). Keep them private.
2. Put these four plain-text files in the **root of the Scout Lite microSD**,
   with only the corresponding value in each file:

   | File | Contents |
   |---|---|
   | `wigle_api_name.txt` | WiGLE API Name |
   | `wigle_api_token.txt` | WiGLE API Token |
   | `home_ssid.txt` | Wi-Fi network name used for uploads |
   | `home_psk.txt` | That network's password; leave the file empty for an open network |

3. Insert the card and restart Scout Lite. At boot, the scanner imports the
   values into its internal settings, then overwrites and removes the source
   files. Check that the files disappeared; if they remain, do not assume the
   import succeeded. Never post credentials in an issue or serial log.
4. Start a survey on `Dash` and press OK to stop it. The scanner first closes
   and verifies the CSV and session manifest. It then attempts WiGLE upload
   using the configured Wi-Fi, processing pending sealed files. Leave the
   scanner powered and its card inserted while `Uploading...` or `Sending`
   is shown.
5. Open **Upload** on the Flipper to see `Key` / `Home`, the queue, and the
   result. `Key: none` or `Home: none` means that credential was not imported.
   `Uploaded`
   with a WiGLE `transId` means WiGLE accepted that file; WiGLE's later
   processing or scoring is a separate step. If the Wi-Fi was unavailable,
   return within range and press **Upload** to retry pending files.

**Automatic means “after a successful STOP.”** SigRoam does not periodically
pause a scan to upload or promise to detect your home Wi-Fi and start a new
upload later. Collection is passive; the scanner uses a normal Wi-Fi connection
and HTTPS only in the separate upload phase. A failed upload leaves the sealed
file on the scanner card for retry.

| Upload page | What to check |
|---|---|
| `No SD` | Insert/check the Scout Lite microSD; the Flipper card is not the survey store. |
| `No home Wi-Fi` | Retry where the configured network is reachable. |
| `No reply` / `before upload` | The WiGLE profile check received no HTTP status; the file was not sent. |
| `Key rejected` | Check the WiGLE API Name and Token. |
| `WiGLE busy` | HTTP 429 stopped this upload round; keep the file pending and retry later. |
| `Uploaded` + `transId` | WiGLE accepted the file; retain the ID with that survey. |

You can still copy a CSV from Scout Lite's microSD and upload it manually at
[WiGLE](https://wigle.net/) if you prefer.

## How it fits among Flipper wardriving tools

This compares **documented workflows**, not scan speed or a universal winner.
Hardware and firmware combinations matter: a feature on one scanner does not
automatically work on every GPIO board.

| Setup | Primary use | Where the survey goes and how it uploads |
|---|---|---|
| **SigRoam FAP + SigRoam scanner on Scout Lite** | Focused Flipper field dashboard and scanner-side session visibility | Scout Lite microSD; automatic attempt after STOP plus Flipper manual retry |
| **[ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder/wiki/wardrive) + its [Flipper companion](https://github.com/0xchocolate/flipperzero-wifi-marauder)** | Broad Wi-Fi/Bluetooth toolkit with Wardrive as one function | Hardware-dependent wardrive log; [Marauder Direct Upload](https://github.com/justcallmekoko/ESP32Marauder/wiki/wardriving-direct-upload) supports WiGLE after joining Wi-Fi and choosing an upload action. Initial `Join WiFi` uses AP selection and password entry; saved profiles reduce repeat setup. |
| **[GhostESP + Flipper companion](https://github.com/GhostESP-Revival/GhostESP-FlipperCompanion)** | Broad wireless toolkit with GPS wardriving | GhostESP CSV; its [WiGLE integration](https://docs.ghostesp.net/latest/gps/wigle/) supports automatic upload when a Wi-Fi STA connection is established |
| **[ESP32GPS Wardriver](https://github.com/Sil333033/flipperzero-wardriver)** | DIY Flipper AP/GPS list workflow | Its README documents a CSV saved on the Flipper SD card for WiGLE upload |

For board shoppers, the firmware boundary matters as much as the radio:

| Board | Documented wardriving hardware | SigRoam scanner v0.6 |
|---|---|---|
| **[Scout Lite](https://www.pingequa.com/products/scout-lite)** | ESP32-C5 2.4/5 GHz, onboard GPS and microSD | Verified reference board for SigRoam 0.6; optional SigRoam or factory Marauder firmware |
| **[Apex 5 V2](https://github.com/HoneyHoneyTeam/ESP32-Marauder-5G-Apex-5-Module---For-Flipper-Zero)** | ESP32-C5 2.4/5 GHz, onboard GPS and SD slot; also Sub-GHz and nRF24 | Not verified. Its maker documents Marauder Wardrive on **V2**; V1 has a different wiring limitation |

Comparison sources: linked project documentation, checked 2026-09-26.
Marauder's Direct Upload wiki states an update date of 2026-09-15; its
[Join WiFi CLI page](https://github.com/justcallmekoko/ESP32Marauder/wiki/join)
states 2025-06-05. The Apex 5 manual notes a 2026-07-08 update. The linked GhostESP
and ESP32GPS pages do not state an update date.
For a broader hardware comparison including Biscuit, C5 wardrivers and
multi-radio Flipper boards, see our
[Flipper Zero wardriving comparison](https://www.pingequa.com/blogs/guides-tutorials/flipper-zero-esp32-gps-wardriver-sigroam-marauder).

## Screens and controls

| Dashboard | Upload |
|---|---|
| <img src="screenshots/dashboard.png" width="320" alt="SigRoam Dash showing 2.4 GHz and 5 GHz counts, AP, BLE, GPS fix and capture status"> | <img src="screenshots/upload.png" width="320" alt="SigRoam WiGLE Upload page showing a pending queue and Sending status"> |
| **Probe firmware** | **About** |
| <img src="screenshots/probe.png" width="320" alt="SigRoam Probe page showing Scout Lite, v0.6, and state SCANNING"> | <img src="screenshots/about.png" width="320" alt="SigRoam Wardriving v0.6 about screen, receive-only, 2.4/5G and GNSS, with QR code"> |

The menu entries are **Dashboard**, **Probe firmware**, **Upload**, **Rank**,
**Raw log**, **Settings** and **About**. Dashboard tabs are `Dash` (start/stop
and status), `Strm` (recent records), `GPS` (fix and coordinates) and `Sess`
(session diagnostics). Settings includes baud, sound, vibration, backlight,
Stealth and `New net tick`. Sound, vibration and LED alerts respond to
GPS-fix changes; Stealth suppresses the LED. `New net tick` is sound only.

## Compatibility and limits

- **Flipper firmware:** the v0.6 release provides `sigroam-0.6.fap`, an
  Official-SDK API-87 build for Official and compatible Momentum firmware, and
  `sigroam-0.6-unleashed.fap` for Unleashed API 88. Unleashed-board testing is
  not claimed. Check the [release files and notes](https://github.com/pingequalab/sigroam-wardriving/releases/latest)
  for your installed firmware; do not bypass an API-mismatch warning.
- **Scanner hardware:** Scout Lite is the verified reference for the dedicated
  SigRoam scanner firmware. The FAP speaks the Marauder serial protocol and may
  work with other boards exposing it on Flipper pins 13/14, but those boards
  have not all been bench-tested. GNSS and microSD are needed for located
  WiGLE CSV; an ESP32-C5 provides 2.4 and 5 GHz through one radio, not
  simultaneous dual-radio scanning.
- **Survey policy:** SigRoam does not provide deauthentication, handshake
  capture, evil twin, beacon/BLE spam or password cracking. Scanner survey
  capture is passive. WiGLE upload uses an ordinary Wi-Fi connection after
  scanning stops; “receive-only survey” does not mean the device never
  transmits any network traffic.
- **Power:** pin 1 can receive USB VBUS or battery-powered OTG boost. If USB is
  unplugged during a survey, the scanner can restart; the FAP detects silence
  and tries to resynchronize. Keep the scanner connected while data is being
  saved or uploaded.

This is an early release, not a product-v1.0 field-readiness claim.

## Build from source

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt                  # build
ufbt launch           # build, install and run on a connected Flipper
make -C tools/host_test
make -C tools/host_test asan
```

The host tests cover the pure-logic parser, model and Bloom filter. They do not
replace on-device testing. See [CHANGELOG.md](CHANGELOG.md) for version details.

## License

GPL-3.0. See [LICENSE](LICENSE). The separate
[SigRoam scanner firmware repository](https://github.com/pingequalab/sigroam-firmware)
documents its own distribution and hardware scope.

**PINGEQUA Lab** · [Scout Lite hardware](https://www.pingequa.com/products/scout-lite)
· [Scout Lite Web Flasher](https://flash.pingequa.com/devices/scout-lite)
· [Field guide](https://www.pingequa.com/blogs/guides-tutorials/how-to-first-wardrive-flipper-zero-scout-lite)
