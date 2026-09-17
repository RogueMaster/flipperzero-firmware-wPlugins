# 5Ghost WiFi Lab — Changelog

## [Unreleased]

## [2.7.6] — 2026-09-09

> Flipper app only. Board firmware stays **2.7.3** — you do not need to reflash the board.

### Changed
- SD captures now write under `/ext/apps_data/5ghost_wifi_lab/` (the app id folder).
  Files already in `/ext/apps_data/5ghost/` are moved on the next launch.
- About page and app website field point at the public GitHub repository.

## [2.7.5] — 2026-09-05

> Flipper app only. Board firmware stays **2.7.3** — install **2.7.5** from GitHub
> Releases; you do not need to reflash the board.

### Added
- **Guided Audit** on the main menu (between Channel Map and Capture Handshake).
  Same path as public 2.7.4: pick an AP; the app chooses handshake or clientless
  PMKID from PMF and whether stations are present. Complete only if a
  quality-gated PCAP or `.22000` file was written. DFS stays receive-only
  (Unsupported). Capture Handshake and Capture PMKID remain for manual use.

### Fixed
- GPIO UART: disable Flipper Expansion before acquiring USART, so Scan no
  longer false-reports "Check 5V/wiring" when Expansion still owns the port.
- Home menu wraps from the first item to the last and back. Back from About
  stays on About, not BadBLE.

## [2.7.4] — 2026-09-01

> Flipper app only. Board firmware stays **2.7.3** — update the `.fap` from GitHub
> Releases; you do not need to reflash the board.

### Added
- **Guided Audit** on the main menu (between Channel Map and Capture Handshake).
  Pick an AP; the app chooses handshake or clientless PMKID from PMF and whether
  stations are present. The result is a short word: Complete, Partial,
  Unsupported, Timeout, or Blocked. Complete only if a quality-gated PCAP or
  `.22000` file was written. DFS channels stay receive-only (Unsupported).
  Capture Handshake and Capture PMKID remain for manual use.

## [2.7.3] — 2026-09-01

> Firmware + Flipper app. Update both — the 2.7.1 app can time out on the longer scan.

### Changed
- Passive scan now covers 2.4 GHz channels 1–13 and 5 GHz 20 MHz channels
  36–48, 52–64, 100–144, 149–165, so EU 5 GHz APs on DFS and channel 165
  show up in the list.
- Transmit (deauth / AP / beacon / handshake / PMKID) stays off DFS.
  Those channels are receive-only.
- Channel 14 is not included (Japan 802.11b-only; EU 2.4 GHz is 1–13).

## [2.7.2] — 2026-08-08

> Firmware maintenance release. The Flipper app is unchanged from 2.7.1 — dual-band
> Wi-Fi, deauth, Evil Portal, handshake and clientless PMKID capture, and the full
> BLE suite (GATT recon, iBeacon spoofing, BadBLE HID, tracker detection) all behave
> exactly as in 2.7.1. API 87.1, still loads on Official · Momentum · Unleashed.

### Changed
- Firmware housekeeping and version bump to 2.7.2. The Flipper app (`.fap`) is
  unchanged from 2.7.1 and remains current — no reflash of the app is needed.

## [2.7.1] — 2026-08-08

> Multi-AP deauth. Pick any mix of access points — across different SSIDs — straight
> from the scan list and deauth them together in one pass, with the firmware
> round-robining every target (channel-hopping as needed). Same-SSID mesh deauth now
> reliably hits every node in the group. BLE recon/injection, dual-band scan, Evil
> Portal, handshake and clientless PMKID capture are unchanged. API 87.1, still loads
> on Official · Momentum · Unleashed.

### Added
- **Multi-AP deauth from the scan list** — toggle-select any set of access points in
  the scan list, including across different SSIDs, then deauth them all at once. The
  firmware keeps a target list and round-robins deauth across every entry, so all
  selected APs are hit in a single run rather than one at a time.

