# Contributing to FlipDeFlock

This is a community counter-surveillance effort; it improves with more boards and more
field data. Thanks for being here.

What the project is working toward — and the things it deliberately refuses to build —
are in [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md). Worth a read before proposing
anything substantial.

## Most useful contributions

- **Field reports & signatures** — new Flock/ALPR OUIs, SSID/BLE patterns, or false
  positives and misses. Test a candidate in your own `signatures.json` first (see the
  [signatures guide](docs/signatures.md)), then send the ones that hold up. A report
  that a detection *misfired* is as valuable as a new signature.
- **Probe IE fingerprints** captured from a corroborated Flock unit. Each detection's
  detail screen shows its `IE-fp:` for exactly this purpose. Only vetted fingerprints
  get promoted into the compiled-in table; signatures loaded from SD are capped at the
  `Class?` rung by design (see the [signatures guide](docs/signatures.md)).
- **Board support** — try it on your ESP32 and report wiring quirks, baud issues, or
  boards that need nonstandard ports.
- **Code** — bug fixes, report formats, and features. Please open an issue before
  starting anything substantial, so we don't duplicate effort or build in a direction
  that conflicts with the ground rules below.

## Ground rules

- **Passive recon only.** No deauth, injection, jamming, or replay, ever. This is not
  negotiable; PRs adding transmit capability will be closed.
- **Correctness over features.** A false positive is worse than a missed detection.
  Don't trade precision for recall without good reason.
- **Detections are indicators, not proof.** Never over-claim in UI text, reports, or docs.
- Target **API 87.1**; it must build with both `ufbt` and `fbt`.
- Keep it lean — the `.fap` loads entirely into the Flipper's ~256 KB of RAM.

## Before you open a PR

1. `make -C test` passes (host unit tests — plain gcc, no Flipper SDK needed).
2. `ufbt` builds clean.
3. New pure-logic helpers get a test. The parse and detect modules are deliberately kept
   free of firmware dependencies so they stay host-testable — please keep it that way.
4. Formatting matches the repo's `.clang-format`.

## Sign your commits (DCO)

Every commit must carry a `Signed-off-by` line. Add one automatically with:

```sh
git commit -s
```

This certifies the [Developer Certificate of Origin 1.1](https://developercertificate.org/):
that you wrote the patch, or otherwise have the right to submit it under the project's
license.

## Licensing your contribution

By submitting a contribution you agree that:

1. Your contribution is licensed under **GPL-3.0-or-later**, the same as the project.
2. You grant the maintainer a perpetual, worldwide, irrevocable, royalty-free right to
   **relicense your contribution under other terms**, including proprietary or
   dual-licensing arrangements.

**Why clause 2 exists, stated plainly.** It keeps the project's copyright consolidated
so the maintainer can sell a commercial GPL exception — for example to a hardware vendor
who wants to ship FlipDeFlock preloaded in a closed product. That is a funding mechanism
for continued development.

**What clause 2 does not do.** It does not let anyone take FlipDeFlock closed. The public
project stays free and GPL-3.0-or-later — see the pledge in
[README.md](README.md#support) and [SUPPORTERS.md](SUPPORTERS.md). Clause 2 grants an
*additional* license; it does not revoke the
GPL one. Every version ever published under the GPL remains available under the GPL,
permanently, and that cannot be undone.

If you would rather not agree to clause 2, say so in your PR. Field reports, signature
data, bug reports, and board-compatibility notes are all genuinely valuable and none of
them require it.
