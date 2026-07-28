<!-- visibility: public --><!-- PUBLIC-SAFE: reviewed 2026-07-25 -->

# FlipDeFlock — Project Plan

The public statement of what this project is for, what it commits to, what it is
working toward, and what it refuses to build. Written for prospective funders,
partners, contributors, and users deciding whether to trust it.

For released work see [changelog.md](../changelog.md). For how to contribute see
[CONTRIBUTING.md](../CONTRIBUTING.md).

---

## The problem

Automated licence-plate readers and fixed surveillance cameras are being deployed
across public roads at scale, largely without notice to the people being recorded.
Bluetooth trackers designed to find lost keys are routinely repurposed to follow
people. In both cases the person being surveilled is the only party without
visibility.

Professional counter-surveillance equipment exists, but it is priced and packaged for
institutional buyers. There is no accessible, auditable way for an individual to
answer a basic question: *is something watching me right now, and what is it?*

FlipDeFlock answers that question on hardware many people already own, using an
approach that can be read, verified, and corrected by anyone.

## Who this serves

- **People at risk of being followed.** Bluetooth tracker stalking is a documented
  pattern in intimate-partner abuse. Detection that works offline, on a device that
  isn't a phone, matters when the phone itself may be compromised or monitored.
- **Journalists and researchers** documenting surveillance infrastructure, who need
  defensible, exportable evidence rather than screenshots.
