# Building

All commands run from inside a specific app's folder, e.g. `apps/jayx/`.

## Requirements

- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (`pip install ufbt`)
- Flipper Zero connected via USB for flashing (optional for just building)

## Commands

```sh
ufbt                  # build -> ./dist/<app>.fap
ufbt launch           # build + install to a connected Flipper
ufbt cli              # open Flipper's serial CLI
ufbt update           # update the SDK
ufbt vscode_dist      # (re)generate .vscode/ and lint/format config
ufbt debug            # build + start GDB against the connected Flipper
```

## Debugging

`ufbt vscode_dist` generates `.vscode/launch.json` wired to the ufbt toolchain's `arm-none-eabi-gdb` and OpenOCD. Open the app's folder (not the repo root) in VS Code to get working debug/build tasks and IntelliSense.
