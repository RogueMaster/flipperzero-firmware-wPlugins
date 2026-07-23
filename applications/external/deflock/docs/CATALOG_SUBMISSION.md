# Publishing FlipDeFlock to the Flipper App Catalog

The official catalog (lab.flipper.net) is a PR-based, open-source-only store. Your
source stays in this repo; you add a small `manifest.yml` pointer to the catalog
repo. ESP32-companion apps and passive security tools are allowed (WiFi Marauder
and GhostESP are precedents). Keep the copy framed as "detection / audit /
anti-stalking" rather than "attack X".

## One-time prep (this repo)
- `application.fam` has the required fields: `appid`, `name`, `apptype`,
  `fap_category`, `fap_version` (bump every submission), `fap_icon` (10x10 1-bit),
  `fap_author`, `fap_description`. ✅
- `changelog.md` ✅, `README.md` documents the **ESP32 companion requirement** ✅.
- **Screenshots**: capture a few with qFlipper (unmodified), e.g. the Flock
  detect screen, WiFi audit list, and the BLE "following you" alert. Put them in
  `screenshots/` and reference them below. (Required by the catalog.)
- App must build with `ufbt` against latest Release/RC (CI already does this).

## Submit
1. Fork `github.com/flipperdevices/flipper-application-catalog`.
2. Add `applications/Tools/flipdeflock/manifest.yml` (template below), pinning a
   specific `commit_sha` of this repo.
3. Open a PR using their template. Review is ~1-2 business days.

### manifest.yml template
```yaml
sourcecode:
  type: git
  location:
    origin: https://github.com/ReconGrunt/FlipDeFlock.git
    commit_sha: <PUT A REAL COMMIT SHA HERE>
    subdir: .
short_description: Passive counter-surveillance site survey (Flock/ALPR, WiFi, BLE trackers).
description: "@README.md"
changelog: "@changelog.md"
screenshots:
  - screenshots/flock.png
  - screenshots/wifi.png
  - screenshots/ble.png
```

## Also list on (lower effort)
- **Momentum**: open an issue on `Next-Flip/Momentum-Apps` requesting inclusion
  (maintainers integrate it for you).
- **Unleashed / RogueMaster**: PR to `RogueMaster/flipperzero-firmware-wPlugins`
  and coordinate on their Discord.
- **awesome-flipperzero / awesome-flipper.com**: submit a PR to get listed.
- Announce on r/flipperzero + the Flipper Discord (a demo GIF of the BLE
  "following you" flag travels well).

See the official guide:
https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md
