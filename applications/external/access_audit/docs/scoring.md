# Scoring

The score is a single integer in 0-100 that rates the **likelihood that a scanned credential can be compromised** — how easily its technology can be cloned or its secret recovered. It is built from additive rule contributions minus any mitigations, then clamped to 0-100.

## Methodology — OWASP Risk Rating alignment

This score is framed against the [OWASP Risk Rating Methodology](https://owasp.org/www-community/OWASP_Risk_Rating_Methodology), where `Risk = Likelihood × Impact`.

**Reference:** <https://owasp.org/www-community/OWASP_Risk_Rating_Methodology> (also cited in the header of every generated report).

- **The tool rates LIKELIHOOD only.** Every rule below is a likelihood signal — the credential lacks crypto, uses a broken/known cipher, exposes only a static identifier, or was left on default keys. The rules encode OWASP likelihood factors such as **Ease of Exploit** (passive replay vs. active attack vs. infeasible), **Awareness** (publicly documented breaks), and **Detectability** (cloning leaves no trace).
- **IMPACT is out of scope for the tool** — it depends on what the credential protects (a turnstile vs. a vault), which only the operator knows in engagement context. Final risk for a report should combine this likelihood with the operator's impact judgement.

Reports surface this as a `Likelihood: <HIGH/MODERATE/LOW/MINIMAL>` band plus an `Ease of exploit: <trivial/moderate/hard>` factor per card, and cite OWASP RRM in the header. The likelihood band maps directly from severity: HIGH→HIGH, MEDIUM→MODERATE, LOW→LOW, Info→MINIMAL.

### Ease of exploit factor

| Factor | Meaning | Credentials |
|---|---|---|
| **trivial** | no crypto barrier — clone the identifier directly, or default keys/password read | 125 kHz (EM4100/HID/Indala), NTAG/Ultralight, any card read with a default credential |
| **moderate** | broken or publicly-known crypto — active attack or known-key tooling | MIFARE Classic, Plus SL1, Plus SL2, Ultralight C, HID iCLASS Legacy, FeliCa Lite |
| **hard** | modern cryptography with no public break | DESFire EV1/EV2/EV3/Light, Plus SL3, FeliCa Standard |
| **indeterminate** | type not confirmed | unknown / generic ISO14443/15693 |

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
| `crypto1_breakable` | -10 | Small reduction for MIFARE Classic; cracking Crypto1 requires active attack unlike passive 125 kHz replay |

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
| `crypto1_breakable` fires (Crypto1 requires active cracking, not passive replay) | -10 |
| Score | 60 |
| Max severity | HIGH |

**Result: HIGH RISK · 60/100**

> If sector 0 authenticates with a default key, `default_keys` also fires (+15), raising the score to 75/100.

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
| Max severity | LOW |
| Confidence | 55% |

**Result: LOW RISK · 20/100 · 55% confidence**

> Note the **label follows the highest-severity finding (`max_severity`), not the score band**. Here both findings are LOW severity, so the label is LOW RISK even though the score (20) sits in the MODERATE band — a failed/incomplete read shouldn't read as a moderate-risk *credential*. The score-range column in the threshold tables is a typical-case guide; the authoritative label is the dominant finding's severity. (A card whose `card_type` is genuinely `Unknown` short-circuits to 0/100 at 0% confidence.)

---

## Implementation reference

- Rule definitions: [core/rules.c](../core/rules.c)
- Score calculator: [core/scoring.c](../core/scoring.c)
- Rule documentation: [docs/rules.md](rules.md)
