# Changelog

All notable changes to **wM-Buster** are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.1] — 2026-05-01

### Fixed
- **Internal CC1101 receiving nothing in 1.1.0.** Routing the on-board
  radio through the `subghz_devices_*` plugin API left the chip in a
  state where the FIFO never accumulated bytes, so no telegrams were
  decoded. Reverted the internal path to direct `furi_hal_subghz_*`
  calls (the same code that worked in 1.0). The plugin abstraction is
  retained for the external module, which it was actually designed for.

### Changed
- Trimmed README, GitHub Pages, and source comments. The external CC1101
  toggle is documented as a one-liner; standard practice in any modern
  Sub-GHz app and not worth a feature pitch.

## [1.1.0] — 2026-04-30

### Added
- **External CC1101 module support.** `Settings → Module → External` routes
  the radio through the `cc1101_ext` plugin on the GPIO header, with auto-
  fallback to the on-board chip when no external module is detected. Lifted
  from ProtoPirate's `radio_device_loader` pattern. OTG power is enabled
  only while scanning.
- **New manufacturer drivers**:
  - `bmeters/hydrodigit` — BMeters Hydrodigit water (firmware 0x13)
  - `engelmann/hydroclima` — Engelmann Hydroclima room-temperature HCA
  - `sontex/rfmtx1` — Sontex RFM-TX1 water (legacy XOR + new OMS firmwares)
  - `zenner/zenner0b` — Zenner B.One water (1/256000 m³ resolution)
  - `apator/apator_na1` — Apator NA-1 water
  - `techem/mkradio3a` — Techem MK-Radio 3a water (compact frame v0x70)
  - `misc/gwfwater` — GWF water (CI 0x8C ELL frames)
  - `misc/bfw240radio` — BFW 240-Radio HCA
- **Driver engine refactor.** Introduced `drivers/engine/` with a uniform
  `WmbusDriver` interface (`mvt`, `decode_ex`, optional legacy `decode`) and
  a static registry. New drivers drop into `drivers/europe/<vendor>/` and
  ship as one C file each — see `drivers/CONTRIBUTING.md`.
- **Host-side regression tests.** `tests/test_izar` and `tests/test_ports`
  exercise every new driver against the wmbusmeters reference telegrams.
  Wired into `make -C tests check` and the GitHub Actions workflow.

### Fixed
- **OMS short/long header was being walked as DIF/VIF.** `oms_split_emit`
  now skips the post-CI header (4 bytes for `CI=0x7A/0x5A`, 12 bytes for
  `CI=0x72/0x53/0x8B`) before invoking the application-layer walker. This
  removes a class of phantom records (e.g. an "ACC" byte being decoded as
  a 4-byte volume) that affected every unencrypted CI=7A/72 frame.
- **`rfmtx1` index off-by-one.** The driver was treating `apdu[0]` as the
  CI byte instead of post-CI, so the legacy XOR de-obfuscation reached past
  the volume field and the BCD datetime decoded as garbage. Re-anchored
  every offset against `frame[11] → apdu[0]`.
- Added regression tests for both bugs above.

### Changed
- Worker thread now talks to the radio through the `subghz_devices_*` API
  instead of `furi_hal_subghz_*` directly. The chip-FIFO drain still uses
  raw SPI, but now selects the right bus handle (`furi_hal_spi_bus_handle_subghz`
  for internal, `_external` for external) based on the active device.
- **License changed from MIT to GNU GPL v3.0 or later.** Aligns with the
  upstream reference implementations (`wmbusmeters`, `rtl_433`) we
  cross-checked against and ensures any redistributed derivative ships
  its complete corresponding source. Re-using earlier 1.0.x sources is
  still possible under MIT; the GPLv3 terms apply from 1.1.0 onwards.

## [1.0.0] — 2026-04-29

### Added
- Initial public release.
- Modes T1 / C1 / T+C / S1 on the 868 MHz band (CC1101 only).
- EN 13757-3 application-layer walker covering Energy, Volume, Mass,
  Power, Volume flow, Mass flow, temperatures, pressure, on-time / operating
  time, dates, HCA units, fabrication number, bus address.
- Manufacturer drivers: Techem (HCA / heat / water / smoke), Kamstrup,
  Diehl / Hydrometer / Sappel (incl. IZAR), Qundis / ista / Brunata.
- AES-128 mode 5 decryption with `keys.csv` on SD card.
- SD logging of raw + parsed telegrams for offline analysis.
- Custom canvas views for the meter list and per-meter detail page.

[Unreleased]: https://github.com/i12bp8/wmbuster/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/i12bp8/wmbuster/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/i12bp8/wmbuster/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/i12bp8/wmbuster/releases/tag/v1.0.0
