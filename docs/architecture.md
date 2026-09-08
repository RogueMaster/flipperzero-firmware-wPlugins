# MVP architecture and module plan

## Confirmed baseline

The installed Flipper SDK exports `usb_cdc_dual`, `furi_hal_cdc_set_callbacks`,
`furi_hal_cdc_send`, and `furi_hal_cdc_receive`. The firmware's own USB-UART
Bridge application demonstrates dual CDC with system CLI on channel 0 and app
traffic on channel 1. The FAP follows that less disruptive pattern: it switches
to dual CDC, owns only channel 1, and restores the previous USB configuration on
cleanup. qFlipper/CLI remains on channel 0.

## Components

```text
Flipper UI
    | commands / bounded preview
Flipper bridge session
    | packets
portable C codec + streaming parser
    | bytes
USB CDC transport
================ USB cable ================
POSIX serial port + candidate monitor
    | bytes
Swift codec + bridge session state machine
    | validated request model
permission store ---- secure HTTP policy/client
    |                         |
SwiftUI menu/UI          ephemeral URLSession
```

The FAP manifest compiles only the bridge sources. No Wi-Fi credential path is
present in or linked into the FAP.

## Flipper modules

- `usb_internet_bridge.c`: lifecycle, menu, text input, status and response views
- `usb_transport.[ch]`: CDC channel 1 ownership, worker, RX buffering, bounded TX
- `bridge_protocol.[ch]`: portable frame encoder, CRC32 and streaming parser
- `bridge_session.[ch]`: handshake, permissions, one-request state machine
- `config.h`: all memory, timeout and payload limits

## macOS modules

- `Serial`: IOKit-discovered `/dev/cu.*` candidates and POSIX termios transport
- `Protocol`: wire codec, payload codec and state validation
- `Permissions`: per-device/per-protocol one-time and persistent grants
- `Networking`: URL policy, DNS address classification and ephemeral URLSession
- `App/UI`: menu bar state, permission alert, diagnostics and actions
- `scripts/fibp_simulator.py`: pseudo-terminal peer for hardware-free tests

The core library has no SwiftUI dependency and accepts transport, permission,
and HTTP interfaces, allowing deterministic tests.

## Delivery phases

1. Freeze protocol, limits, threats, and test vectors.
2. Build/verify codec plus HELLO/PING/PONG over CDC and the simulator.
3. Gate every request behind once/always/deny permission state.
4. Add bounded HTTPS GET, response metadata and incremental body frames.
5. Add DNS/redirect SSRF checks, cancellation, timeouts and disconnect cleanup.
6. Run portable C and Swift tests, build the FAP/helper, and document hardware
   validation separately from simulator validation.
