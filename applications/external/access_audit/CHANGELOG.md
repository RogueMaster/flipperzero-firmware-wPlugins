# Changelog

All notable changes to this project are documented here.

## [1.11.2]: Report shows the on-screen verdict

### Fixed
- **Report now matches the result screen**: each card's likelihood line shows the on-device verdict in brackets when it differs from the OWASP band — e.g. `Likelihood: MINIMAL [SECURE]`, `Likelihood: HIGH [HIGH RISK]`. Previously the screen said `SECURE`/`HIGH RISK` while the report only said `MINIMAL`/`HIGH`, so the rating appeared to be missing from the report. The verdict words now have a single source of truth in `core/scoring.c` so the screen and report can no longer drift

## [1.11.1]: Clarify iCLASS scan mode

### Changed
- **iCLASS scan-mode prompt clarified**: the prompt now reads `Tap iCLASS SE/Legacy...` with a `Seos? use NFC mode` hint below it. iCLASS Legacy/SE are on ISO 15693 (iCLASS mode); Seos is ISO 14443-4A (NFC mode) — the shared "iCLASS" branding made the mode ambiguous

## [1.11.0]: HID Seos detection

### Added
- **HID Seos detection**: ISO14443-4A smart cards are now routed to the `iso14443_4a` poller, which performs an ISO-7816 `SELECT` of the HID Seos applet AID. A `0x9000` response classifies the card as **HID Seos** (previously it fell through to generic `ISO 14443-4A`). Seos is a modern AES secure element, so it scores **SECURE** with advice to disable any legacy/Prox fallback at the reader. Passive identification only — reading the PACS (facility code / card number) still requires a HID SAM (#53). Non-Seos ISO14443-4A cards are unaffected (still classified as `ISO 14443-A`)

## [1.10.2]: Scan-screen spacing fix

### Fixed
- **Scan-screen lines re-spaced (~10px)** so the version line no longer collides with the title divider above or the "Tap card to reader" prompt below. The version, scan prompt, "Scanning…" status, mode hint, and controls row are now evenly spaced

## [1.10.1]: Scoring fixes — MIFARE Plus SL2 and FeliCa Standard

### Fixed
- **MIFARE Plus SL2 now scores MODERATE (was SECURE)**: SL2 is the weak transitional mode (AES authentication but Classic frame structure, downgrade-prone), so it is no longer treated as having protected application memory. Only SL3 (full AES + ISO14443-4) is. This matches the documented behaviour in `docs/card-types.md`
- **FeliCa Standard now scores SECURE (was MODERATE)**: standard FeliCa has proprietary mutual authentication protecting its blocks, so it is treated as having protected memory (like DESFire); FeliCa Lite (no mutual auth) remains HIGH RISK. Matches `docs/card-types.md`
- `ease_of_exploit` for MIFARE Plus SL2 moved from `hard` to `moderate` to match its MODERATE likelihood (downgrade/UID-replay, not a crypto break)
- Corrected the `docs/scoring.md` "Unknown card / scan failure" worked example: a failed/incomplete read scores 20/100 but is labelled **LOW RISK** (both findings are LOW severity), not MODERATE — the label follows the highest finding severity, not the score band

## [1.10.0]: OWASP Risk Rating Methodology framing

### Changed
- **Reports now frame the score as a LIKELIHOOD-of-compromise rating aligned with the [OWASP Risk Rating Methodology](https://owasp.org/www-community/OWASP_Risk_Rating_Methodology)**. Each card entry now shows `Likelihood: <HIGH/MODERATE/LOW/MINIMAL> (N/100, confidence N%)` plus an `Ease of exploit: <trivial/moderate/hard>` factor (OWASP's likelihood factor, derived from the credential technology). The report header carries a methodology note and cites OWASP RRM, and states explicitly that this tool rates **likelihood only** — **impact** (what the credential protects) is assessed in engagement context. The numeric scoring engine is unchanged; only the framing/labels are OWASP-aligned
- Session advisory reworded in likelihood terms ("N credential(s) with HIGH likelihood of compromise") and prompts the operator to assess impact in context

### Added
- `likelihood_label()` and `ease_of_exploit()` helpers in `scoring.c` mapping severity → OWASP likelihood band and credential → OWASP "Ease of Exploit" factor
- Documentation (`docs/scoring.md`, `docs/rules.md`, `docs/card-types.md`, README, catalog description) now explains and cites the OWASP RRM alignment

## [1.9.0]: Expanded MIFARE Classic default key dictionary

### Changed
- **MIFARE Classic default key check now covers 8 well-known public keys** (was 2) (#35): added the NXP MAD key A (`A0A1A2A3A4A5` was already present), the NFC Forum NDEF public key `D3F7D3F7D3F7`, the all-zero `000000000000`, and common vendor defaults `A0B0C0D0E0F0`, `A1B1C1D1E1F1`, `B0B1B2B3B4B5`, `AABBCCDDEEFF`, alongside the factory transport key `FFFFFFFFFFFF`. Each is tried as both key A and key B against sector 0, the loop stops at the first match, and the list is capped under 10 keys so the active auth scan stays fast. Key list documented in `docs/rules.md`

### Added
- **Default-key finding shown on the result screen**: when sector 0 authenticates with a default key, the result screen now shows "! Default key readable" in place of the controls hint, surfacing the active-scan finding on-device instead of only in the saved report

### Fixed
- Corrected the documented default-key scores: a MIFARE Classic 1K with default keys scores **75/100** (legacy 35 + identifier 35 + default_keys 15 − crypto1 10), not 65/100 as previously stated in `docs/rules.md` and `docs/scoring.md`; `docs/card-types.md` was also updated from stale pre-`crypto1_breakable` values (85/70) to the current 75/60

## [1.8.1]: MIFARE Classic scan fix

### Fixed
- **MIFARE Classic 1K/4K/Mini cards now scan correctly**: `MfClassicPollerEventTypeCardDetected` was hitting the catch-all handler and aborting the read before sector 0 was ever requested. Added explicit handling for `CardDetected` and `DataUpdate` events with `NfcCommandContinue`, and guarded the catch-all so it cannot overwrite a successful result

## [1.8.0]: Version displayed on scan screen

### Added
- **Version on home screen**: the app version is now shown in small text below the title divider on the scan (home) screen, so the installed version is always visible at a glance

## [1.7.1]: Report version string fix

### Fixed
- Report header now correctly shows the full version ("Generated by Access Audit v1.7.1") using the same vX.Y.Z format as GitHub releases. Previously stuck at v1.0 since initial release

## [1.7.0]: Lint CI and code quality

### Added
- **clang-format enforcement**: `.clang-format` using the official Flipper Zero firmware style is now committed to the repo; a CI lint job checks all `.c`/`.h` files on every push and PR
- **cppcheck static analysis**: second lint job runs cppcheck with warning/style/performance checks on every push and PR

### Fixed
- Added `const` to event pointer variables in all poller callbacks (`iso_event`, `df_event`, `plus_event`, `fc_event`) - these are read-only
- Added `const` to `line` pointer in `report.c` summary parser
- Narrowed scope of `line[48]` buffer in `access_audit.c` to the branch where it is used
- Reformatted all source files to match the official Flipper Zero clang-format style

## [1.6.0]: MIFARE Classic score adjustment

### Changed
- **MIFARE Classic scores lower than EM4100**: added a new mitigating rule `crypto1_breakable` (-10 points) for MIFARE Classic 1K, 4K, and Mini. Crypto1 is broken but exploiting it requires an active attack (dictionary scan or hardnested/darkside), unlike 125 kHz EM4100 which is a passive serial-number replay with no cryptographic barrier. Scores: Classic 1K is now 60/100 (was 70), Classic 1K with default keys is 65/100 (was 85); EM4100 and MIFARE Plus SL1 remain 70/100

## [1.5.0]: Default MIFARE Classic key detection

### Added
- **Default key detection**: when a MIFARE Classic card is scanned, the app actively attempts to authenticate sector 0 using four common default key combinations (`FFFFFFFFFFFF` and `A0A1A2A3A4A5`, key A and B each). If any succeeds, the new `default_keys` rule fires (+15 score), the card scores 85/100 instead of 70/100, and the report includes an explicit note flagging the finding as an active scan result. No sector data is read or stored; the card is halted immediately after the auth attempt

## [1.4.3]: Duplicate card deduplication

### Fixed
- Tapping the same card multiple times in a session no longer adds duplicate entries; `session_append` now compares incoming UID bytes against all existing entries and skips the tap if a match is found. UID-less cards are always appended as they cannot be deduplicated
- Report write loop now deduplicates by UID before writing each card entry, so reports correctly list unique cards only even if duplicates reached the session buffer on older builds. Card numbering (Card X/Y) uses the unique count as the total

## [1.4.2]: Scoring fix for unconfirmed iCLASS cards

### Fixed
- `CardTypeHidIclass` (unconfirmed TI ISO15693 card, not yet verified via iCLASS scan) was incorrectly included in `rule_legacy_family`, causing it to score HIGH RISK instead of MODERATE. Only confirmed Legacy variants (`CardTypeHidIclassLegacy`, `2k`, `16k`, `32k`) now trigger the legacy rule, matching the documented behaviour in `docs/card-types.md`

## [1.4.1]: Build fix

### Fixed
- `date_str` buffer in report list draw widened to `REPORT_NAME_LEN` to silence `-Werror=format-truncation` on the CI build toolchain (local build passed; CI dev SDK triggered it)

## [1.4.0]: FeliCa sub-type detection, risk summary in report list

### Added
- **FeliCa sub-type detection**: FeliCa Lite is now detected and classified separately from standard FeliCa; Lite has no mutual authentication and scores HIGH RISK with advice to avoid it for access control. Standard FeliCa retains SECURE scoring. Previously all FeliCa cards fell through to an unread state (no poller callback existed)
- **Risk summary in report list**: each row now shows `H:N M:N` (high and medium counts) right-aligned alongside the formatted date/time, so you can identify the most critical reports at a glance without opening them

### Fixed
- FeliCa cards now scan and classify correctly; `callback_for_protocol` previously had no FeliCa case, causing the scanner to restart instead of reading the card

## [1.3.0]: Card count on scan screen, SAK/ATQA in reports, CI version check

### Added
- **Card count badge on scan screen**: `[N]` counter appears top-right once the first card in a session is scanned, matching the result screen badge
- **SAK/ATQA in reports**: ISO14443-3A SAK and ATQA bytes are now captured and written to every report entry as `ATQA: XX XX  SAK: XX`; particularly useful when the card falls through to the generic `ISO14443-A` type
- **Version consistency CI check**: `release.yml` now asserts that `fap_version` in `application.fam` matches the git tag's MAJOR.MINOR before building; mismatches fail the release job immediately

### Fixed
- RFID cards (`identifier_only_pattern`): `metadata_complete` was already set correctly in the RFID provider; combined with the v1.2.0 rule fix, EM4100/HID/Indala cards now correctly score HIGH RISK 70/100

## [1.2.0]: Code cleanup, delete report, full docs

### Added
- **Delete report from viewer**: hold Back in the report viewer to open a delete confirmation screen; OK deletes and returns to the report list, Back cancels
- **docs/scoring.md**: full scoring formula with severity thresholds, confidence penalties, and worked examples for every common card type
- **docs/card-types.md**: reference table for every `CardType` enum value: risk rating, common deployment, and recommended remediation
- **docs/contributing.md**: step-by-step guide for adding a new card type or rule, including provider patterns and code style notes

### Changed
- `rule_identifier_only_pattern`: removed the `repeated_reads_identical` field that was never set by any provider; rule now fires on `uid_present && !user_memory_present && metadata_complete`, which correctly identifies all static-replay credential patterns
- `AccessObservation` struct: removed the dead `repeated_reads_identical` field

### Fixed
- docs/rules.md: updated `identifier_only_pattern` signal conditions to match the implementation; added HID iCLASS Legacy family to `legacy_family` match list; corrected rule interaction examples table (scores now reflect `identifier_only_pattern` firing correctly)

## [1.1.0]: HID iCLASS support and bug fixes

### Added
- **HID iCLASS scanning**: proprietary ACTALL → IDENTIFY → SELECT → READ block 1 exchange over ISO15693 RF; detects and scores iCLASS DES/3DES legacy cards (HIGH RISK) with advice to upgrade to iCLASS SE/Seos
- **iCLASS memory variant classification**: reads configuration block to distinguish 2k, 16k, and 32k variants where block 1 is accessible; falls back to generic "HID iCLASS (Legacy)" gracefully when block is protected
- **Three-way scan mode**: Left/Right on scan screen now cycles NFC → RFID → iCLASS → NFC

### Fixed
- Left arrow now cycles scan mode in reverse; previously both arrows cycled in the same direction
- Consecutive iCLASS rescans now work correctly; a stale-poller slot prevents the poller appearing still-running after it completes
- Report directory (`/ext/apps_data/access_audit`) now created recursively on first save; previously silent failures on SD cards where the parent directory did not yet exist
- Report list now reads all reports before sorting, so the newest reports always appear first; previously capped at 20 before sort, which hid newer files when more than 20 existed
- Post-save confirmation screen now returns to scan mode instead of exiting the app
- Save result correctly captured when saving without a session name via the Back button

## [1.0.0]: First stable release

- Full NFC 13.56 MHz and RFID 125 kHz support with hardware-safe toggling
- Deep card classification: DESFire EV1/EV2/EV3/Light, MIFARE Classic 1K/4K/Mini, Plus SL1/SL2/SL3, NTAG203/213/215/216/I2C, EM4100, HID H10301/Generic, Indala, and more
- Six named audit rules with additive scoring (0-100) and four severity labels
- MIFARE Plus SL1 correctly scored HIGH RISK; DESFire EV2/EV3 correctly scored SECURE
- Named scan sessions via on-screen QWERTY keyboard
- Multi-scan session buffer (up to 20 cards) with live counter
- SD card reports with per-card advice, confidence score, memory capacity, manufacturer, UID byte count, unique card count, mixed-tech flag, and session-level advisory
- On-device report viewer with scrolling
- App icon (10x10px)

## [0.5.0]: Deep card classification and report improvements

- DESFire EV1/EV2/EV3/Light detection via GetVersion command (poller callback)
- MIFARE Plus SL1/SL2/SL3 detection via SDK security level response
- Scoring fix: DESFire EV2/EV3 and Plus SL2/SL3 now correctly score SECURE
- Scoring fix: MIFARE Plus SL1 now scores HIGH RISK (Classic-compatibility mode)
- Per-card `Advice:` line in reports with plain-English recommendation per card type
- Confidence percentage shown alongside risk score in reports
- Session-level `ACTION REQUIRED` / `REVIEW RECOMMENDED` advisory at end of report
- README and docs updated to reflect full card classification depth and scoring behaviour

## [0.4.0]: Named sessions

- Optional session naming on save via on-screen QWERTY keyboard
- Session name written to report header when provided
- Back button on keyboard acts as backspace; saving with an empty name skips the name

## [0.3.0]: RFID support and report improvements

- RFID 125 kHz support: EM4100, HID H10301, HID Generic, Indala, and more
- Left/Right on scan screen toggles between NFC and RFID mode (lazy hardware allocation prevents conflict)
- Radio type (NFC 13.56MHz / RFID 125kHz) written to each card entry in reports
- Session summary stats (High/Medium/Low/Secure counts, most common card type) added to report header

## [0.2.0]: On-device report viewer and card sub-types

- On-device report viewer: browse and scroll saved reports without leaving the app
- Card sub-type detection: MIFARE Classic 1K/4K/Mini (via SAK byte), NTAG213/215/216/I2C (via MfUltralight poller)
- Multi-scan session buffer: scan up to 20 cards per session, session counter on result screen

## [0.1.0]: Initial release

- NFC card scan and classification (MIFARE Classic, DESFire, Plus, Ultralight, NTAG, ISO14443-A/B, ISO15693, FeliCa, SLIX, ST25TB)
- Instant risk score 0-100 with HIGH RISK / MODERATE / LOW RISK / SECURE label
- Six named audit rules: legacy_family, identifier_only, uid_no_memory, modern_crypto, incomplete_evidence, no_uid
- SD card report saving to `/ext/apps_data/access_audit/`