- **Civic transparency efforts** mapping public-space surveillance, including the
  [DeFlock](https://deflock.org) community.
- **Security practitioners** performing authorised site surveys and access-control
  reviews.

## What exists today

Version 0.44. A Flipper Zero application paired with commodity ESP32 hardware that
detects Flock/ALPR cameras across Wi-Fi and BLE, identifies Bluetooth trackers that
persist across your movement, grades nearby Wi-Fi security, flags active Wi-Fi
attacks, audits NFC/RFID credentials, and exports findings as Markdown, GeoJSON, KML,
CSV, and WiGLE.

Detections carry an explicit confidence ladder — *Possible*, *Likely*, *Confirmed*,
*Class?* — because a counter-surveillance tool that overstates certainty is worse than
none. Correctness-critical logic is covered by a 291-check host test suite gated in CI.

**Stated honestly:** most work since v0.20 is verified by continuous integration
against the compiler, not against real deployed hardware. Closing that gap is the
project's first objective, below, and no claim of field reliability should be read
into the current release.

---

## Commitments

These constrain everything below and are not subject to feature pressure.

1. **Passive only.** The tool listens. It does not transmit, inject, jam, deauthenticate,
   or replay. Not as a default — as an architectural property.
2. **No network.** The device never connects to anything. No telemetry, no update checks,
   no accounts, no server. A tool used by people evading surveillance must not itself be
   a reporting channel.
3. **Detections are indicators, not proof.** The interface, the reports, and the
   documentation all state confidence explicitly and never assert more than the evidence
   supports.
4. **Precision over recall.** A false positive is worse than a missed detection. Someone
   acting on a wrong reading is worse off than someone who got nothing.
5. **Free and open, permanently.** GPL-3.0-or-later. No paid tiers, no gated detections,
   no feature withheld from anyone.

## Objectives

Outcome-framed. Each names what will exist that does not exist now, and how anyone can
verify it.

### 1. Field validation against real deployments

The gap between "compiles and passes tests" and "works when it matters."

**Delivers:** detection thresholds tuned against real deployed hardware rather than
estimated; a documented, repeatable validation methodology others can run; a published
false-positive and false-negative characterisation across the detection ladder; verified
behaviour for the geolocation, mapping, and export paths.

**Verifiable by:** published methodology and results; threshold values traceable to
field data rather than to a developer's guess.

### 2. Detection that keeps pace without waiting on releases

Surveillance hardware changes. A detector that can only be updated by shipping a new
binary is perpetually behind, and users on older builds are silently less protected.

**Delivers:** a community-maintained signature set distributed independently of
releases; a documented capture-and-corroboration procedure so contributors can submit
evidence; a vetting path that promotes only independently corroborated signatures to
higher confidence. Signatures loaded from removable media are capped below *Confirmed*
by design — unvetted data cannot assert certainty.

**Verifiable by:** the count of independently corroborated signatures, and the published
provenance of each.

### 3. Safe to use in the situations it is for

A tool that reveals you are scanning is dangerous to the people who most need it. Reading
a screen that says a surveillance device is nearby, while standing near that device, is
itself an exposure.

**Delivers:** a discreet operating mode conveying state without visible on-screen
disclosure; review of every user-facing string for information that could endanger
someone if read over their shoulder.

**Verifiable by:** the tool being usable end-to-end without displaying anything that
identifies what it is doing.

### 4. Works on hardware people can actually get

**Delivers:** verified compatibility across commodity ESP32 boards rather than one
blessed configuration; firmware installable from the Flipper itself with no computer;
a documented companion wire protocol so third parties can build compatible hardware
independently of this project.

**Verifiable by:** the number of distinct boards confirmed working, and an interoperable
implementation existing that this project did not write.

### 5. Assurance proportionate to the claim

A security tool should meet the standard it audits against.

**Delivers:** test coverage extended across the remaining detection- and
parsing-critical modules; hardening of the untrusted-input surfaces, principally
signature-file parsing and report generation against hostile device names; a documented
vulnerability disclosure process.

**Verifiable by:** coverage of correctness-critical modules, and CI gating that blocks
regressions.

---

## Explicitly out of scope

Refusals, with reasons. These are boundaries, not a backlog.

- **Any transmit capability.** No deauthentication, injection, jamming, or replay. This
  disqualifies a range of otherwise-effective techniques, and that is accepted.
- **Active NFC key recovery.** Recovering keys from a reader requires actively emulating
  a card to harvest its responses. That is outside the passive posture. It would only be
  reconsidered as a separately gated mode with explicit, informed consent.
- **Mapping private home-security devices.** Decoding consumer alarm and motion sensors
  is technically adjacent and deliberately not pursued. The project exists to give people
  visibility into surveillance *of* them, not to catalogue the private security of
  households.
- **On-device submission to public databases.** Uploading findings directly would require
  network connectivity and authentication on the device, breaking the no-network property.
  An offline hand-off — a locally rendered QR code the user's own phone chooses to act on
  — is the deliberate substitute.
- **Asserting a vendor or model without a verified signature.** Naming the wrong
  manufacturer in someone's evidence is a serious harm. Unverified classifications stay
  capped at low confidence or are not made.

---

## Sustainability and licensing

All software produced is licensed **GPL-3.0-or-later** and published in full. Detection
signatures are distributed freely; the project does not gate detection capability behind
payment, and will not.

Development is presently volunteer-led and unfunded. Ongoing costs are test hardware and
the time required for field validation — which is precisely the bottleneck described in
Objective 1. The project is supported by individual donations
([SUPPORTERS.md](../SUPPORTERS.md)), and is exploring additional ways to sustain the
work. All of them are constrained by the same rule: whatever funds development, the
resulting software is published freely to everyone under the same licence. Funding
changes who pays for the work, never who is allowed to use it.

## How to judge whether this succeeded

- Detection thresholds traceable to field measurement rather than estimation.
- A published false-positive characterisation, and it being low.
- Independently corroborated signatures contributed by people other than the maintainer.
- Distinct ESP32 boards verified working by their owners.
- A compatible third-party implementation of the companion protocol.
- Correctness-critical modules under test, with CI blocking regressions.
- Evidence produced by the tool proving useful in real documentation or reporting work.

## Contact

Issues and contributions: <https://github.com/ReconGrunt/FlipDeFlock>.
Security reports follow [SECURITY.md](../SECURITY.md) — please do not file those publicly.
