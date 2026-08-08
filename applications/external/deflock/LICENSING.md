# Licensing

FlipDeFlock is available under two licences. You choose which one applies to you.

## 1. GPL-3.0-or-later — free, for everyone

This is the licence the project ships under and the one almost everyone wants. It costs
nothing, it is not time-limited, and it is not a trial. You get the complete application:
every detection, every screen, every signature. See [LICENSE](LICENSE).

**This is not going away.** The pledge in [README.md](README.md#support) and
[SUPPORTERS.md](SUPPORTERS.md) stands: FlipDeFlock is free and stays free, nothing is
gated, and every version published under the GPL remains available under the GPL
permanently. The commercial option below is *additional*. It takes nothing away from
anyone and it does not close any part of the project.

Under the GPL you may use, study, modify, redistribute and **sell** FlipDeFlock. The
main obligation is reciprocity: if you distribute a modified version, you publish its
source under the same terms and keep the notices intact.

## 2. Commercial licence — for closed products

The GPL's reciprocity is a problem for exactly one kind of user: someone shipping
FlipDeFlock as part of a product whose source they cannot publish. GPL §5 and §6 would
require them to release the source of their derivative, and a contract, a supplier
agreement, or an internal policy may make that impossible.

A commercial licence waives that specific obligation for that specific party.

**Typical cases**

- A hardware vendor shipping FlipDeFlock preloaded on a device with closed firmware.
- Bundling the detection engine into a proprietary product or internal tool.
- An organisation whose policy forbids distributing copyleft-licensed code.

**If you are not doing one of those, you do not need this.** Personal use, research,
educational use, forking, modifying, redistributing, and selling copies are all already
permitted by the GPL at no cost.

### What a commercial licence covers

- The FlipDeFlock source authored by the project — the ~100 files carrying
  `SPDX-License-Identifier: GPL-3.0-or-later` and the project's copyright line.

### What it does not cover

- **The name and logo.** Trademark is separate and is not licensed here. See
  [TRADEMARK.md](TRADEMARK.md).
- **Bundled third-party components.** `esp-serial-flasher` (Apache-2.0), Nayuki
  `qrcodegen` (MIT), `jsmn` (MIT) and the ESP MD5 routine (BSD) belong to their authors
  and keep their own licences. They are permissive, so a closed product can use them —
  but you comply with them directly, not through this project.
- **The Flipper Zero firmware and SDK**, which have their own licence terms.
- **Any warranty.** The software is provided as-is under both options. Detections are
  indicators, not proof.

### What it does not change

The project's ground rules are not for sale and are not negotiable as licence terms:
passive recon only, precision over recall, no network, and no over-claiming a detection.
A commercial licensee gets different *distribution terms*, not a different product and
not influence over what the product does.

## Why this arrangement exists

Consolidated copyright is what makes it possible. Every line of FlipDeFlock is the
maintainer's own work, and [CONTRIBUTING.md](.github/CONTRIBUTING.md) keeps it that way by asking
contributors for a relicensing grant — with an explicit opt-out, since data contributions
(field reports, signatures, board reports) are the most valuable ones and carry no
copyright entanglement at all.

Selling an exception to a vendor who needs one funds development time and test hardware
without paywalling anything for anyone else. It is the one revenue mechanism that is
compatible with the pledge, which is why it is the one being used.

## Enquiries

Open a thread in
[Discussions](https://github.com/ReconGrunt/FlipDeFlock/discussions) titled
"Commercial licensing" and a private channel can be arranged from there.

Please include what you intend to ship, roughly how it would be distributed, and whether
you need the exception for the Flipper application, the ESP32 companion firmware, or both.
