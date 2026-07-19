# Android KB Bridge (Rust)

**Educational only.** This FAP reimplements the C bridge so you can read a
safe-first Rust version of the same BLE Serial → USB HID flow. It is not the
supported production app — use `../android_keyboard_bridge/` (C) for real use
(`make flipper-launch`).

Built with [flipperzero-rs](https://github.com/flipperzero-rs/flipperzero).
Same BLE protocol as the C FAP.

## Why “safe Rust”?

Flipper’s firmware API is C. You cannot eliminate `unsafe` entirely — the OS
calls your code through C function pointers. What you *can* do is:

1. Keep **all business logic** in safe Rust (`protocol`, shared state, main loop).
2. Hide FFI behind **small safe wrappers** (`usb_hid`, `ble`, `viewport`).
3. Leave only **thin `extern "C"` shims** that turn raw pointers into `&mut App`
   and immediately call safe methods.

### Layering

| Module | Safe? | Role |
|--------|-------|------|
| `protocol.rs` | yes | Stream sync + frame → `HidCmd` |
| `shared.rs` | yes | UI/connection state in `Mutex` |
| `usb_hid.rs` | safe API | USB HID; `unsafe` only inside |
| `ble.rs` | safe API | Serial profile; `unsafe` only inside |
| `viewport.rs` | safe API | GUI viewport RAII |
| `app.rs` | mostly | Orchestration + tiny C callbacks at bottom |
| `main.rs` | yes | Entry |

Start reading at `protocol.rs`, then `shared.rs`, then the safe methods in
`app.rs` (`run`, `apply_hid_cmd`, `on_serial_bytes`). The `unsafe extern "C"`
block at the end of `app.rs` is the whole FFI surface for callbacks.

## Build

```bash
rustup target add thumbv7em-none-eabihf
# from repo root:
make flipper-rust-build
```

→ `android_keyboard_bridge_rust.fap` (copy to Flipper SD Apps).

`make flipper-launch` still builds the **C** FAP. Do not run both at once.
