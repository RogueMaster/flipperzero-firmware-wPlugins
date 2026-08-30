# Native BLE plugin

The Flipper's own radio: the GATT service, the profile, and the peripheral
role built on them.

Built as a `.fal` and shipped inside the app's `.fap`. See
`../ble_ext_fal/README.md` for how the boundary works and why `ufbt` warns
about unresolved symbols.
