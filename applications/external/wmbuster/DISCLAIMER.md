# Legal Disclaimer

> [!IMPORTANT]
> Read this document **before** installing or using **wM-Buster**. By
> downloading, building, flashing, or otherwise using this software you
> acknowledge that you have read and accepted the terms below.

## 1. Nature of the Software

**wM-Buster** is a passive, **receive-only** research and educational tool
for inspecting Wireless M-Bus traffic on the unlicensed 868 MHz Short Range
Device (SRD) band. It does not transmit, jam, replay, spoof, or otherwise
emit on the radio spectrum. It is intended for:

- The author's own metering equipment.
- Authorised security research with the operator's prior written consent.
- Educational study of the EN 13757 / OMS protocol stack.
- Validation of metering deployments by the responsible utility, property
  manager, or sub-meter operator.

## 2. Permitted Use

You may use this software **only** in jurisdictions and contexts in which
its operation is lawful. You are solely responsible for determining whether
that is the case for your specific situation. In particular:

- **Reception of signals you are entitled to receive** — for example, your
  own meter, where applicable national law gives you a statutory right of
  access to your consumption data (e.g. EU Directive 2018/2002 Art. 9–11).
- **Authorised research** with documented permission from the meter
  operator or the meter's owner.
- **Possession and study** of the software itself as protected expression
  (open-source / academic speech).

## 3. Prohibited Use

The author, contributors, and distributors **expressly forbid** and
**will not assist with** any of the following:

- Decrypting or analysing telegrams from a meter that does not belong to
  you and for which you have no documented authorisation. In most
  jurisdictions this constitutes:
  - unlawful interception of private communications,
  - unlawful processing of personal data (GDPR Art. 6 / equivalents),
  - and/or theft-of-services / fraud where consumption data is used to
    estimate occupancy or to evade billing.
- Transmitting on the 868 MHz band using firmware modifications derived
  from this project. The on-device code path is RX-only and any TX
  capability would require a fork that the upstream maintainers do not
  endorse.
- Reverse-engineering or breaking the AES key infrastructure of a meter
  fleet you do not operate.
- Stalking, profiling, or surveilling individuals through their utility
  consumption patterns.
- Any other use prohibited by applicable law in your jurisdiction.

> [!CAUTION]
> Receiving and decrypting your neighbour's meter is illegal in most of
> Europe, North America, and the UK. "I was just curious" is **not** a
> defence. If in doubt, **don't**.

## 4. Radio Spectrum

The CC1101 transceiver in the Flipper Zero, including any external CC1101
module attached to its GPIO header, is licence-exempt under
**ETSI EN 300 220** (EU) and equivalent rules in other regions, *for
receive-only use within the 433 / 868 / 915 MHz SRD bands*. wM-Buster does
not move outside these constraints.

You are responsible for compliance with your local radio regulations
(e.g. FCC Part 15, Industry Canada RSS-210, UK OFCOM IR 2030, etc.). If
operation of the CC1101 is restricted in your jurisdiction even on receive,
you must not run this software there.

## 5. AES Keys

> [!WARNING]
> **Never publish, share, or commit AES-128 keys obtained from a meter.**

Meter keys are typically printed on the installation document or made
available through the property-management portal that operates the meter
(Techem, ista, Kamstrup, etc.). Sharing them — even in a bug report — may
expose the meter operator's customers to billing fraud and is almost
always a violation of the meter-supply contract. The maintainers will
**delete and refuse** any issue, pull request, or attachment that contains
real keys, real meter IDs in clear, or decrypted telegrams from a meter
that is not the contributor's own.

## 6. No Warranty

The software is provided **"AS IS"**, without warranty of any kind. See
sections 15–17 of the [GNU GPL v3](./LICENSE) for the full disclaimer of
warranty and limitation of liability. The author and contributors are
not liable for any direct, indirect, incidental, special, exemplary, or
consequential damages arising from the use of, or inability to use, this
software — including but not limited to civil or criminal liability
arising from misuse, regulatory action, lost revenue, lost data, or
hardware damage.

## 7. No Affiliation

wM-Buster is an **independent** open-source project. It is **not**
affiliated with, endorsed by, or supported by:

- Flipper Devices Inc. or any Flipper Zero firmware distribution
  (mainline, Momentum, Unleashed, Xtreme, RogueMaster, etc.).
- Any meter manufacturer named in the source code, documentation, driver
  comments, or test fixtures (Techem, Kamstrup, Diehl, Engelmann,
  Sontex, BMeters, Apator, Zenner, Qundis, ista, Brunata, GWF, etc.).
  These names appear solely to identify the protocols the drivers
  decode, in compliance with nominative-fair-use principles.
- Any utility, metering authority, or property-management company.

## 8. Indemnification

By using or distributing wM-Buster you agree to indemnify and hold
harmless the maintainers and contributors against any claim, demand,
loss, liability, damage, fine, or expense (including reasonable legal
fees) arising out of your use of the software in violation of this
disclaimer or applicable law.

## 9. Reporting Misuse

If you become aware of someone using wM-Buster to attack a meter
infrastructure they do not own, please report it to the meter operator
and to the appropriate authority in your jurisdiction (national CERT,
data-protection authority, or police). Do **not** open a public issue —
that gives attackers a pre-built roadmap.

---

*This disclaimer is informational and does not constitute legal advice.
Consult a qualified lawyer in your jurisdiction if you are unsure whether
a specific use is lawful.*
