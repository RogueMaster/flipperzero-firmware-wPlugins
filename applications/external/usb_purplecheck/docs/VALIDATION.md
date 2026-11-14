# Validation - 2026-11-10 - candidate 0.3

## Build and native checks

- Momentum MNTM-012, target f7 API 87.1: compile/link and uFBT APPCHK passed locally.
- Official release 1.5.1, target f7 API 88.2: compile/link, APPCHK and native AddressSanitizer/UndefinedBehaviorSanitizer tests passed in [the source PR workflow](https://github.com/RogerF5-Security/USB-PurpleCheck/pull/1/checks). The workflow also validates the catalog bundle for the source commit it reports.
- Native checks cover settings defaults, CRC and invalid values; translation presence; Ukrainian glyph coverage; FAT12 geometry; exact synthetic return/session binding and altered content; malformed FAT loops, read failures and out-of-volume clusters; separated observations; SCSI write protection, capacities, command length limits and rejection of misaligned writes before backend invocation.

## Physical-device observations

- Uploaded the FAP to /ext/apps/USB/usb_purplecheck.fap with qFlipper and launched it on the attached Flipper Zero running Momentum MNTM-012.
- Visually verified the banner, English default menu, settings, live Spanish/Ukrainian language switching and Created by RogerF5/version rows. SD save confirmation was visible. Six unedited qFlipper PNG exports are in screenshots; their 512x256 geometry and exact orange/black palette are checked.
- Started the real read/write storage profile through its eight-second preparation screen, then closed qFlipper to release its USB lock. Windows briefly displayed a PURPLECHECK volume with PROBE.TXT, README.TXT, RETURN.TXT and SEND.TXT.
- The endpoint security agent displayed an explicit policy message prohibiting USB storage, and the volume disappeared before a file copy could be completed. This is an operator-observed enforcement event on one endpoint, not an automatic Flipper policy verdict and not proof of a bypass. No host policy or security agent was modified.
- Following the configured 120-second trial window, the normal CDC serial port was present again. The result screen and manual conclusion were not subsequently verified or saved through remote control because desktop window activation was unavailable. No exact timeout-latency claim is made.

## Pending hardware acceptance

- A full synthetic SEND-to-PC-to-RETURN round trip on a host that allows storage, followed by an on-device exact-match result and persisted JSONL/last-result inspection.
- HID marker reception in a focused text field; read-only profile behavior on allowed storage; cancellation under active I/O; repeated profile changes; settings language persistence across restart; and SD error handling.
- Testing on official firmware hardware. A passing official build is not a physical test on that firmware.
- Endpoint policy findings require a documented allowed baseline and host enforcement evidence. Block reads can be metadata/indexing. Device-side write protection is not evidence of host-side write blocking. No response remains inconclusive.

The earlier 0.2 on-device menu and native tests passed before this revision. Its desktop receiver was removed from the standalone distribution; no PowerShell execution-policy change is part of the app flow. Old JSONL evidence is retained locally, while last.bin uses a new validated 0.3 record format.
