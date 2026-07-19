## v0.5

- Exit is Back-only again (USB unplug no longer leaves the app)
- Keep fast teardown: skip HID release without a host; disconnect BLE before restore; drain HID queue when USB is down
- Catalog README links the companion APK to the matching GitHub release

## v0.4

- Exit promptly when USB cable is unplugged (after a short debounce) — reverted in v0.5
- Faster teardown: skip HID release when host is gone; disconnect BLE before profile restore
- Drain HID queue on USB loss / exit so leftover frames cannot stall Back

## v0.3

- Catalog packaging polish: storage required only when screenshots are enabled
- Safer startup zero-init and clearer shared-state visibility across BLE/UI threads

## v0.2

- Mouse / touchpad events over the same BLE Serial protocol
- Status UI improvements (USB / phone / frame counters)
- Optional on-device screenshots via triple Down (AKB_SCREENSHOT=1 at build time)
- Up toggles forced backlight; Back exits

## v0.1

- Initial release: Android phone → BLE Serial → Flipper → USB HID keyboard
