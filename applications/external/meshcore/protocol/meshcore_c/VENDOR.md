# Vendored: meshcore_c

Unmodified copy of the portable C99 MeshCore companion-protocol client.

- Upstream: <https://github.com/SH3D/meshcore_c>
- Commit: `b9e727ade800eadbfbc5d97b0094f21417473d54` (2026-06-10, "bump version for RSSI/SNR fix")
- Library version: `MESHCORE_COMPANION_VERSION` `0.2.2`
- License: MIT — © Scott Penrose / Digital Dimensions (see `LICENSE`)
- Files taken: `src/meshcore_companion.h`, `src/meshcore_companion.c`

Only the portable core is vendored. The upstream Arduino C++ wrapper is not
used — this app binds the core to `furi_hal_serial` in `../meshcore_link.c`.

## Do not patch these files

The core does no I/O and no dynamic allocation, which is exactly what a Flipper
app wants. Keep it byte-identical to upstream so it can be re-synced with a
plain copy. Anything Flipper-specific belongs in `meshcore_link.c`.

In particular, **do not "fix" the frame lead bytes.** `MC_FRAME_APP_TO_RADIO`
is `0x3C` (`'<'`) and `MC_FRAME_RADIO_TO_APP` is `0x3E` (`'>'`), which matches
the firmware: `ArduinoSerialInterface::writeFrame()` emits `'>'` and
`checkRecvFrame()` waits for `'<'`, both from the *node's* side. See AGENTS.md.

## Re-syncing

```sh
curl -sSL -o protocol/meshcore_c/meshcore_companion.h \
  https://raw.githubusercontent.com/SH3D/meshcore_c/main/src/meshcore_companion.h
curl -sSL -o protocol/meshcore_c/meshcore_companion.c \
  https://raw.githubusercontent.com/SH3D/meshcore_c/main/src/meshcore_companion.c
```

Then update the commit hash above and rebuild.
