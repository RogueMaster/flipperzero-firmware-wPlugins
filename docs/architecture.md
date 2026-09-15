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
desktop serial candidate monitor
    | bytes
FIBP host session state machine
    | validated request model
permission store ---- secure HTTP policy/client
    |                         |
SwiftUI/terminal UI      URLSession or direct TLS
```

The FAP manifest compiles only the bridge sources. No Wi-Fi credential path is
present in or linked into the FAP.

## Flipper modules

- `usb_internet_bridge.c`: lifecycle, menu, text input, status and response views
- `usb_transport.[ch]`: CDC channel 1 ownership, worker, RX buffering, bounded TX
- `bridge_protocol.[ch]`: portable frame encoder, CRC32 and streaming parser
- `bridge_session.[ch]`: handshake, permissions, one-request state machine
- `sdk/flipper`: stable streaming GET/status/cancel API for consumer FAPs
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

## Cross-platform host modules

- `host/fibp_host/serial_ports.py`: pyserial discovery and CDC transport
- `host/fibp_host/session.py`: HELLO, permission and request state machine
- `host/fibp_host/network.py`: direct TLS, pinned resolved IP and redirect policy
- `host/fibp_host/permissions.py`: hashed per-user persistent grants
- `host/fibp_host/transforms.py`: bounded National Today and radio responses
- `host/fibp_host/cli.py`: permission prompt, reconnect loop and worker lifecycle

## Delivery phases

1. Freeze protocol, limits, threats, and test vectors.
2. Build/verify codec plus HELLO/PING/PONG over CDC and the simulator.
3. Gate every request behind once/always/deny permission state.
4. Add bounded HTTPS GET, response metadata and incremental body frames.
5. Add DNS/redirect SSRF checks, cancellation, timeouts and disconnect cleanup.
6. Run portable C and Swift tests, build the FAP/helper, and document hardware
   validation separately from simulator validation.
7. Publish the source client SDK and CI-built Windows/Linux terminal hosts.
