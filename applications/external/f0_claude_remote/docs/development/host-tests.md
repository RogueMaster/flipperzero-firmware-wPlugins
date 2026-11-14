---
description: How the core is tested on the Mac with no device, and what only the device can check.
---

# Host tests

```shell
mise run test
```

From `.mise/tasks/`:

- `test` builds the suites under `test/` with the system `cc` and runs them on the Mac,
  one `ok` line per test.

`mise run test` alone also needs the system `cc`, unpinned.
It compiles with the SDK's warning flags as errors,
plus the address and undefined-behavior sanitizers the device cannot run,
and needs no device.
A failed assertion stops the task non-zero, naming its file and line,
after the `ok` lines of the tests before it.

## Writing a test

The core, `src/core/`, includes no Flipper header, so it builds on the Mac.
A suite `test/<name>_test.c` is built with `src/core/<name>.c` and every `test/*_fake.c`:

- each test is a `static void test_…(void)` function on `assert`;
- `main` runs each through `test_run` from `test/test.h`, which prints `ok <name>`;
- code that sends keys gets a `Transport` from `transport_fake_alloc` in `test/transport_fake.h`,
  reads back each press and release with `transport_fake_count` and `transport_fake_event`.

A `.c` file under `test/` builds into no package, and lint checks it.

## What only the device checks

The platform layer, `src/platform/`, and the view, `src/ui/`, run only on the device:
the Bluetooth and USB links, the buttons, the screen, the buzz and the flash, the dolphin's XP,
and what the keys do in Claude Code.
