# FlipDeFlock Roadmap

A living plan, ordered by rough priority rather than a schedule. Current release:
v0.43. The principles at the bottom constrain everything above them.

For the full per-version history see [changelog.md](changelog.md).

## Now (highest priority)

- [ ] **Hardware field test the whole app.** Most work since v0.20 is
  compile-verified via CI, not tested on real gear. Validate on an actual ESP32
  board against a real Flock deployment: the BLE Flock serial readout, the
  WATCHSCORE states and thresholds, the anti-stalking follow-gate, the map
  projection and scale bar, the NFC default-key deep check, the flasher
  backup/flash, and that the Share-to-DeFlock QR actually scans on a phone. Tune
  the `#define` thresholds from field data.
- [ ] **Seed the probe IE-fingerprint table.** The pipeline ships inert (empty
  built-in table = no behaviour change). v0.42 made it *field-seedable*: an
  `"ie_fps"` list in `signatures.json` registers user fingerprints (capped at the
  "Class?" rung), and each detection's fingerprint is shown as `IE-fp:` on its
  detail screen to harvest from a confirmed unit. Remaining work: capture from
  corroborated Flock units and, once validated, promote vetted hashes into the
  compiled-in `flock_db.c` table (the only path that can score above "Class?").

## Next

- [ ] **Discreet mode** for the BLE serial / WATCHSCORE display: haptic-only, no
  on-screen "AUDIO SURVEILLANCE HERE", so reading the screen in public isn't a
  personal-safety exposure. (Now more relevant: v0.25 can positively label a
  Raven as audio surveillance on-screen.)
- [ ] **Ship a seed `signatures.json`.** The SD-card signature loader (load-only,
  fail-safe, merged over the built-ins) now covers OUIs, SSID substrings, and — as
  of v0.42 — IE fingerprints. Next: distribute a community-maintained
  `signatures.json` so users get fresh signatures without waiting on a release. The
  remaining loader gap is **BLE name patterns** (deferred: BLE Flock ID is already
  robust via the `0x09C8` manufacturer advert, and a user name-substring classifier
  is false-positive-prone — revisit only as an additive-only signal).
- [ ] **CI: retry the ESP32 core install.** The firmware build's "Install ESP32
  core" step flaky-fails on the arduino-cli core download and currently needs a
  manual re-run. Wrap it in a retry so it self-heals.

Shipped recently (full detail in [changelog.md](changelog.md)): **Net Guardian**
(v0.34–v0.41, the fused always-on watch face + Suspicious list), the **Locator**
RSSI-homing HUD (v0.41), the **Raven vs Falcon split** (v0.25, positive Raven ID
via its 0x3100–0x3500 GATT services; Falcon never asserted by elimination), the
**updatable signature database** (v0.25, extra OUIs/SSID patterns from
`signatures.json`), and **field-updatable IE fingerprints** (v0.42, `"ie_fps"` in
`signatures.json` + the on-screen `IE-fp:` readout, to catch MAC-randomizing units),
and a **precision/correctness pass** (v0.43: stricter `Flock-XXXXXX` Confirm gate,
NFC/GPS/report-escaping fixes, and a 291-check host test suite wired into CI).

## Later

- [ ] **Multi-vendor classification in reports.** Say which vendor/model, not just
  "Flock" (the camera OUIs in `oui_vendor.c` already cover Hikvision/Dahua/Axis).
  Hard-cap any non-Flock ALPR vendor at LOW / NEEDS-VALIDATION; never assert a
  vendor without a verified passive signature.
- [ ] **Beacon/association cross-check.** Catch cameras that are associated and
  not probing by admitting WiFi data frames, gated hard on Flock OUI/SSID on the
  ESP side so it can't flood the table.
- [ ] **Map polish.** Optional point-cycling/pan and a north-up vs heading-up
  toggle.

## Deferred on purpose (not planned, with reasons)

- **mfkey32 NFC key recovery** — needs active card emulation to harvest a reader's
  nonces. Outside the passive posture. Only revisit as a clearly separated,
  explicitly consented mode.
- **Direct on-device DeFlock/OSM submission** — OSM ingestion needs OAuth2 + TLS
  on the ESP and there is no headless auth flow, so it would break the no-network
  promise. The QR phone-handoff is the substitute.
- **Sub-GHz detection (CC1101)** — a real future direction (RF energy-presence
  sweep, and FSK/OOK device decoding), but it can't detect Flock (Flock backhaul
  is LTE, outside the CC1101's bands), the CC1101 can only dwell on one frequency
  at a time, and the alarm/PIR-sensor decode idea carries an ethics flag (it maps
  private home-security devices). If this lane opens, scope it carefully and keep
  it off the Flock confidence ladder.

## Community and sustainability

- [ ] Submit to the official Flipper app catalog, awesome-flipperzero, and the
  Momentum/RogueMaster catalogs.
- [ ] Stand up the Discord server (blueprint + setup script are ready) and add the
  invite to the README and the in-app About once it is live.
- [ ] Cut a demo clip (the BLE following flag and the live map) for launch posts.
- [ ] Launch sequence: the DeFlock community first (contributor-first), then an
  r/flipperzero show-and-tell, a Show HN, r/privacy, and short reels.
- [ ] Funding: enroll GitHub Sponsors (the button is already wired), consider
  Ko-fi, apply to NLnet/NGI Zero, and decide on a pre-flashed companion ESP32
  (own SKU vs affiliate).

## Principles (do not break these)

- Passive recon only. No transmit, inject, jam, deauth, or replay.
- A false positive is worse than a missed detection. Precision over recall.
- The `.fap` loads entirely into ~256 KB of RAM. Justify every byte.
- API 87.1, and it must build with both `ufbt` and `fbt`.
- Detections are indicators, not proof. Never over-claim.
- The app stays free and open source (GPL-3.0-or-later).
