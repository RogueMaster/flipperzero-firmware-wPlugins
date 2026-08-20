# Backlog and handover

Where the work stands, what is blocked and on what, and — most importantly — **which merged
changes have never been run on real hardware.** Written 20 Aug 2026.

If you are picking this up cold, read [README.md](README.md) for what the app is, then this
file for what is left.

## State

- `master`, everything merged, no open PRs on this repo.
- CI (`.github/workflows/checks.yml`) runs five checks on every PR: `clang-format`,
  `SUPPORTED_CHIPS.md matches chip_db.c`, and a build against each of the three firmware
  SDKs (official, Unleashed, Momentum). The builds are the only compile check that exists —
  `ufbt lint` runs clang-format and nothing more.
- 80 I²C parts and 15 1-Wire families in the database.

## The honest list: merged but never exercised on hardware

None of these are known broken. They are simply unproven — they compile, their logic was
traced, and no one has watched them run. Anything on this list should be the first thing
tried when a Flipper and the right parts are next in the same room.

| What | Landed | What has never happened |
|---|---|---|
| LPUART transport and the automatic listen after an empty sweep | #31 | No byte has been sent or received on a real LPUART. Test plan steps 7a–7e all outstanding. |
| UART self-test screen | #34 | The screen has never been drawn; the loopback has never executed. |
| Bare SDA-to-SCL short detection | #36 | Never reproduced with an actual jumper. This is `TESTING.md` Step 2, which the app could not satisfy at all until #36. |
| Strap-and-blink power-cycle ladder, and the pad meter | #20 | The rail blink, and the automatic rescan behind it, unwatched. The pad meter has never been checked against a known level (pin 8 must read LOW, pin 9 HIGH, open air FLOATING). |
| Live-test verdict wording | #21 | Never seen on a screen. |
| Chip `kind` renames | #33 | Text only. Widths were measured exactly (see below), not photographed. |

Screen widths in #33 and #34 were measured with
[`tools/screen_width.py`](tools/screen_width.py), which decodes the real `FontSecondary`
out of the SDK's `firmware.elf` and reimplements `u8g2_string_width`. That is exact, but it
is arithmetic, not a photograph.

## Blocked on the bench

Needs a Flipper plus the named part. Nothing here can be closed by reading code.

- **Test plan steps 7a–7e** — the LPUART transport, end to end. `TESTING.md` Step 2 now has
  a self-test paragraph; start there, it needs one jumper and no sensor.
- **Stage C: BNO055 interrogation over UART.** Deliberately **not** in `master` — the
  inter-byte timing and the reset-to-ready delay cannot be learned without the part, and
  shipping it marked "unverified" was rejected. Needs a live BNO055 strapped into UART mode.
- **The 1-Wire busy screen** — does it need a spinner? Needs a real 1-Wire part on pin 17 to
  judge whether the pause is long enough to look wedged.
- **Power cycle and rescan**, on the VL6180X: the wiring screen's live lines should visibly
  go dark and come back, and the automatic rescan should find the part again.

Note the standing limitation: the app **cannot** switch the external 3V3 rail on its own —
that was measured, not assumed. The ladder works around it by asking the user.

## Blocked on datasheets

- **21 mode-pin rows** are drafted but unverified. Every row in `chip_mode_pins` must carry a
  datasheet quote in a comment, because a wrong `i2c_high` sends someone to strap a pin the
  wrong way — worse than having no row at all. Bosch, ST, Analog Devices and TDK PDFs are
  unreachable from the machine this was developed on; they need fetching some other way.

## Needs a decision from the maintainer

- **A paced, captioned demo GIF** for the catalog pull requests — half-built, and it is not
  clear it is still wanted. Decide, then either finish it or drop the idea.

## Getting the app into the firmware catalogs

Four outbound pull requests, none of them on this repo. Status as of 18 Aug 2026:

| Catalog | PR | State |
|---|---|---|
| Unleashed | `xMasterX/all-the-plugins#254` | **Merged**, and confirmed by eye in the catalog. |
| Official | `flipperdevices/flipper-application-catalog#1183` | Changes requested. Both review items addressed 12 Aug, branch bumped 16 Aug, no reply since. The `fixes needed` label is still on. Its `task-list-completed` check sits at 6/10 and cannot go green here: three of the four remaining boxes belong to the reviewer, not the submitter. |
| Momentum | `Next-Flip/Momentum-Apps#82` | Untouched since 11 Aug. |
| Curated list | `djsime1/awesome-flipperzero#172` | Untouched since 11 Aug. |

## Things that will trip you up

- **Run `ufbt` from `fake_chip_detector/`**, never from the repo root, or it cannot find
  `application.fam`.
- **The SDK minor-API trap.** The loader requires the app's minor API to be **no higher**
  than the firmware's. An app built against API 88.3 will not start on a 88.2 firmware;
  one built against 88.2 loads on both.
- `~/.ufbt/current` is **machine-wide shared state.** Changing the SDK affects anything else
  building on that machine.
- **`python tools/gen_supported_chips.py --check` must pass.** It regenerates
  `SUPPORTED_CHIPS.md` from `chip_db.c` and enforces a 21-character cap on `kind` strings.
  That cap is a *tripwire, not a guarantee* — characters do not predict pixels. When it
  fires, measure with `tools/screen_width.py` and raise it if the string genuinely fits.
- **Every ID register must trace to a datasheet.** A wrong expected value makes the app call
  a genuine part counterfeit, which is the one failure this app must never have. Dropping a
  chip is always better than guessing at it.
- **A strap is configuration, not fraud.** Nothing in the mode-pin work is allowed to
  produce a counterfeit verdict, and an ID read over UART is exactly as forgeable as one
  read over I²C.
