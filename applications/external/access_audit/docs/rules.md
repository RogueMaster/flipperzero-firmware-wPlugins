# Audit Rules

Each rule maps to a named finding in saved reports. Rules are additive; multiple rules can fire on the same observation. The final score is clamped to 0-100.

These rules are **likelihood signals** under the [OWASP Risk Rating Methodology](https://owasp.org/www-community/OWASP_Risk_Rating_Methodology) (`Risk = Likelihood × Impact`): they rate how readily a credential can be compromised (its OWASP *Ease of Exploit*, *Awareness*, and *Detectability* factors), not the impact of a compromise. Impact — what the credential protects — is assessed by the operator in engagement context. See [scoring.md](scoring.md#methodology--owasp-risk-rating-alignment).

---

## High-risk rules

### `legacy_family`

Triggers for credential families that lack effective cryptographic protection.

Matches:
- All 125 kHz RFID protocols (EM4100-like, HID Prox, HID Generic, Indala, generic 125 kHz), no crypto at all
- MIFARE Classic (1K, 4K, Mini), Crypto1 cipher is practically broken
- MIFARE Plus SL1, Classic-compatibility mode; AES is not active
- HID iCLASS Legacy (all memory variants: 2k, 16k, 32k), DES/3DES, master key publicly known

**Score contribution:** +35 · **Max severity:** HIGH

---

### `identifier_only_pattern`

Triggers when a UID is present with no evidence of protected application memory, and classification metadata is complete. The card's security relies on the UID alone; it can be cloned without breaking any crypto.

Signals:
- `uid_present` = true
- `user_memory_present` = false
- `metadata_complete` = true

Does not fire when `metadata_complete` is false; use `uid_no_memory` for incomplete observations.

**Score contribution:** +35 · **Max severity:** HIGH

---

## Medium-risk rules

### `uid_no_memory`

Weaker form of `identifier_only_pattern`. A UID is present but no evidence of authenticated/protected memory has been observed, and metadata is incomplete so a confident conclusion cannot be drawn. Does not fire when `identifier_only_pattern` already applies.

Signals:
- `uid_present` = true
- `user_memory_present` = false
- `metadata_complete` = false (or `identifier_only_pattern` did not fire for another reason)

**Score contribution:** +20 · **Max severity:** MEDIUM

---

## Low-risk / confidence rules

### `incomplete_evidence`

Classification metadata is incomplete; the poller could not read all expected fields. The result should be treated with caution.

**Score contribution:** +10 · **Confidence penalty:** -20%

---

### `no_uid`

UID could not be extracted. The observation cannot be fully assessed.

**Score contribution:** +10 · **Confidence penalty:** -15%

---

## Active scan findings

### `default_keys`

Fires when sector 0 on a MIFARE Classic card was authenticated using a well-known public key. Indicates the card keys have never been changed from a publicly documented default.

Keys checked (key A and key B for each):
- `FFFFFFFFFFFF` - factory transport default for all sectors on a new card
- `A0A1A2A3A4A5` - MAD key A (NXP AN10787 application directory)
- `D3F7D3F7D3F7` - NFC Forum NDEF public key
- `000000000000` - blanked / all-zero key
- `A0B0C0D0E0F0` - common vendor default
- `A1B1C1D1E1F1` - common vendor default
- `B0B1B2B3B4B5` - common vendor default
- `AABBCCDDEEFF` - common vendor default

The list is intentionally capped (under 10 keys) so the active auth loop completes quickly. The check stops at the first key that authenticates. This rule requires an active authentication attempt. A note is written to the report indicating the finding came from an active scan.

**Score contribution:** +15 · **Max severity:** HIGH

---

## Mitigating rules

### `crypto1_breakable`

MIFARE Classic card. Crypto1 is cryptographically broken, but exploiting it still requires an active attack (dictionary scan or hardnested / darkside attack) against a reader. This is meaningfully more effort than cloning a 125 kHz EM4100 card, which is a passive serial-number replay with no cryptographic barrier at all. Applies a small score reduction relative to 125 kHz RFID cards.

Matches:
- MIFARE Classic 1K, 4K, Mini

**Score contribution:** -10

Does not apply to MIFARE Plus SL1 (Classic-compatible mode, same threat model as EM4100 for relay purposes once the SL1 downgrade is exploited).

---

### `modern_crypto`

Card family uses modern cryptography. Reduces the effective risk score and lowers the severity label.

Matches:
- MIFARE DESFire (all variants: EV1, EV2, EV3, Light), DES/3DES or AES
- MIFARE Plus SL2, AES crypto active (Classic-format frames)
- MIFARE Plus SL3, AES crypto + ISO14443-4 protocol
- FeliCa Standard, proprietary crypto (FeliCa Lite is excluded, no mutual auth)

**Score contribution:** -20

**Severity downgrade** (applies only when `legacy_family` did not also fire):
- HIGH → MEDIUM
- MEDIUM → SECURE when the resulting score reaches zero

---

## Scoring thresholds

| Label | Score range | Typical cause |
|---|---|---|
| HIGH RISK | 35-100 | `legacy_family` or `identifier_only_pattern` fired |
| MODERATE | 20-34 | `uid_no_memory` fired without modern crypto mitigation |
| LOW RISK | 10-19 | `incomplete_evidence` or `no_uid` only |
| SECURE | 0-9 | Modern crypto family with no other risk factors |

---

## Rule interaction examples

| Card | Rules fired | Score | Label |
|---|---|---|---|
| EM4100 | legacy_family, identifier_only_pattern | 70 | HIGH RISK |
| MIFARE Classic 1K | legacy_family, identifier_only_pattern, crypto1_breakable | 60 | HIGH RISK |
| MIFARE Classic 1K (default keys) | legacy_family, identifier_only_pattern, default_keys, crypto1_breakable | 75 | HIGH RISK |
| MIFARE Plus SL1 | legacy_family, identifier_only_pattern | 70 | HIGH RISK |
| HID iCLASS Legacy 2k | legacy_family, identifier_only_pattern | 70 | HIGH RISK |
| MIFARE Plus SL2 | identifier_only_pattern, modern_crypto | 15 | MODERATE |
| MIFARE Plus SL3 | modern_crypto | 0 | SECURE |
| DESFire EV2/EV3 | modern_crypto | 0 | SECURE |
| DESFire EV1 | modern_crypto | 0 | SECURE |
| NTAG213 | identifier_only_pattern | 35 | HIGH RISK |
| Unknown card | incomplete_evidence, no_uid | 20 | MODERATE |

> **Note on the examples table:** scores shown assume `metadata_complete=true`. If the poller returned incomplete metadata, `incomplete_evidence` fires (+10, -20% confidence) and `identifier_only_pattern` is replaced by `uid_no_memory` (+20) since `metadata_complete=false`.

---

## Implementation reference

- Rule functions: [core/rules.c](../core/rules.c)
- Score calculator: [core/scoring.c](../core/scoring.c)
- Scoring details and further examples: [docs/scoring.md](scoring.md)
