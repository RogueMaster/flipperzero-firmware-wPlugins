# Flipper Zero — HID API Reference (Dual Transport)

> By Kasen Sansonetti & the Wetware Labs team. WetwareOfficial.com
>
> Sources:
> - https://docs.flipper.net/zero/apps/controllers
> - Flipper firmware headers: `furi_hal_usb_hid.h`, `extra_profiles/hid_profile.h`

## Overview

Flipper Zero can act as a USB or Bluetooth LE (BLE) HID keyboard. Claude Remote ships two builds from one codebase:

| Transport | API | Firmware | Build Define |
|-----------|-----|----------|-------------|
| USB HID | `furi_hal_hid_kb_*` | Stock, Momentum, Unleashed | `HID_TRANSPORT_USB` |
| BLE HID | `ble_profile_hid_kb_*` | Momentum, Unleashed only | `HID_TRANSPORT_BLE` |

Stock firmware exposes only USB HID functions (`+` in `api_symbols.csv`). BLE HID functions are marked `-` (restricted) but accessible on custom firmware via `fap_libs=["ble_profile"]`.

---

## USB HID Transport

### Required Includes

```c
#include <furi_hal_usb_hid.h>
```

### application.fam

```python
requires=["gui"],
cdefines=["HID_TRANSPORT_USB"],
```

### Setup and Teardown

```c
// Save previous USB config, switch to HID
FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
furi_hal_usb_unlock();
furi_hal_usb_set_config(&usb_hid, NULL);

// ... app runs ...

// Cleanup: release all keys, restore previous USB config
furi_hal_hid_kb_release_all();
furi_hal_usb_set_config(usb_prev, NULL);
```

### Connection Check

```c
bool connected = furi_hal_hid_is_connected();
```

### Sending Keys

```c
static void send_hid_key(uint16_t keycode) {
    furi_hal_hid_kb_press(keycode);
    furi_delay_ms(50);
    furi_hal_hid_kb_release(keycode);
}
```

---

## BLE HID Transport

### Required Includes

```c
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
```

### application.fam

```python
requires=["gui", "bt"],
cdefines=["HID_TRANSPORT_BLE"],
fap_libs=["ble_profile"],
```

The `fap_libs=["ble_profile"]` directive links the BLE profile library, making `ble_profile_hid_*` functions available even though they're marked `-` in `api_symbols.csv`. This only works on custom firmware (Momentum, Unleashed) that includes the `ble_profile` library for external FAPs.

### Setup and Teardown

```c
// Disconnect existing BT, start HID profile
Bt* bt = furi_record_open(RECORD_BT);
bt_disconnect(bt);
furi_delay_ms(200);
FuriHalBleProfileBase* ble_profile = bt_profile_start(bt, ble_profile_hid, NULL);

// ... app runs ...

// Cleanup: release all keys, restore default BT profile
ble_profile_hid_kb_release_all(ble_profile);
bt_profile_restore_default(bt);
furi_record_close(RECORD_BT);
```

### Connection Check

```c
bool connected = ble_profile_hid_is_connected(ble_profile);
```

### Sending Keys

```c
static void send_hid_key(FuriHalBleProfileBase* profile, uint16_t keycode) {
    ble_profile_hid_kb_press(profile, keycode);
    furi_delay_ms(50);
    ble_profile_hid_kb_release(profile, keycode);
}
```

---

## Compile-Time Transport Switching

The app uses `#ifdef HID_TRANSPORT_BLE` / `#ifdef HID_TRANSPORT_USB` to select the correct API calls at compile time. Both builds share the same `.c` file with two entry points:

```c
int32_t claude_remote_usb_app(void* p) { return claude_remote_main(p); }
int32_t claude_remote_ble_app(void* p) { return claude_remote_main(p); }
```

---

## Key Codes for Claude Remote

| Logical Action | Keycode Constant | Notes |
|----------------|------------------|-------|
| Option 1 (Yes) | `HID_KEYBOARD_1` | Numeral 1 |
| Option 2 (No) | `HID_KEYBOARD_2` | Numeral 2 |
| Option 3 (Other) | `HID_KEYBOARD_3` | Numeral 3 |
| Enter / Submit | `HID_KEYBOARD_RETURN` | Return key |
| Voice / Dictation | `HID_KEYBOARD_F5` | macOS dictation shortcut (configurable) |

### Full HID Keycode Reference

Common keycodes from USB HID spec (available in Flipper headers):

```c
HID_KEYBOARD_A through HID_KEYBOARD_Z      // letters
HID_KEYBOARD_1 through HID_KEYBOARD_0      // numbers
HID_KEYBOARD_RETURN                         // Enter
HID_KEYBOARD_ESCAPE                         // Escape
HID_KEYBOARD_DELETE                         // Backspace
HID_KEYBOARD_TAB                            // Tab
HID_KEYBOARD_SPACEBAR                       // Space
HID_KEYBOARD_F1 through HID_KEYBOARD_F12   // function keys
HID_KEYBOARD_UP_ARROW                       // arrow keys
HID_KEYBOARD_DOWN_ARROW
HID_KEYBOARD_LEFT_ARROW
HID_KEYBOARD_RIGHT_ARROW
```

### Modifier Keys

```c
// USB transport
furi_hal_hid_kb_press(HID_KEYBOARD_L_CTRL);
furi_hal_hid_kb_press(HID_KEYBOARD_C);
furi_delay_ms(50);
furi_hal_hid_kb_release_all();

// BLE transport
ble_profile_hid_kb_press(profile, HID_KEYBOARD_L_CTRL);
ble_profile_hid_kb_press(profile, HID_KEYBOARD_C);
furi_delay_ms(50);
ble_profile_hid_kb_release_all(profile);
```

### Release All Keys

Always call on exit or error to avoid stuck keys:

```c
// USB
furi_hal_hid_kb_release_all();

// BLE
ble_profile_hid_kb_release_all(profile);
```

---

## Latency Considerations

- `furi_delay_ms(50)` between press and release is sufficient for most hosts.
- Total key send should be <100ms from physical button press.
- The event loop's 100ms queue timeout doesn't add input latency (events are processed immediately when available).

## Pairing (BLE only)

Pairing is handled by the Flipper OS, not the app:
1. App calls `bt_profile_start()` to switch BT to HID keyboard mode.
2. Host (Mac) discovers Flipper in **System Settings > Bluetooth**.
3. User confirms pairing on both devices.
4. After initial pairing, reconnection is automatic.
5. On app exit, `bt_profile_restore_default()` restores the serial profile.

The app just needs to check connection state via `ble_profile_hid_is_connected()`.