### Fixed
- **Same-SSID mesh deauth now targets every node**, not just the last one enumerated —
  deauthing a network from its detail page covers all mesh nodes sharing that SSID in
  one pass.
- **Stopping an attack always clears armed targets** — stopping while already idle now
  empties the target list, so a later run never reuses stale targets.

## [2.7.0] — 2026-07-30

> BLE reconnaissance & injection expansion, all validated on real hardware: GATT
> active recon, iBeacon spoofing, BadBLE HID keystroke injection, and tracker
> detection widened to all four big ecosystems (AirTag, Tile, Samsung SmartTag,
> Google Find My). Dual-band Wi-Fi, deauth, Evil Portal, handshake and clientless
> PMKID capture are unchanged. API 87.1, still loads on Official · Momentum ·
> Unleashed.

### Added
- **GATT reconnaissance** — from a scanned device's detail page, actively connect,
  enumerate its GATT services, and read the **Device Information** profile
  (manufacturer / model / firmware) — data a passive scan can't obtain.
- **iBeacon spoofing** — broadcast a spec-compliant Apple **iBeacon** with a
  UUID / major / minor you choose, or a built-in demo identity, for a chosen
  window (30 / 60 / 120 s).
- **BadBLE HID** — advertise a BLE **keyboard**; once the target pairs, type a
  keystroke payload — a built-in preset or your own script loaded from the SD
  card. Manual start / stop / re-send control (BadUSB-style).
- **Tracker detection expanded to all four big ecosystems** — Find My tracker
  detection now also flags **Tile**, **Samsung SmartTag**, and **Google Find My
  Device (FMDN)** alongside Apple AirTag, merged into the main BLE scan.

### Changed
- **Main menu**: new `iBeacon Spoof` and `BadBLE HID` entries join `BLE Scan` in
  the recon block.

## [2.6.1] — 2026-07-26

### Fixed
- **Custom SD portal credentials not appearing on the Flipper or SD card.** New bundled
  portals now submit to the canonical `/submit` path, while the BW16 firmware accepts
  legacy `/userinput` pages already stored on customer SD cards. Existing custom pages
  are preserved and do not need to be edited.

## [2.2.0] — 2026-07-02

> Major update: a new BLE reconnaissance suite (Flipper / Find My tracker detection +
> a general BLE scanner), a from-scratch BW16 firmware (fw 2.3.0), and the P3–P5
> features (dual-band scan, handshake capture, Evil Portal) promoted from
> compile-verified to on-device-verified. API 87.1, still loads on Official · Momentum
> · Unleashed.
>
> ⚠️ **Reflash the BW16 to firmware 2.3.0** to use BLE and dual-band
> scan — the recovery image on `fw.pingequa.com` / the flasher is updated to 2.3.0. The
> BLE menu requires this updated firmware.

### Added
- **BLE reconnaissance** — new `BLE Scan` menu, a passive BLE central sweep that lists
  every advertising device:
  - **General BLE scanner**: MAC, RSSI, advertised name and vendor per device;
    de-duplicated; CSV export to `/ext/apps_data/5ghost/ble_<timestamp>.csv`.
  - **Flipper Zero detection**: flags nearby Flipper Zeros — a bare Flipper can't scan
    BLE natively, so this needs the BW16. By MAC OUI (`0C:FA:22`, `80:E1:26/27`) + name.
  - **Find My tracker detection**: flags Apple Find My devices (AirTag etc.) by
    manufacturer id `0x004C` + offline-finding type byte — an anti-stalking aid.
  - **Vendor identification**: manufacturer company-id → vendor name (Apple, Samsung,
    Google, Microsoft, Tile, …) from the Bluetooth SIG registry, so an unnamed device
    reads as e.g. `~Apple` instead of a raw MAC.
  - **Per-device detail page** (kind / MAC / RSSI / vendor) and a **detection alert
    chip**: the header hides the Flipper/tracker counters while zero and shows an
    inverted `!Flip N` / `!Track N` chip the moment one is seen.
