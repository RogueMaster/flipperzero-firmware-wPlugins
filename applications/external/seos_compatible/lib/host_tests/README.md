# Host tests

`make test-host` builds and runs the suite on a workstation.

The app's own `.c` files are compiled directly against the shims here, so the
tests exercise the same code the firmware runs. `furi.h`, `furi_hal.h` and
`lib/toolbox/bit_buffer.h` stand in for the Flipper API; nothing else is mocked.

Two shims differ from the firmware on purpose:

- `bit_buffer_mock.c` bounds-checks every append and aborts on overflow, as
  `furi_check` does on device. A mock that silently overran would hide the
  faults these tests are looking for.
- `furi_hal_mock.c` makes randomness replayable, so a recorded exchange
  reproduces exactly. Queue octets with `seos_host_set_random`.

Each `test_*.c` exports one `MunitSuite`; `test_main.c` lists them.
