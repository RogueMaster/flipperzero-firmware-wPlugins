# Flipper Bridge Client SDK

This source SDK lets another Flipper Zero FAP use an authorized desktop host as
an HTTPS transport. It is not a background service: Flipper OS runs one FAP at a
time, so each consumer compiles the client sources into its own application.

## Add it to a FAP

Vendor this repository (or add it as a Git submodule) and compile these files:

```python
sources=[
    "your_app.c",
    "vendor/flipper_usb_to_wifi/sdk/flipper/fib_bridge_client.c",
    "vendor/flipper_usb_to_wifi/bridge_session.c",
    "vendor/flipper_usb_to_wifi/bridge_protocol.c",
    "vendor/flipper_usb_to_wifi/usb_transport.c",
]
```

Include `vendor/flipper_usb_to_wifi/sdk/flipper/fib_bridge_client.h`, allocate a
client, start it, and call `fib_bridge_client_tick()` from the application's
regular event loop. Wait for `FibBridgeStateReady` before calling
`fib_bridge_client_get()`.

Response chunks are delivered to `on_body`; they are not accumulated by the SDK.
Return `false` from that callback to stop a response when the consumer's own
limit is reached. Call `fib_bridge_client_cancel()` before leaving a screen with
an active request, and always call `fib_bridge_client_free()` during shutdown.

`examples/flipper_bridge_client/` is a complete standalone FAP using only the
public client API. Build it without modifying the main application manifest:

```sh
UFBT=/path/to/ufbt ./scripts/build_sdk_example.sh
```

The script prints the generated `fib_sdk_example.fap` path. The example displays
connection state, performs a bounded HTTPS GET, pages through its preview, and
cancels active work during exit.

## Constraints

- FIBP v1 supports one request at a time.
- Only HTTPS GET is exposed by this SDK version.
- The desktop helper must be running and the user must grant permission.
- The SDK temporarily owns USB CDC interface 1 and restores the previous USB
  configuration when freed.
- Status/body callbacks run on the transport worker and must remain short.
