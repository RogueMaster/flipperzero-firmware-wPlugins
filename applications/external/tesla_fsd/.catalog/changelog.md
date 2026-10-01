## v2.16.1

- HW4 speed profile is written to the right bits again. The old write cleared the frame's valid flag, which stopped the speed offset from working.
- OTA detection no longer mistakes the rolling counter in 0x318 for an update in progress, which could silently stop all transmitting on newer cars.
- HW4 Autopilot state is read from the documented DBC position, and all injection pauses while the car runs in-car Autopark.
- Signal Map presets for cars with a non-standard DAS layout.
- Telemetry Off (experimental), opt-in and off by default.
- Steer-jerk work: Soft Engage, Instant Engage and Minimal Inject (experimental).
- CAN Capture to the SD card and Send Test profiles with parked-only transmit.
- Hardware selector: Auto, Force HW4, Force HW3, Force Legacy.

## v2.15

- Nag killer fires a grip pulse as soon as the car asks for hands on the wheel.
- Warning for Tesla firmware 2026.14 and newer, where transmitting can drop Autopilot.
- Ban Shield renamed to GTW Config Replay to describe what it really does.

## Older versions

Full history: https://github.com/hypery11/flipper-tesla-fsd/blob/main/changelog.md
