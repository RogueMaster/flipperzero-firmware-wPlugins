# Security Policy

FlipDeFlock is a security tool, so it should be held to the standard it audits against.

## Reporting a vulnerability

**Please do not open a public issue for a security problem.**

Use GitHub's private vulnerability reporting:
**[Security → Report a vulnerability](https://github.com/ReconGrunt/FlipDeFlock/security/advisories/new)**. That opens a
private advisory visible only to you and the maintainer.

Expect an acknowledgement within a week. This is a single-maintainer project, so please
allow reasonable time for a fix before public disclosure — 90 days is the default, less
if an issue is being actively exploited, more if we agree a fix needs longer.

## Supported versions

Only the **latest release** receives fixes. There are no long-term support branches.

## In scope

- **The Flipper app (`.fap`)** — memory safety, crashes, and anything that lets untrusted
  radio input or SD-card content compromise the device.
- **`signatures.json` parsing** (`helpers/sig_db.c`) — this reads a user-supplied file
  from the SD card and is the app's primary untrusted-input surface. Findings here are
  especially welcome.
- **Report writers** (`helpers/recon_report.c`, `helpers/report_escape.c`) — a hostile
  SSID or BLE device name must not be able to break out of a CSV column, inject KML
  elements, or otherwise corrupt a report.
- **The ESP32 companion firmware** (`esp32_companion/`) and the UART wire protocol.
- **The in-app flasher plugin** (`plugins/flasher/`).

## Out of scope

- **Missed detections and false positives.** These are correctness bugs, not
  vulnerabilities — please open a normal issue. Detections are indicators, not proof,
  by design.
- **The absence of network features.** FlipDeFlock never transmits and never connects to
  a network. That is intentional and is a core principle, not an oversight.
- **Physical access to an unlocked Flipper.**
- **Forks, clones, and repackaged builds we do not control.** See below.

## Verifying what you run

Official binaries come only from this repository's
[Releases](https://github.com/ReconGrunt/FlipDeFlock/releases) or from
per-push CI artifacts under the **Actions** tab. A tagged release currently contains:

| Artifact | What it is |
|---|---|
| `flipdeflock.fap` | Official-firmware Flipper app |
| `flipdeflock-momentum.fap` | Momentum Flipper app |
| `flipdeflock-unleashed.fap` | Unleashed Flipper app |
| `deflock.fap` | RogueMaster Flipper app |
| `flipdeflock_companion_esp32wroom.bin` | Classic ESP32 companion firmware |
| `flipdeflock_companion_esp32s2.bin` | ESP32-S2 Wi-Fi-only companion firmware |
| `flipdeflock_companion_esp32c5_EXPERIMENTAL.bin` | Experimental ESP32-C5 companion firmware |
| `flipdeflock_asset_pack.zip` | Optional Flipper desktop-animation asset pack |
| `SHA256SUMS.txt` | Checksums for every release asset |

**Anything else distributed under this project's name is not from this project.** In
particular, FlipDeFlock ships no Windows `.exe`, `.dll`, or `.bat`, and no executable
archive. The only official ZIP is the named animation asset pack above. A download
link pointing at an archive inside a source directory rather than at a GitHub Release
is a strong sign you are not on the official repository.

If you find a third-party repository distributing something as "FlipDeFlock", please
report it to GitHub and let us know. See the name reservation in
[README.md](../README.md#license).
