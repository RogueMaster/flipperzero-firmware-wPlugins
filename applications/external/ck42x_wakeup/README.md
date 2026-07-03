# CK42X WakeUp

CK42X WakeUp is a Flipper Zero external app (`.fap`) that turns the device into a small alarm-clock utility with saved alarm slots and hardware-native alert options.

## Features

- 8 configurable alarm slots
- Clock display with RTC startup sync and build-time fallback
- Haptic alerts
- Piezo speaker tone previews and alarm tones
- IR raw capture/replay per alarm slot
- Canned Sub-GHz pulse test/transmit path
- Optional BadKB URL wake triggers for authorized systems
- Persistent Settings toggle to keep the display backlight on while WakeUp is open
- Persistent alarm and counter settings under the app data path

## Time behavior

WakeUp initializes its clock from the Flipper RTC when the RTC date looks valid (`2024-2099`). If the RTC is clearly invalid, such as a device reporting an old/default year, it falls back to the FAP build time instead of trusting the bad RTC. After startup, the displayed clock advances from the Flipper kernel tick counter instead of UI-loop iterations, so rapid button input or slow hardware work does not make the clock jump fast/slow or skip an alarm minute.

The CK42X browser uploader also attempts to set the Flipper RTC from the browser's local time before launching the app.

## Safety and authorization

WakeUp includes optional BadKB URL triggers. Use those only on computers you own or are explicitly authorized to test. The default release does not embed secrets or private payloads.

## Build

Install uFBT, then run from this repository:

```bash
ufbt
```

The built artifact is written to:

```text
dist/ck42x_wakeup.fap
```

## Install

Copy the FAP to the Flipper SD card:

```text
/ext/apps/Tools/ck42x_wakeup.fap
```

Or use the CK42X browser uploader:

```text
https://ck42x.com/tools/ck42x-wakeup/uploader
```

## Release artifact

Current source version: `2.28`

The v2.28 build was verified locally with uFBT:
```text
Target: 7, API: 87.1
Size: 22868 bytes
SHA-256: 5438063cb2781c964e9fbfe0b62a2868f064bc0912f2015031d95dc2764cb2fc
```

## License

MIT. See `LICENSE`.
