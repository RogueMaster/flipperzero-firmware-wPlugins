# Fortune Spinner for Flipper Zero

A text-free YES / NO / MAYBE spinner for Flipper Zero.

## Screenshot

![Fortune Spinner running on Flipper Zero](images/fortune-spinner-screen.png)

## Controls

- `OK`: Spin the wheel
- `OK` after a result: Spin again
- `BACK`: Exit

## Target

- Hardware target: Flipper Zero (`7`)
- Firmware: Momentum `mntm-dev`
- Firmware commit: `8ed809fb`
- Firmware API: `87.1`

## Build

Use the Momentum SDK matching the target device:

```powershell
$env:UFBT_HOME = "C:\UFBT"
ufbt update --hw-target=7 --url="https://up.momentum-fw.dev/builds/firmware/dev/flipper-z-f7-sdk-mntm-dev-8ed809fb.zip"
ufbt
```

The compiled app is written to `dist/fortune_spinner.fap`.

## Install and launch

Close qFlipper, connect the device over USB, then run:

```powershell
$env:UFBT_HOME = "C:\UFBT"
ufbt launch
```

The app is installed to `/ext/apps/Games/fortune_spinner.fap`.
