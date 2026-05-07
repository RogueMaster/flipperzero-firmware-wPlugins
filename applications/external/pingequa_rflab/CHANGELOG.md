# Changelog

All notable changes to PINGEQUA RF Lab.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning: [SemVer](https://semver.org/spec/v2.0.0.html).

## [0.5.1] — 2026-05-06

Patch release — broader firmware compatibility + UI polish.

### Fixed
- **OFW compatibility**: chip arbiter rewritten to single-handle SPI design.
  Previously used `furi_hal_spi_bus_handle_external_extra` which is a
  Momentum-only symbol — CI rejected the FAP on OFW SDK with
  `Symbols not resolved`. Now uses only the standard `external` handle
  and manually toggles PA4/PC3 CS lines via GPIO. SPI timing, command
  framing, and W_TX_PAYLOAD latch behavior all preserved. Two CS lines
  still never simultaneously LOW (verified). Real-device regression
  passed: BLE/WiFi jamming + Scanner + Sub-GHz Read clean exit all work.
- **Jammer view spacing**: `2402/26/80MHz` (freq) and `@080 2480MHz N358`
  (tag) rows had only 1px gap, `@` character visually cramped against
  the freq row baseline. Bumped TAG_BASELINE 48→50 and BTM_DIV_Y 51→52
  for 3px breathing space.
- **Scanner CSV cosmetic**: `# Peak channel: 14 (2414 MHz, 32 hits)`
  comment was being split into two columns by Excel CSV importer (comma
  inside the comment). Replaced with semicolon `; 32 hits)`.

### Changed
- **CI**: `actions/checkout` v4→v5, `actions/setup-python` v5→v6 to run
  on Node.js 24 (Node 20 is deprecated as of GitHub announcement; full
  removal Sep 2026).
- **Internal**: removed stray `module.elf.c` build artifact from repo root.

### Notes for users
- If you installed v0.5.0 on **OFW / Unleashed / RogueMaster** and it
  refused to launch — that's the bug v0.5.1 fixes. Re-install from
  Releases.
- Momentum users: v0.5.0 worked fine; v0.5.1 has the same behavior
  + UI polish.

## [0.5.0] — 2026-05-05

Data export & settings persistence. v0.5.0 adds research-grade CSV export
for both Scanner and Jammer, plus jammer mode/channel persistence so users
don't have to re-pick on every launch.

### Added
- **Scanner CSV export** — long-press OK on Scanner screen exports current
  126-channel hits + metadata (sweep count, dwell, peak channel) to
  `/ext/apps_data/pingequa/scans/scan_<boot_ms>.csv`. Self-describing CSV
  with `# ` comment header — Excel / Python pandas `read_csv` compatible.
  UI feedback: `SAVED!` flash on top-right for ~1 sec.
- **Jammer session log** — auto-written on every Jammer scene exit to
  `/ext/apps_data/pingequa/jammer/session_<start_ms>.csv`. Captures mode,
  CW channel, start/end ms, duration, total chunks, and (for BLE React
  mode) total reactive jam count. Both human-readable header comments and
  `key,value` CSV rows for parsing.
- **Settings persistence** — Jammer mode + CW channel saved to
  `/ext/apps_data/pingequa/jammer.conf` (FlipperFormat) on scene exit,
  loaded on next entry. Defaults to `CW Custom @ ch 42` if file missing or
  corrupt.

### Added — Engineering
- New core modules:
  - `core/pq_jammer_config.{h,c}` — FlipperFormat read/write for jammer state
  - `core/pq_jammer_log.{h,c}` — CSV session log writer
  - `core/pq_scan_export.{h,c}` — Scanner CSV exporter
- New scanner_view setter: `scanner_view_show_export_flash` — top-bar
  countdown indicator (decays via `update_sweep` calls).

### Changed
- `scanner_scene` long-press OK now triggers CSV export (was no-op).
- `jammer_scene_on_enter` loads previous mode/channel via
  `pq_jammer_config_load` (was hardcoded to CW Custom @ 42).
- `jammer_scene_on_exit` auto-writes session log + saves config.

### Compliance (spec v1.4)
- Storage IO via official `Storage` + `FlipperFormat` API (M14 / R1) ✓
- All file paths under `/ext/apps_data/pingequa/` (§8.2 数据目录) ✓
- Scanner export runs synchronously in input_callback (~50 ms IO) but
  acceptable since one-shot user action; not in worker hot path.
- No new spec violations vs v0.4.0 audit.

### File paths created
```
/ext/apps_data/pingequa/
├── jammer.conf                          ← Settings (auto-managed)
├── scans/
│   └── scan_<boot_ms>.csv              ← Scanner long-OK export
└── jammer/
    └── session_<start_ms>.csv          ← Jammer auto session log
```

## [0.4.0] — 2026-05-05

Effective jamming. After hardware-validated +20 dBm output (per Ashining
A01-S2G4A20S1a datasheet, integrated nRF24L01P + PA), v0.4.0 expands the
jammer to 7 modes including the first known **RPD reactive BLE jamming on
Flipper Zero NRF24** and **WiFi pilot-aware OFDM** disruption.

Real-device verification: BLE devices and 2.4 GHz WiFi can be disconnected
within typical room range.

### Added — 7 jammer modes (was 2)
- **CW Custom** — single user-selected channel (kept from v0.3.0)
- **BLE Adv** — blind CW hop {ch 37, 38, 39} (kept from v0.3.0)
- **BLE React** ★ NEW — RPD reactive jamming. NRF24 enters RX, polls RPD
  register every 50 µs (-64 dBm threshold, 40 µs latency per Nordic
  datasheet §6.4); on detection, switches to CW for sustained 2.5 ms burst
  on the active BLE adv channel, then resumes listening. Catches the next
  20–100 ms BLE adv interval on the same channel for high-probability hit.
  Concept from Brauer et al. IEEE 7785169 (2016) "Practical Selective
  Jamming of BLE Advertising"; first implementation on Flipper Zero NRF24
  platform.
- **WiFi 1 / 6 / 11** — pilot-aware OFDM jamming. Targets the 4 OFDM pilot
  subcarrier positions per WiFi channel (±2.2 MHz and ±6.6 MHz from center,
  per 802.11g spec) instead of barrage sweep. Sustained CW per pilot for
  25 ms / chunk, hops between 4 pilot frequencies. +7.5 dB equivalent
  jamming efficiency over barrage per Clancy 2011 (IEEE 5962467).
- **ALL 2.4G** — full-band CW sweep 0–125 (kept from v0.3.0)

### Changed
- **CSN command latching fix** (root cause of v0.3.0 weak jamming): every
  `pq_chip_spi_trx` now ends with CSN HIGH per huuck and Nordic datasheet
  §8 spec. Previously each command's CSN HIGH only happened on next call's
  start, leaving the last command (typically W_TX_PAYLOAD before CE HIGH)
  uncommitted when the chip transitioned to TX. Affects both NRF24 and
  CC1101 paths; Channel Scanner stability also improves.
- **CW setup** now follows huuck/RF24-lib recipe — pre-loads 32-byte 0xFF
  W_TX_PAYLOAD into TX FIFO (required for nRF24L01+ CW emission, not in
  datasheet §6.4 but empirically necessary).
- **Tstby2a 130 µs delay** added after CE HIGH (datasheet §6.1.7 Table 16).
- **CC1101 SLEEP** (SPWD strobe) on jammer scene entry — deeper than IDLE,
  crystal off, minimal cross-talk to NRF24 antenna 1 cm away.
- **Diagnostic register readback** — start_cb logs CFG/RF_SETUP/RF_CH/STATUS/
  FIFO/EN_AA via `FURI_LOG_I` for SPI verification (use `log debug` in CLI).
- **Reactive jam_count log** — only on detection, format `react ch=X jams=N`.
- FAP size: 28 KB → 33.6 KB (+20% for 7 modes + 3 engines).

### Added — Engineering
- New SPI engine: `JamEngineReactive` (was Cw + PayloadSpam)
- New NRF24 helpers: `setup_reactive` / `react_to_tx` / `react_to_rx` /
  `load_tx_payload_noack` / `flush_tx` / `diag_log` /
  `setup_payload_spam`
- New CC1101 helper: `pq_cc1101_sleep` (SPWD)
- Profile table in jammer_scene.c — per-mode engine + channel list +
  dwell/chunk parameters

### Compliance (spec v1.4 §14 audit)
- No `furi_hal_spi_acquire/release` outside arbiter (M3/M18) ✓
- No `furi_hal_gpio_*` on PB2/PA4/PC3 outside arbiter (M15) ✓
- No `FuriWaitForever` in input/GUI path (M9) ✓
- No self-managed ViewPort state machine (M7) ✓
- All callback durations within §11.2 budgets (≤ 50 ms ceiling, reactive
  loop time-bounded to 30 ms via `furi_get_tick`)
- All `pq_chip_nrf24_ce_set` calls inside `pq_chip_with_nrf24` callback (§17.2 22) ✓

### Known limitations
- WiFi jamming requires close range (< 30 cm typical) due to 2.4 GHz
  WiFi router being similar TX power (+18~20 dBm); hardware ceiling.
- BLE protocol-aware (decode access address before jamming) deferred
  to v0.5.x — would let us skip false RPD triggers from microwave/
  non-BLE 2.4 GHz noise.
- Multi-waveform mixing (JamShield 2025 ML defense resistance) deferred
  to v0.5.x.

## [0.3.0] — 2026-05-04

NRF24 Jammer feature added.

### Added
- **NRF24 Jammer mode** — two transmit modes:
  - **CW** — constant wave at user-selected channel (0–125), ←/→ navigation with ±1 / ±5 step
  - **Sweep** — full 2.4 GHz band hop across all 126 channels
- **Main menu** — choose between Scanner and Jammer after the splash screen
- Fixed maximum transmit power for jamming effectiveness

### Changed
- After splash, the app now lands on a main menu (previously went directly to scanner)
- Pressing Back in the scanner returns to the main menu (previously exited the app)
- FAP size: 21 KB → 28 KB

### Known limitations
- WiFi flood mode (payload-based interference) — planned for v1.2.x
- Custom channel list from text file — planned for v1.2.x
- Adjustable transmit power UI — planned for v0.4.0

## [0.2.0] — 2026-05-04

First public release. Channel scanner.

### Added
- 126-channel real-time NRF24 spectrum scanner
- WiFi 1/6/11 and BLE 37/38/39 frequency markers
- Adjustable dwell time (130–2000 µs, ±10 µs / ±50 µs step)
- Cursor inspection with live frequency and hit-count readout
- Auto max-hold mode — freezes display on saturation for analysis
- Hardware auto-detect at startup with diagnostic error screen
- Clean exit — restores GPIO and SPI state so other Flipper apps work normally afterwards

### Known limitations
- Officially supports only the PINGEQUA 2-in-1 RF Devboard
- No scan log persistence (display only)
- No transmit features (added in v0.3.0)

## [Unreleased]

Planned for v0.5.x:
- BLE protocol-aware jamming — decode access address (0x8E89BED6 for adv) via
  promiscuous PDU header capture, react only on confirmed BLE packets
  (eliminates false RPD triggers from microwave / WiFi noise)
- Multi-waveform mixing (CW + payload spam + random EVM) per JamShield 2025
  ML defense evasion (arXiv 2507.11483)
- Random hop / jitter dwell (defeat adaptive frequency hopping in WiFi/BT)
- Jam intensity indicator on Jammer view (per-second jam count)
- Settings persistence (last mode/channel) via FlipperFormat at
  `/ext/apps_data/pingequa/jammer.conf`

Planned for v0.6+ (Phase 4 deferred):
- Scan log export to `/ext/apps_data/pingequa/scans/` (CSV)
- Quick-jump cursor (long-press OK → peak channel)
- Multi-language UI (zh / en toggle)
- About scene (product page link, license, version info)

See [README §Roadmap](README.md#roadmap) for the full pipeline.