- **About page 4 — Firmware / Version**: shows the module's reported firmware version.

### Changed
- **New BW16 firmware, fw 2.3.0** — a ground-up rewrite that revives
  2.4 GHz scanning (dual-band 2.4/5 GHz verified on device) and adds BLE. **The
  BW16 must be reflashed**; the recovery image is updated to 2.3.0.
- **Main menu reordered (recon-first)**: Scan Wi-Fi → BLE Scan → Channel Map →
  Capture Handshake → Send Beacon → Create AP → About.
- **P3–P5 promoted to on-device-verified**: dual-band passive scan, WPA handshake
  capture (full M1–M4 on 5 GHz), and Evil Portal (iOS captive auto-open) — previously
  compile-verified — are now validated end-to-end on real hardware.
- **Startup splash redesigned**: a pixel-perfect brand splash rendered with a pixel
  font (no more scaled-vector aliasing).
- Internal `DelfyRTL` fork naming cleaned up to `ghost5` (no user-facing change).

### Fixed
- **BLE Scan crash on entry**: three scene-handler arrays were missing the new BLE
  scenes, so opening BLE Scan indexed past the array and hard-faulted the Flipper.
- **BEACON hard-fault**: `strrchr()` is unusable on the AmebaD newlib and crashed the
  beacon path; replaced with a hand-rolled reverse scan.

## [2.1.2] — 2026-06-04

> Compatibility fix. The 2.1.1 binary imported the firmware-provided icon
> `I_WarningDolphin_45x42`, which is not resolvable on every firmware build —
> on some Unleashed builds it failed to load with a "Missing imports" error.
> This release is rebuilt against the official Flipper SDK (API 87.1), which
> bundles that icon into the `.fap` itself, so the app is now self-contained
> and loads cleanly on Official · Momentum · Unleashed. No feature changes.

### Fixed
- **"Missing imports" on launch (firmware-dependent).** The error-popup warning
  icon is now baked into the `.fap` instead of being imported from firmware, so
  the app no longer depends on a given firmware exposing `I_WarningDolphin_45x42`.

## [2.1.1] — 2026-05-29

> Brand-polish follow-up to v2.1. API 87.1, still compatible with Official · Momentum · Unleashed.

### Added
- **Startup splash screen**: the app now opens on a dedicated `PINGEQUA Lab` splash instead of
  blocking invisibly on the HELLO probe. A separate timer drives a short progress bar and retries
  `HELLO`; the app enters the main menu as soon as the BW16 identifies itself or after the old
  ~2.3s timeout. The home-menu Back path now exits the app directly, so it never falls back to the
  splash scene.
- **Ghost Beacon animation pack**: the former RickRoll FAP-side animation is replaced with a 12-frame
  branded ghost sequence (`ghost_anim0..11`) generated from `tools/gen_brand_assets.py` — ghost,
  Wi-Fi ripples, and light particles on a fixed 128×64 canvas.

### Changed
- **Splash branding pass 2**: after the first device check proved the startup path stable but the
  screen felt too brief and too heavy, the splash now holds for a minimum ~1.8s even when `IDENT`
  returns immediately (`GHOST_SPLASH_TICKS_MIN`) while keeping the ~2.4s no-module timeout. The
  old large `ghost_about` block is replaced with a dedicated 128×50 `ghost_splash.png` hero and a
  cleaner bottom status strip / thin progress line.
- **Beacon UI is now Ghost-branded while keeping firmware compatibility**: the menu entry reads
  `Ghost AP Beacon`, the info screen headline is `Ghost`, and the on-screen copy changes to
  `Haunting the airwaves` / `5Ghost Beacon`. The UART command remains `RICKROLL`, so no firmware
  change or protocol drift is introduced.

## [2.1] — 2026-05-26

