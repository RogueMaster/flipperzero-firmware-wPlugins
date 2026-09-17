# Security

## Reporting

Please report anything you think is a security problem by opening an issue at
[github.com/at0m-b0mb/Specter-FlipperZero/issues](https://github.com/at0m-b0mb/Specter-FlipperZero/issues).
If you would rather not do that in public, say so in the issue without details
and a private channel will be arranged.

Fixes land in the next tagged release, and the report is credited unless you ask
otherwise.

## Supported versions

The latest release. This is a single-binary Flipper app with no update channel of
its own, so older versions are not patched — reinstall the current `.fap`.

## Threat model

Being clear about the attack surface matters more than a long checklist, because
most of the usual categories simply do not apply here.

**Specter never transmits.** It puts the onboard ST25R3916 into field-detect mode
and reads one bit: *is an external 13.56 MHz carrier present right now?* It does
not emit a carrier, does not modulate, does not talk to readers or cards, and
does not decode anything a reader sends. A hostile reader cannot deliver a
payload to Specter, because there is no channel to deliver one over — the only
thing it can influence is the *timing* of a single boolean.

**There is no network.** No Wi-Fi, no Bluetooth, no USB protocol, no companion
app, no telemetry. Nothing leaves the device.

**The realistic untrusted input is the SD card.** `specter.conf` and
`logbook.txt` / `logbook.csv` live under `apps_data/specter/` and can be edited
by anyone with physical access to the card or by another app on the Flipper.
These are treated as untrusted:

- settings are range-clamped before any value is used as a table index, and
  booleans are normalised, because a `_Bool` holding something other than 0 or 1
  is undefined behaviour the moment it is read;
- the logbook parser is bounds-checked and host-tested against ragged input,
  truncated buffers and entries with no detail line;
- the viewer only ever reads the tail of the log into RAM, so the file is allowed
  to be larger than memory.

**Adversarial RF timing.** A reader could in principle modulate its field to
drive the edge-timing code oddly. All the resulting figures are clamped, the
cadence buffers are fixed-size ring buffers, and the classifier is a pure
function that is exhaustively host-tested. The worst outcome is a wrong reading
on screen, not memory corruption.

## What is *not* a vulnerability

These are documented limits, not defects:

- **It cannot see 125 kHz (LF) readers.** The Flipper's LF path has no equivalent
  field-detect bit.
- **It detects a carrier, not intent.** Specter cannot tell a skimmer from a
  legitimate terminal. It reports that something is emitting and how it behaves.
- **A silent reader is invisible.** A dormant skimmer that only wakes on a real
  tap, or one that is shielded, produces no field to detect. `CLEAN` means clean
  *at the sensitivity you chose*, and the app says so.
- **`FIELD %` is relative, not calibrated.** It is a scaled carrier duty-cycle for
  comparing positions, not a measured distance.

## Assurance

The decision layers — everything that turns numbers into a claim — are pure C
with no hardware dependency, and are covered by **437 host checks** that run in
CI before the firmware is built. A static layout checker guards the screen
drawing. Every release builds clean on both the release and dev firmware SDKs
under `-Wall -Wextra -Werror`.
