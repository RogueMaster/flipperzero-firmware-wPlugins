# Host tests

Protocol-layer tests that run on a PC, not on the Flipper.

```sh
pwsh test/run.ps1
```

Exit code 0 means every check passed; failures print file, line and the
expected/actual values.

## Why these exist

The Flipper can prove the app compiles, links and starts. It cannot prove we
understood the MeshCore wire format — only a node can do that, and one is not
always on the bench. These tests close that gap for everything that does not
need radio:

- **Framing.** That an app→radio frame is led by `0x3C` (`'<'`) and not `0x3E`.
  This is the single easiest thing to get backwards in this protocol, and
  getting it wrong looks exactly like a dead node. There is a check whose only
  job is to fail if that byte ever flips.
- **Frame assembly.** Frames split across reads, two frames in one read, and
  resync past line noise — the three things a real UART does that a loopback
  test on the bench would not show.
- **Command layout.** Byte-for-byte checks on the commands the app sends, so a
  field that moves upstream is caught here rather than as a node that ignores
  us.
- **Response parsing.** `SELF_INFO`, `DEVICE_INFO` and `CONTACT` are decoded
  from hand-built payloads with known values.
- **`meshcore_link` logic.** Request/reply matching, stepping over unsolicited
  pushes that arrive mid-request, telling a node-side error apart from a
  timeout, and that silence times out instead of spinning.

## What they do not cover

Anything touching real hardware: `furi_hal_serial`, the RX interrupt, the
worker threads, and every scene. Those still need a Flipper, and the parsing of
replies from a real node still needs a real node.

## How it is wired

`shims/furi.h` is a minimal stand-in for the Flipper `<furi.h>` — only what the
protocol layer actually calls. `fakes.c` replaces the UART and the debug log,
and drives a fake clock: time advances only when a blocking read is asked to
wait, which makes timeout tests deterministic and instant.

`protocol/meshcore_link.c` and the vendored `meshcore_companion.c` are compiled
from their real sources. Our own files are built with `-Wall -Wextra -Werror`,
the same bar as the firmware build; the vendored library is compiled without
`-Werror` so a clang-only warning upstream cannot block our tests.

## Toolchain

Zig provides the C compiler, because it installs without admin rights and
without MSYS2:

```sh
pip install ziglang
```

Any C11 compiler works if you would rather edit `run.ps1` — nothing here is
Zig-specific.

## Keeping them honest

A test suite that cannot fail is worse than none. After a change that makes
everything pass, break one expectation on purpose and confirm the run turns
red and points at the right line.