> Post-v2.0 improvements, validated on device (Flipper + BW16). API 87.1, no `strtok`/`strcat`
> so it still loads on the official firmware.

### Added
- **Dim, calm scanning LED** (task8): a Wi-Fi scan now pulses the LED a low-brightness blue (~12%)
  once per poll, instead of either staying dark or — as reported on device — flashing too harshly.
  Custom low-value blue `NotificationMessage` (the stock blink sequences run at full 255); the LED
  is reset when the sweep ends and again on app exit. Applies to both the main scan and the
  handshake AP picker, which share the same synchronous LISTAP loop (so no extra timer is needed).
- **Custom captive-portal example pack** (task#9, `portals-sd/`): three demo portals with more
  personality than the built-in clones — an on-brand haunted-Wi-Fi ghost page, a retro green-on-
  black CRT access terminal, and an over-the-top "FREE Premium WiFi" bait page. Drop them on the SD
  card and serve via Load Custom Portal. Each is a self-contained file under the 4 KB limit, using
  the `/userinput` GET + `password`-field convention; firmware unchanged.

### Changed
- **Channel Map best-channel footer no longer crowds** (task#12): with both bands present the
  recommendation read `Best 2.4G:6 5G:40` jammed together on device; it now splits the row — 2.4G
  left-aligned, 5G right-aligned — so the two numbers read clearly. Single-band keeps the full
  `Best 2.4G channel N` phrasing.
- **About fully redesigned into one unified three-page layout** (repeated on-device feedback:
  "messy, uneven QR sizes, too much whitespace"): every page is now a 50px visual on the left
  (P1 website QR, P2 GitHub QR, P3 ghost mascot) + a right text column (heading, rule, two lines),
  with a 3-dot page indicator and no header bar so the blocks fill the screen. Both QRs are 50px —
  the GitHub QR now encodes the short link `pingequa.com/gh` (a full repo URL is a v3 code = only
  33px; the short link is v1 = 50px, matching the website QR). New 48px `ghost_about.png` asset.
  (The short link needs a Cloudflare 302 `pingequa.com/gh` → repo to resolve.)
- **Home-header ghost mark dropped** (task7, tried then reverted): an 11×11 ghost beside the bold
  `5Ghost` title read oversized and misaligned at 128×64, so the home screen is back to a clean
  title. The `ghost_hdr.png` asset is kept for a future splash where there's room for it.
- **About reworked into a three-page sibling-product card** (matches the PINGEQUA RF Lab style):
  a big `PINGEQUA WiFi Lab` title + rule, then left text / right QR, Up/Down to flip pages. P1
  website (version, bands, tagline, `pingequa.com` + the pingequa.com QR), P2 the `5Ghost` repo
  (a GitHub QR + a short `github.com/pingequalab` handle), P3 authorized-use legal. A compact
  3-dot indicator (top-right, hints Up/Down) replaces the wider `k/3` string. The v2.0
  vertical-scroll version read poorly on device.
- **GitHub QR encodes the full `https://` repo URL** — zero hosting setup, and the scheme makes
  strict scanners open it (WeChat treats a scheme-less payload as plain text and won't jump; iOS
  Camera / Google Lens / Android open it either way). The scheme is free: 46 chars is still a v3
  code = 33px. A 50px code would need a ≤17-char payload — a hosted short link *without* https —
  which reintroduces the no-scheme problem, so 33px-with-https is the reliable choice. The P2 text
  is trimmed to `5Ghost` / `on GitHub` / `@pingequalab`, centred against the QR. (The website QR
  stays the bare 50px `pingequa.com`.)
- **FAP renamed `ghost5_wifi_lab` → `5ghost_wifi_lab`** (appid + `.fap` filename + icons header) to
  match the `5Ghost` brand and the `5ghost-wifi-lab` repo. fbt accepts the leading digit.
- **Firmware "update available" nudge moved to the home header**: an outdated official module now
  shows `Official !` on the home screen (previously only on the About page), so the hint is seen
  first; the About legal page is left clean.
- **App version synced to 2.0**: `GHOST_APP_VERSION` was still `1.2` while the released
  `fap_version` was `2.0`, so the About page showed a stale version. Now `2.0`.
- **Default fake-AP SSID is now `PINGEQUA`** (was `TEST`) — brand presence in Create AP / beacon.

### Fixed
- **Send Beacon now says it's broadcast-only** (#3): a user expected to *connect* to a Send Beacon
  SSID, but beacon spam only advertises the name — no AP accepts the connection. The custom-SSID
  beacon screen now reads "Broadcast only - not joinable. Capture? Create AP.", pointing at Create
  AP, which starts a real joinable softAP + captive portal. (No behaviour change to the beacon
  itself; this is a clarity fix.)
- **Handshake AP picker no longer shows blank rows**: the 5GHz picker built its menu rows from the
  raw SSID, so non-ASCII (e.g. CJK) and hidden/empty SSIDs rendered blank in the 1-bit font. It now
  runs each SSID through the same `ghost_ui_ssid()` sanitizer (non-ASCII → `?`, empty → `<hidden>`)
  as the main scan list.

## [2.0] — 2026-05-25 — first public release

> Software-only hardening of the same hardware (Flipper + BW16/RTL8720DN) toward
> integration / reliability / dual-band / PMF-awareness. Roadmap: `docs/PLAN.md` (P0–P9).
> Items are compile-verified; on-device validation status noted per phase.

### Added
- **Full UI redesign** (traditional / Marauder style): every screen is now custom-drawn for the
  128×64 display — home menu, scan list (lock + SSID + RSSI + encryption + PMF, same-SSID mesh
  markers, non-ASCII SSIDs sanitized), AP detail card, Channel Map, and a scrolling About page.
  Replaces the stock submenus; shared-widget screens cut from 4 to 3.
- **Three-firmware compatibility**: the FAP builds and runs on Official (1.4.3), Momentum
  (mntm-012), and Unleashed (unlshd-089). Removed `strtok` / `strcat` — disabled in the official
  firmware API, so the app would fail to load there — replacing them with `strchr` / `snprintf`
  across 5 call sites (MAC parse, beacon list, Evil credential / error / IDENT parsing).
- **Channel Analyzer** (P8 headline): a new main-menu tool that turns the last scan's AP list into a
  congestion view. 2.4GHz is a fixed 1–13 vertical histogram with the recommended non-overlapping
  channel (1/6/11, fewest APs within ±2) drawn filled; 5GHz is a scrollable list of the channels
  actually seen (number + bar + count) with the least-busy channel marked. A tab header shows the
  active band; Left/Right toggles 2.4G⇄5G, Up/Down scrolls the 5G list. Its own Canvas View + view
  model (not the shared widget). FAP-only — channel/RSSI already arrive in the LISTAP rows.
  On-device verified.
- **Custom Evil Portal from SD** (task10): pick any `.html` from the Flipper SD
  (`/ext/apps_data/5ghost/portals/`) and serve it as the captive portal — a real step up
  from the predecessor's hardcoded pages. New firmware `Custom` portal type + chunked,
  ACKed `PHTMLBEGIN`/`PHTMLDATA` hex upload (paced so the slow command reader doesn't
  overrun its UART RX ring). FAP file browser + 96-byte chunk streaming with a loading
  spinner. Compile-verified; end-to-end pending hardware.
- **SD-save feedback** (P3): green success / red+vibro notification after every SD write
  (scan CSV, captured credentials, handshake PCAP) so a save is no longer silent.
- **Promiscuous capture framework** (P1): firmware `SNIFF <ch>` / `SNIFFSTOP` with a
  light frame-classifying callback (mgmt/beacon/probe/deauth/data/EAPOL) + ~1/s
  `SNIFFSTAT:` report. Original code on stock AmebaD 3.1.7 (not ported from third-party
  GPL). **On-device verified** (ch6, 15s → 466 frames, mgmt+data captured) — clears the
  project's largest technical risk.
- **Handshake capture** (P2): firmware `CAPTURE <bssid> <ch>` / `CAPSTOP` — promisc +
  burst-then-listen deauth to force a client reconnect, captures the target's EAPOL
  frames + a beacon, streamed out as hex; `tools/bw16.py` writes a standard PCAP
  (LINKTYPE_IEEE802_11). **On-device verified**: captured a complete 4-way handshake
  (M1-M4, crackable) on 5GHz. On 2.4GHz this chip couldn't hear the client's uplink
  M2/M4, so handshake capture uses 5GHz.
- **Passive AP discovery** (`LISTAP`, P3 core): promisc beacon sweep across 2.4+5GHz
  parsing SSID/BSSID/channel — replaces the firmware's active scan, which returns no
  results on Ameba 3.1.7. On-device verified (34 APs, both bands).
- **On-device handshake capture in the FAP** (P2, write-to-SD): the PCAP writer that was
  prototyped in `tools/bw16.py` now lives in the FAP, so the Flipper captures and saves
  handshakes standalone (no Mac). New "Capture Handshake" main-menu item, a LISTAP-based
  AP picker, and a capture screen showing live EAPOL
  count. Frames arrive as hex over UART, are decoded and written as a classic libpcap
  (LINKTYPE_IEEE802_11) to `/ext/apps_data/5ghost/handshake_<timestamp>.pcap`. Compile-
  verified both ends; the PCAP global header matches the on-device-verified sample
  byte-for-byte. Capture is self-contained (its own LISTAP sweep + `CAPTURE <bssid> <ch>`)
  and does not go through the deauth/evil index path.
- **Precise PMF / encryption / RSSI in scans** (P4): the firmware parses each beacon's RSN IE
  during the `LISTAP` sweep (`parse_rsn`) to recover the real security bitmask, the precise
  802.11w level (MFPC capable / MFPR required; WPA3-SAE forced to required), and per-AP RSSI —
  so `LISTAP` now reports 7 fields `AP:idx|ssid|bssid|ch|security|rssi|pmf`. The FAP consumes the
  PMF field directly: the networks list shows `P` (capable) / `P!` (required), the AP-info screen
  marks the security line `PMF` / `PMF!`, and encryption type + RSSI come from the real beacon
  instead of the (empty) active scan. Falls back to the v1.1 security-bitmask heuristic when the
  field is absent (legacy path). Compile-verified both ends.

### Fixed
- **Captive portal now auto-opens on iOS** (task25 ④, on-device): the firmware served the portal only
  for a short path whitelist and returned 404 for everything else — so iOS's `/hotspot-detect.html`
  probe got a 404 and the phone showed "no internet" instead of auto-opening the captive sheet. A
  captive portal must intercept *every* path, so the firmware now serves the portal for all paths
  (credential capture still gated to the form's `/userinput` submit). iOS auto-opens; verified on
  device. Android (Samsung) auto-open is limited by Private DNS / DoH, which resolves the
  connectivity-check domain off-device and bypasses the DNS hijack — an inherent captive-portal
  limitation, not fixed here; the portal still appears when the user opens any HTTP page (DNS hijack
  + serve-all fallback). Deeper Android auto-open work deferred.
- **Custom-portal load no longer freezes the app** (task10, on-device): "Load Custom Portal" called
  the blocking `dialog_file_browser_show` directly from the VariableItemList enter callback, nesting
  the file-browser's GUI loop inside the input-handling stack frame — every button locked up. The
  enter callback now only posts a custom event; the browser is opened from the scene's event handler
  on the dispatcher loop, after the input stack unwinds. (Scene transitions from the same callback
  are fine — only the nested blocking dialog deadlocks.) On-device verified.
- **Custom-portal HTML no longer arrives corrupted** (task10, on-device): each `PHTMLDATA` line was
  203 B (96 HTML bytes → 192 hex + header), but the AmebaD UART RX ring is only 128 B
  (`SERIAL_BUFFER_SIZE`, RingBuffer.h). The firmware drains it at ~1 byte/3 ms, far slower than the
  115200-baud burst fills it, so each line overran the ring and ~1/3 of every chunk was dropped —
  the served page rendered as garbled HTML on the phone. Chunk size cut 96 → 40 bytes (91 B/line),
  so a whole line fits inside the ring before any draining. FAP-only fix; on-device verified
  (clean page + credential captured end-to-end through a custom portal).
- `SCAN` now disables any residual promiscuous mode first and retries a bounded number of
  times (emits `SCAN:ERR` instead of looping forever) — fixes scans that hung after a
  prior `SNIFF`/`CAPTURE`. The dev tool now echoes raw firmware output for diagnosability.
- **Capture Handshake now writes the PCAP on exit** (task24): pressing Back during a capture used
  to tear the scene down before the firmware's HSDONE arrived, so the flush — which runs in the
  HSDONE handler — was delivered to the wrong scene and nothing was saved. Back now sends CAPSTOP
  and keeps the capture scene up until HSDONE flushes the PCAP and shows the filename; a second
  Back exits. Because the firmware is idle by the time the scene pops, the AP picker's LISTAP
  sweep on return no longer hangs ~15s or comes back empty (same root cause as the "stuck exit"
  and "empty list" reports). FAP-only change (no firmware/protocol change); compile-verified,
  on-device validation pending.
- **Evil Portal credentials screen no longer locks up** (task25 ②): `evilCredsView` had been
  backed by the shared `app->widget` (the same View object registered under five view IDs) and
  re-rendered itself by re-running its own `on_enter` (widget_reset + view switch) on every
  captured credential — producing an immediate "ViewPort lockup" on hardware. Rewritten to use a
  dedicated `TextBox` view; arriving credentials only push text via `text_box_set_text()`. The
  Evil button also no longer resets the widget mid-button-callback. Compile-verified; end-to-end
  capture pending hardware.
- **Captive portal page rebuilt** (task25 ⑤): the upstream template shipped malformed markup
  (literal `\"` escapes inside raw strings, a `getElementById("isURL")` on a nonexistent element).
  New page is a clean modern router re-auth card (inline SVG, no base64 blob), visually distinct
  from stock delfyRTL builds. Also fixed `default_web()` returning a local String's `c_str()`
  (dangling pointer).
- **Empty scan list now explains itself** (task26): instead of a blank list (usually a module
  flashed without a reset, not responding), it shows "Module not responding" / "No APs found"
  with check-wiring / reflash hints and a one-tap rescan.
- **Fake-AP SSID/password memory bugs** (upstream UB): `ssidAP` was malloc'd then overwritten with
  a string literal, `passwordAP` was never allocated (and `passwordAPLength` never set), so editing
  the fake-AP name/password wrote into read-only memory and `app_free` freed a literal. Both are
  now real heap buffers, freed on exit. Also zeroed `fw_caps`/`fw_version`/`status_label` so
  pre-IDENT menu rendering can't read uninitialised heap. **On-device verified**: Create AP with a
  custom edited SSID + password starts cleanly (no crash).

### Changed
- P0: reverted the rejected custom menu to the clean stock baseline (kept in `backups/`);
  all v1.1 features retained.
- Firmware IDENT now advertises `fw=1.1.0`
  (reflecting the P1/P2 promisc + capture work). FAP bumped to 1.2; `GHOST_LATEST_FW`
  synced to 1.1.0 so the matching module doesn't falsely show an update hint. **The BW16
  must be reflashed** for the handshake menu to appear.
- **Scan Wifi switched to LISTAP; deauth / Evil Portal decoupled from the broken active scan**
  (P4/P5): the FAP's main "Scan Wifi" now sends `LISTAP` (passive beacon sweep) instead of the
  active `SCAN`+`LIST`, which returns zero APs on Ameba 3.1.7 — so scanning finally lists APs on
  real hardware, with precise PMF / encryption / RSSI. To keep indices consistent, the firmware's
  deauth loop, Evil-Portal SSID, and the DEAUTH/EVIL/STOP bounds now index `ap_list[]` (filled by
  LISTAP) instead of the empty `scan_results[]`. **These two ship together**: switching the FAP to
  LISTAP while deauth still read `scan_results[]` would index an empty vector and crash. Compile-
  verified both ends; **on-device validation of deauth + Evil Portal is still required** (this
  changes the attack path). The legacy `SCAN`/`LIST` firmware commands remain for the Mac dev tool.
- **Deauth hits every same-SSID node in one pass** (task23): the firmware's deauth loop used to
  transmit to one target BSSID per `loop()` and round-robin through the set, leaving a gap a roaming
  client could slip through. It now blasts every selected node back-to-back within a single pass
  (same-channel nodes with no retune between them), shrinking that gap — a meaningful improvement
  against a same-channel Mesh. Single-AP deauth is unchanged. Cross-channel Mesh with 802.11r
  fast-roaming is still hard to fully suppress on single-radio hardware (inherent limit). Compile-
  verified (firmware 795388 B); on-device effect pending (requires reflashing).
- **Spanish leftovers from upstream delfyRTL translated to English** (task21): ~15 Spanish comments
  rewritten in English and the `cadena`→`cmd` / `chanel`→`channel` identifiers renamed. No behaviour
  change; compile-verified.

## v1.1 — 2026-05-21

### Added
- **PMF / 802.11w detection** (task8): networks list shows a `P` flag and the AP
  info screen marks the security line `PMF!`. Derived from the Realtek `security`
  bitmask the firmware already reports (WPA3 / mixed / AES-CMAC) — no firmware change.
  Surfaces that deauth frames are ignored by 802.11w-protected APs.
- **SD export** (task9): under `/ext/apps_data/5ghost/`
  - `scan_<timestamp>.csv` — scan results (RFC4180-quoted SSIDs, incl. a `pmf` column),
    written once per fresh scan.
  - `creds_<timestamp>.txt` — Evil Portal captures, appended live as they arrive.
- **Branded About page + update detection** (task11): title, live module/firmware
  status, UI legend (`D`/`P`), a QR code to pingequa.com, and an "Update!" hint when an
  official module reports firmware older than the app knows about.
- **Brand assets** (task17): app icon, 128×64 logo, and the About-page QR, reproducible
  via `tools/gen_brand_assets.py`.
- **CI** (task12): `.github/workflows/build-fap.yml` builds the FAP against the official
  Flipper SDK on push/tag/PR.

### Fixed
- Initialise `wifiCount` (uninitialised `malloc` could crash the first scan's free loop).
- Guard a NULL token in the `EV:` credential parser.

### Notes / deferred
- **WPA/WPA2 handshake capture (task14)** deferred until after on-device validation of
  the base link (task15–16). Chosen capture format: **pcap** (Wireshark-readable,
  convertible to hashcat 22000). Firmware path will use `wifi_set_promisc`, written from
  the SDK API (not ported from third-party GPL of unconfirmed license).
- Startup splash deferred (would touch the validated handshake startup flow for marginal
  value); brand presence covered by the menu status line + About page.
- `delfy_rtl.png` orphaned by the new `ghost_icon.png` app icon; kept for reference.
- Functional correctness of all features awaits first real-device flashing (task15–16).
  Software changes are compile-verified only (`ufbt` APPCHK / `arduino-cli`).

## v1.0

- Initial 5Ghost rebrand of delfyRTL (MIT): L1 text handshake (`HELLO`/`IDENT`),
  BW16 (RTL8720DN) IDENT firmware. (tasks 1–6, 13)
