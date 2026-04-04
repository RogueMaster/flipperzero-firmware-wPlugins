# Scoring

The audit score is a single integer in 0-100 that summarises the risk exposure of a scanned credential. It is built from additive rule contributions minus any mitigations, then clamped to 0-100.

---

## Score formula

```
score = sum(risk_rule_points) - sum(mitigation_points)
score = clamp(score, 0, 100)
```

Each rule fires independently. Multiple rules can fire on the same observation.

### Risk rule points

| Rule | Points | Severity |
|---|---|---|
| `legacy_family` | +35 | HIGH |
| `identifier_only_pattern` | +35 | HIGH |
| `default_keys` | +15 | HIGH |
| `uid_no_memory` | +20 | MEDIUM |
| `incomplete_evidence` | +10 | LOW |
| `no_uid` | +10 | LOW |

### Mitigation points

| Rule | Points | Effect |
|---|---|---|
| `modern_crypto` | -20 | Reduces score; lowers severity label (see below) |

### Confidence penalties

| Rule | Confidence reduction |
|---|---|
| `incomplete_evidence` | -20% |
| `no_uid` | -15% |

Confidence starts at 90% and cannot go below 0%.

### Severity label thresholds

| Label | Score range |
|---|---|
| HIGH RISK | 35-100 |
| MODERATE | 20-34 |
| LOW RISK | 10-19 |
| SECURE | 0-9 |

### `modern_crypto` severity downgrade

When `modern_crypto` fires, the severity label is adjusted after the score is calculated:

- If `legacy_family` did **not** fire:
  - HIGH → MEDIUM (score reduced but card is not a legacy protocol)
  - MEDIUM → SECURE (when the resulting score reaches zero)

If both `modern_crypto` and `legacy_family` fire simultaneously (which cannot happen with the current card set; no card is both legacy and modern), the severity is not downgraded.

---

## Worked examples

### EM4100 (125 kHz)

| Step | Value |
|---|---|
| `legacy_family` fires | +35 |
| `identifier_only_pattern` fires (`metadata_complete=true`, `uid_present=true`, no memory) | +35 |
| Score | 70 |
| Max severity | HIGH |
| Confidence | 90% |

**Result: HIGH RISK · 70/100**

---

### MIFARE Classic 1K

| Step | Value |
|---|---|
| `legacy_family` fires | +35 |
| `identifier_only_pattern` fires | +35 |
| Score | 70 |
| Max severity | HIGH |

**Result: HIGH RISK · 70/100**

> If sector 0 authenticates with a default key, `default_keys` also fires (+15), raising the score to 85/100.

---

### MIFARE Plus SL1

| Step | Value |
|---|---|
| `legacy_family` fires (SL1 = Classic-compat) | +35 |
| `identifier_only_pattern` fires | +35 |
| Score | 70 |
| Max severity | HIGH |

**Result: HIGH RISK · 70/100**

---

### HID iCLASS Legacy (any variant)

| Step | Value |
|---|---|
| `legacy_family` fires (DES/3DES) | +35 |
| `identifier_only_pattern` fires | +35 |
| Score | 70 |
| Max severity | HIGH |

**Result: HIGH RISK · 70/100**

---

### NTAG213

| Step | Value |
|---|---|
| `identifier_only_pattern` fires (`uid_present=true`, `user_memory_present=false`, `metadata_complete=true`) | +35 |
| `uid_no_memory` does **not** fire; its guard prevents it when `identifier_only_pattern` already applies | 0 |
| Score | 35 |
| Max severity | HIGH |

**Result: HIGH RISK · 35/100**

> If the poller returned incomplete metadata (`metadata_complete=false`), `identifier_only_pattern` does not fire. Instead: `uid_no_memory` (+20) + `incomplete_evidence` (+10, -20% confidence) = 30, MODERATE · 70% confidence.

---

### MIFARE DESFire EV2 / EV3

| Step | Value |
|---|---|
| `modern_crypto` fires | -20 |
| No other rules fire | 0 |
| Score (clamped at 0) | 0 |
| Severity before mitigation | INFO |
| Severity after mitigation (MEDIUM→SECURE at score=0) | SECURE |

**Result: SECURE · 0/100**

---

### MIFARE DESFire EV1

| Step | Value |
|---|---|
| `modern_crypto` fires (EV1 uses 3DES, counts as modern) | -20 |
| Score | 0 |
| Max severity | SECURE |

**Result: SECURE · 0/100**

> EV1 uses 3DES rather than AES. The report advice flags this: "EV1 uses 3DES. Upgrade to EV2/EV3 for AES crypto." The score still reaches SECURE because 3DES is not broken the way Crypto1 is, but the advice recommends an upgrade.

---

### MIFARE Plus SL2

| Step | Value |
|---|---|
| `modern_crypto` fires (AES active) | -20 |
| `uid_no_memory` would fire but `identifier_only_pattern` fires first if `metadata_complete=true` | +35 |
| Score | 15 |
| Severity | HIGH → MEDIUM (modern_crypto downgrade, no legacy rule) |

**Result: MODERATE · 15/100**

> SL2 has AES but uses Classic frame structure. The report advice recommends upgrading to SL3.

---

### Unknown card / scan failure

| Step | Value |
|---|---|
| `incomplete_evidence` fires | +10, -20% confidence |
| `no_uid` fires | +10, -15% confidence |
| Score | 20 |
| Max severity | MEDIUM |
| Confidence | 55% |

**Result: MODERATE · 20/100 · 55% confidence**

---

## Implementation reference

- Rule definitions: [core/rules.c](../core/rules.c)
- Score calculator: [core/scoring.c](../core/scoring.c)
- Rule documentation: [docs/rules.md](rules.md)
