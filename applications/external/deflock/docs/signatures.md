# Updatable signatures (`signatures.json`)

FlipDeFlock ships a trusted, compiled-in detection database (OUIs, SSID patterns,
and — empty by default — probe IE fingerprints). You can **add** to it in the field
without rebuilding by dropping a JSON file on the Flipper's SD card at:

```
apps_data/flipdeflock/signatures.json
```

It's **load-only** (read once at app start, never written, never networked) and
**fail-safe**: if the file is missing, empty, malformed, or oversized, the app
silently falls back to the built-ins — a bad file can't break detection.

Two files ship in this folder, and they are not the same thing:

| File | What it is |
|------|------------|
| [`signatures.example.json`](signatures.example.json) | A **placeholder template**. Its values (`aa:bb:cc`, `deadbeef`, …) match nothing real — copy it and replace them with your own captures. |
| [`signatures.seed.json`](signatures.seed.json) | **Real but unverified** candidate prefixes, tracked upstream and not yet corroborated in the field. See [Seed signatures](#seed-signatures) below before you use it. |

## Schema

All keys are optional; unknown keys are ignored. Every value is an array of strings.

```json
{
  "ouis":           ["aa:bb:cc"],
  "ssid_confirmed": ["example-confirmed-ssid"],
  "ssid_likely":    ["example-likely-ssid"],
  "ie_fps":         ["deadbeef"]
}
```

| Key | Meaning | Scores at most | Cap |
|-----|---------|----------------|-----|
| `ouis` | MAC OUI prefix `aa:bb:cc` (case-insensitive) | **Possible** (OUI-only is weak) | 64 |
| `ssid_confirmed` | SSID substring that all but names a Flock unit | **Confirmed** | 32 |
| `ssid_likely` | Weaker SSID substring | **Likely** | 32 |
| `ie_fps` | 8-hex probe **IE fingerprint** (see below) | **"Class?"** | 32 |

> ### ⚠ `ssid_confirmed` needles are unanchored substrings
>
> This is the one place a user file can cause a **false CONFIRMED**, so read it
> before you add anything here.
>
> Your needle matches **anywhere inside** an SSID. It is *not* the same rule as
> the built-in `Flock-` pattern, which is anchored to the whole SSID (`Flock-` +
> exactly 6 hex — see [Built-in SSID patterns](#built-in-ssid-patterns) for why).
>
> So putting **`flock-` in `ssid_confirmed` confirms `Flock-Guest`,
> `Flock-Freight-WiFi` and every other benign network whose name happens to
> contain it.** That exact over-claim shipped in v0.46 and is the reason the
> built-in rule was anchored. A user file can put it back.
>
> The re-derivation guard in the app does **not** save you here: it re-checks a
> claimed CONFIRMED against this same matcher, which is now consulting *your*
> needle, so it agrees. Nothing downstream will second-guess it.
>
> **Use `ssid_confirmed` only for a string that cannot plausibly occur in a
> non-Flock network name.** If there is any doubt, put it in `ssid_likely`
> instead — that is what that key is for, and a Likely you can weigh beats a
> Confirmed you cannot trust.

Matches are **additive** — your entries can only *add* detections, never remove or
weaken a built-in. Because user signatures are **unverified**, they are deliberately
capped: an OUI never scores above *Possible*, and an IE fingerprint never above the
candidate *"Class?"* rung — **never *Confirmed*, even alongside a Flock OUI**. This
is the precision-over-recall rule: a false "Flock" is worse than a missed one.

## Built-in SSID patterns

These ship compiled in; you don't need to add them. Listed so you can see what a
user pattern would be *adding to*.

| Pattern | Match rule | Scores |
|---------|-----------|--------|
| `Flock-` + exactly 6 hex digits | anchored, whole SSID (`Flock-A1B2C3`) | **Confirmed** |
| `test_flck` | case-insensitive substring | **Confirmed** |
| `flock` | case-insensitive substring | **Likely** |
| `flck` | case-insensitive substring | **Likely** |

`test_flck` is the hard-coded development SSID reported as **CVE-2025-59409**, so its
presence on the air is close to self-identifying. The `Flock-` rule is *anchored* on
purpose: an unanchored substring wrongly confirmed benign names like `Flock-Guest` and
the Flock Freight corporate SSIDs, which now fall through to **Likely** instead.

## Seed signatures

[`signatures.seed.json`](signatures.seed.json) carries prefixes that are **tracked
upstream but not corroborated** — real candidates, not placeholders, and not proof.
Merge them into your own `signatures.json` if you want the extra recall; leave them
out if you'd rather not chase false positives.

| Prefix | Upstream status | Why it's still unverified |
|--------|-----------------|---------------------------|
| `e0:0a:f6` | nitekry: *Active*, **no confidence note**. WatchFlock: **contract manufacturer** | Never independently corroborated, and its second source rates it *lower*, not higher |
| `14:b5:cd` | *"New finding testing"* (April 2026) | Still under test upstream |
| `04:0d:84` | Listed direct-Flock by WatchFlock | No stated corroboration |
| `f0:82:c0` | Listed direct-Flock by WatchFlock | No stated corroboration |
| `1c:34:f1` | Listed direct-Flock by WatchFlock | No stated corroboration |
| `38:5b:44` | Listed direct-Flock by WatchFlock | No stated corroboration |
| `94:34:69` | Listed direct-Flock by WatchFlock | No stated corroboration |
| `b4:e3:f9` | Listed direct-Flock by WatchFlock | No stated corroboration |

Sources:

- [`nitekry/nite-oui-collection` → `groups/flockers/my_tested_flock.md`](https://github.com/nitekry/nite-oui-collection/blob/main/groups/flockers/my_tested_flock.md),
  a per-prefix table with `Confidence` and `Status` columns (`e0:0a:f6`, `14:b5:cd`).
- [`JakeSwiz/WatchFlock`](https://github.com/JakeSwiz/WatchFlock) →
  `esp32_marauder/WiFiScan.cpp` (branch **`master`**), `fy_flock_mac_prefixes[]` (the
  six below it) and `fy_flock_mfr_mac_prefixes[]` (`e0:0a:f6`). Neither array carries a
  per-prefix confidence or status column, which is precisely why these land here rather
  than in the built-ins.

**Why they aren't compiled in.** The built-in OUI table is a claim that a prefix was
*observed in a fielded Flock deployment*. Neither of these meets that bar, so putting
them there would overstate what's known. This file is the honest home for them. The
schema is a flat array of strings and the parser takes no per-entry metadata, so
there is nowhere in the JSON itself to mark an entry unverified — **this page is that
record.**

## What the built-in table is actually claiming

`flock_ouis[]` in `helpers/flock_db.c` describes itself as prefixes *observed in
fielded Flock Safety deployments*. That is true of most of it, but the 31 entries are
**not uniform evidence**, and nothing in the array says so. Recorded here so the claim
is not read as stronger than it is. **None of this changes scoring** — an OUI-only
match caps at *Possible* whatever its grade, and only ever contributes alongside probe
behaviour or an SSID/IE match.

| Grade | Prefixes | What is actually known |
|---|---|---|
| Flock's own registered OUI | `b4:1e:52` | IEEE-registered to Flock Safety (GainSec). The strongest entry in the table. |
| Field-corroborated | the remainder | Curated upstream table, `Active` or `High confidence`. `e4:aa:ea` caught a Falcon V2 in the field; `82:6b:f2` is DeFlockJoplin field testing. |
| Contract manufacturer | `f4:6a:dd`, `00:f4:8d`, `d0:39:57`, `e8:d0:fc` | Liteon/USI. WatchFlock files these **separately** from direct-Flock prefixes and warns a MAC match alone may be a false positive — these OUIs also ship unrelated consumer hardware. |
| Flat-list orphans | `70:08:94`, `58:00:e3`, `5c:93:a2`, `64:6e:69` | Inherited from the superseded flat list. Absent from the curated table in **every** section — not Active, not under test, not Removed. Kept because absence is not retraction, but their status is unverifiable upstream. |
| Weak upstream confidence | `08:3a:88`, `48:27:ea`, `a4:cf:12` | Upstream notes are *"BLE Ring conflict - unsure"* and *"low confidence, WiGLE crowdsource"*. These do **not** meet the field-corroboration bar. |

Retracting any of these would only lose recall against evidence we do not have, so
they stay. But if you are weighing a single *Possible* OUI hit with nothing else
behind it, this table is what that hit is resting on.

Either way the scoring is the same: an OUI hit from a user file caps at **Possible**,
exactly as a built-in OUI hit does. Nothing here can manufacture a *Confirmed*.

**Removed from the built-ins:** `f8:a2:d6`. Upstream marks it **Removed** — "low
confidence; hit on a Sony Media Player." Don't add it back through a user file
either. Same for `6c:cd:d6` (Netgear), `94:2a:6f` and `f4:e2:c6` (Ubiquiti),
`cc:cc:cc` (no hits), and `00:0c:e7` (possible false positive) — all retracted
upstream, and the last five never shipped here.

`f8:a2:d6` has been removed **twice**, and this page previously said it was gone
since v0.44 while five shipped releases still carried it. It was dropped for v0.44,
then silently re-added by a commit that reflowed the table (`93beede`, 2026-08-05),
and it shipped in **v0.67 through v0.71** — scoring *Likely* on any device sending an
ordinary wildcard probe request. Removed again in **v0.72**. Both the CI parity gate
and the host tests now assert every retracted prefix is absent, because the previous
guard only compared the two built-in tables to each other and that commit changed
both the same way. **If you are running v0.67–v0.71, this prefix is live in your
build.**

**Not imported, on purpose.** `JakeSwiz/WatchFlock` still lists both `cc:cc:cc` and
`f8:a2:d6` as direct-Flock prefixes. Its list is flat and statusless, so it has no way
to record that a prefix was later doubted — importing it wholesale would silently undo
the retractions above. Only the six prefixes named in the seed table were taken from
it, and only into this unverified file. **Do not bulk-import that list.**

## Capturing an IE fingerprint (`ie_fps`)

Fielded Flock cameras increasingly phone home as ordinary Wi-Fi **probe requests**
and rotate their MAC to dodge OUI matching. The probe's **IE fingerprint** — the
shape of its 802.11 information elements — is MAC-independent, so it still catches a
unit that has randomized its address.

To seed one from a unit you have **already confirmed** is Flock:

1. Open **Flock / ALPR Detect** and select the confirmed detection to open its
   detail screen.
2. Read the **`IE-fp:`** line (an 8-hex value, e.g. `IE-fp: 1a2b3c4d`). It only
   appears for probe-sourced detections that carried a fingerprint.
3. Add that value to the `ie_fps` array in `signatures.json`.

From then on, a probe with the same IE fingerprint — even from a *different,
randomized* MAC — is flagged **"Class?"** (a candidate device-class match). Only add
a fingerprint you've corroborated against a real deployment; it's a device-*class*
signature, not a unique device ID.

> The placeholder values in `signatures.example.json` (`aa:bb:cc`, `deadbeef`, …)
> are illustrative and won't match anything real — replace them with your own
> captures, and delete any lines you don't need.
