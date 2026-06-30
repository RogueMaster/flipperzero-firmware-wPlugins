# Architecture Patterns & Critical Code Reference

This document contains essential code patterns and architecture details for the Flipper Wedge app. These patterns are critical to maintain - **do not remove or modify** without understanding the implications.

---

## Critical Code Patterns

### ⚠️ NEVER Remove These!

#### 1. NFC Auto-Restart on Error

**Location**: [helpers/hid_device_nfc.c](../helpers/hid_device_nfc.c)

**Pattern**:
```c
case NfcPollerEventTypeStopped:
    // Poller stopped (error or intentional)
    if (nfc->running) {
        // CRITICAL: Auto-restart if we're supposed to be running
        nfc_poller_start(nfc->poller, callback, context);
    }
    break;
```

**Why Critical**: NFC poller can fail unexpectedly. Without auto-restart, NFC stops working after first error. This pattern ensures resilient operation.

**DO NOT**:
- Remove the `if (nfc->running)` check
- Skip the `nfc_poller_start()` restart
- Change this to manual restart only

---

#### 2. HID Worker Thread Pattern

**Location**: [helpers/hid_device_hid_worker.c](../helpers/hid_device_hid_worker.c)

**Why Needed**: Bluetooth HID operations can block for 100ms+. Running HID in a separate worker thread prevents UI freezing during typing.

**Key Functions**:
- `hid_device_hid_worker_alloc()` - Creates worker thread
- `hid_device_hid_worker_start()` - Starts HID in background
- `hid_device_hid_worker_type_string()` - Queues typing (non-blocking)
- `hid_device_hid_worker_get_hid()` - Gets HID instance

**Thread Safety Pattern**:
```c
// CRITICAL: Always use message queue for UI ↔ worker communication
HidDeviceHidWorkerMessage msg = {
    .type = HidDeviceHidWorkerMessageTypeTypeString,
    .data = string_data
};
furi_message_queue_put(worker->queue, &msg, FuriWaitForever);
```

**DO NOT**:
- Call HID functions directly from main thread
- Skip the message queue
- Block the main thread waiting for HID operations

---

#### 3. Dynamic Output Mode Switching (USB ↔ BLE)

**Location**: [hid_device.c:195](../hid_device.c#L195) - `hid_device_switch_output_mode()`

**Pattern** (Must follow this sequence):
```c
// 1. Stop workers FIRST
hid_device_nfc_stop(app->nfc);
hid_device_rfid_stop(app->rfid);

// 2. Stop HID worker
hid_device_hid_worker_stop(app->hid_worker);

// 3. Deinit old HID profile
if (app->hid) {
    hid_device_hid_free(app->hid);
}

// 4. Switch mode in settings
app->output_mode = new_mode;

// 5. Init new HID profile
app->hid = hid_device_hid_alloc(app->output_mode);

// 6. Restart HID worker
hid_device_hid_worker_start(app->hid_worker);

// 7. Restart NFC/RFID workers
hid_device_nfc_start(...);
// or hid_device_rfid_start(...);
```

**Why Order Matters**: Workers access HID instance. Switching HID while workers are active causes crashes.

**DO NOT**:
- Skip stopping workers before HID switch
- Change the order of operations
- Restart workers before HID is ready

---

#### 4. Scan State Machine

**Location**: [hid_device.h:66](../hid_device.h#L66) - `HidDeviceScanState` enum

**States**:
```c
typedef enum {
    HidDeviceScanStateIdle,           // Not scanning
    HidDeviceScanStateScanning,       // Waiting for first tag
    HidDeviceScanStateWaitingSecond,  // Combo mode: waiting for second tag
    HidDeviceScanStateDisplaying,     // Showing result briefly
    HidDeviceScanStateCooldown        // Preventing re-scan of same tag
} HidDeviceScanState;
```

**State Transitions**:
```
Idle → Scanning (user starts scan)
Scanning → WaitingSecond (first tag found in combo mode)
Scanning → Displaying (tag found in single mode)
WaitingSecond → Displaying (second tag found)
WaitingSecond → Scanning (timeout after 5s)
Displaying → Cooldown (brief display complete)
Cooldown → Scanning (cooldown timer expires)
```

**Timers**:
- `timeout_timer` - 5s timeout for second tag in combo mode
- `display_timer` - Brief display before returning to scanning

**DO NOT**:
- Skip state transitions
- Remove timeout handling in combo modes
- Allow immediate re-scan without cooldown

---

## Settings Persistence Architecture

### Critical Pattern: Immediate Save in Callbacks

**Location**: [scenes/hid_device_scene_settings.c](../scenes/hid_device_scene_settings.c)

**ALWAYS Follow This Pattern**:
```c
static void setting_callback(VariableItem* item) {
    HidDevice* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // 1. Update UI
    variable_item_set_current_value_text(item, text_array[index]);

    // 2. Update app state
    app->your_setting = (YourSettingEnum)index;

    // 3. CRITICAL: Save immediately
    hid_device_save_settings(app);
}
```

**Why Critical**: If app exits abnormally (crash, home button), settings would be lost. Immediate save ensures persistence.

**DO NOT**:
- Rely only on Back button save (line 390) - it's a fallback only
- Batch settings saves
- Skip `hid_device_save_settings(app)` in callbacks

---

### Settings Storage Pattern

**Location**: [helpers/hid_device_storage.c](../helpers/hid_device_storage.c)

**Current Settings** (File Version 5):
- `delimiter` (string)
- `append_enter` (bool)
- `mode` (uint32)
- `mode_startup_behavior` (uint32)
- `output_mode` (uint32)
- `vibration_level` (uint32)
- `ndef_max_len` (uint32)
- `log_to_sd` (bool)

**Adding a New Setting** - Required Steps:

1. **Define in app struct** ([hid_device.h](../hid_device.h)):
   ```c
   typedef struct {
       YourSettingType your_new_setting;
   } HidDevice;
   ```

2. **Add storage key** ([helpers/hid_device_storage.h](../helpers/hid_device_storage.h)):
   ```c
   #define HID_DEVICE_SETTINGS_KEY_YOUR_SETTING "YourSetting"
   ```

3. **Initialize default** ([hid_device.c](../hid_device.c) in `hid_device_alloc()`):
   ```c
   app->your_new_setting = default_value;
   ```

4. **Save with error checking** ([helpers/hid_device_storage.c](../helpers/hid_device_storage.c)):
   ```c
   // In hid_device_save_settings()
   uint32_t your_setting = app->your_new_setting;
   if(!flipper_format_write_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &your_setting, 1)) {
       FURI_LOG_E(TAG, "Failed to write your_setting");
       ret = false;
       goto cleanup;  // Use cleanup pattern!
   }
   ```

5. **Load with validation** ([helpers/hid_device_storage.c](../helpers/hid_device_storage.c)):
   ```c
   // In hid_device_read_settings()
   uint32_t your_setting = default_value;
   if(flipper_format_read_uint32(fff_file, HID_DEVICE_SETTINGS_KEY_YOUR_SETTING, &your_setting, 1)) {
       // Validate range if it's an enum
       if(your_setting < YourSettingEnumCount) {
           app->your_new_setting = (YourSettingEnum)your_setting;
       }
   }
   ```

6. **Save immediately in callback** (see pattern above)

**CRITICAL**: Always check return values from `flipper_format_write_*` functions! They return `bool` and can fail silently.

**Lessons Learned**:
- Not saving immediately caused settings to reset unexpectedly
- Not checking write return values caused random settings to not persist
- Always use goto cleanup pattern for error handling

---

## Thread Safety Patterns

### View Model Updates

**ALWAYS use `with_view_model()` for thread-safe view updates**:

```c
with_view_model(
    view,
    HidDeviceStartscreenModel* model,
    {
        // Modify model here (thread-safe)
        model->status = "Scanning...";
        model->uid = uid_string;
    },
    true  // redraw flag
);
```

**DO NOT**:
- Access view model directly from callbacks
- Skip the `with_view_model()` wrapper
- Access GUI from worker threads

---

### Worker ↔ Main Thread Communication

**Pattern**: Use FuriMessageQueue

```c
// In worker thread
HidDeviceMessage msg = {.type = TypeSuccess, .data = result};
furi_message_queue_put(app->queue, &msg, FuriWaitForever);

// In main thread
HidDeviceMessage msg;
if(furi_message_queue_get(app->queue, &msg, 100) == FuriStatusOk) {
    // Process message safely in main thread
    handle_message(&msg);
}
```

**DO NOT**:
- Access app state directly from worker threads
- Call GUI functions from workers
- Use global variables for communication

---

## Memory Management Patterns

### Resource Cleanup Order

**Pattern**: Reverse allocation order

```c
// Allocation
app->nfc = hid_device_nfc_alloc();
app->rfid = hid_device_rfid_alloc();
app->hid_worker = hid_device_hid_worker_alloc();

// Cleanup (reverse order)
hid_device_hid_worker_free(app->hid_worker);
hid_device_rfid_free(app->rfid);
hid_device_nfc_free(app->nfc);
```

**Why**: Dependencies - HID worker uses NFC/RFID, so free worker first.

**CRITICAL**: Always stop workers before freeing:
```c
// ALWAYS stop before free
hid_device_nfc_stop(app->nfc);
hid_device_nfc_free(app->nfc);
```

---

### Dynamic Allocation for Temporary Buffers

**Pattern**: Allocate → Use → Free immediately

```c
// Type 4 NDEF reading
uint8_t* ndef_data = malloc(buffer_size);
if(!ndef_data) {
    FURI_LOG_E(TAG, "Failed to allocate NDEF buffer");
    return 0;
}

// Use buffer
size_t text_len = hid_device_nfc_parse_ndef_text(ndef_data, buffer_size, ...);

// Free immediately
free(ndef_data);
```

**DO NOT**:
- Keep large buffers allocated permanently
- Forget to free on error paths
- Allocate on stack for buffers > 256 bytes

---

## API Usage Patterns

### NFC Poller API

```c
// Create poller
NfcPoller* poller = nfc_poller_alloc(nfc_device, protocol);

// Start polling
nfc_poller_start(poller, callback, context);

// Handle events in callback
static NfcPollerEventType callback(void* context) {
    switch(event->type) {
        case NfcPollerEventTypeReady:
            // Tag detected
            break;
        case NfcPollerEventTypeRemoved:
            // Tag removed
            break;
        case NfcPollerEventTypeStopped:
            // Auto-restart pattern here!
            break;
    }
}

// Stop polling
nfc_poller_stop(poller);

// Free
nfc_poller_free(poller);
```

---

### RFID Worker API

```c
// Create worker
LfRfidWorker* worker = lfrfid_worker_alloc();

// Start reading
lfrfid_worker_start_read(worker, callback, context);

// Handle events in callback
static void callback(LfRfidWorkerEvent event, void* context) {
    if(event == LfRfidWorkerEventReadDone) {
        // Tag read complete
    }
}

// Stop
lfrfid_worker_stop(worker);

// Free
lfrfid_worker_free(worker);
```

---

### USB HID API

```c
// Enable USB HID mode
furi_hal_usb_set_config(&usb_hid, NULL);

// Check connection
if(furi_hal_usb_is_connected()) {
    // Type key
    furi_hal_hid_kb_press(HID_KEYBOARD_A);
    furi_delay_ms(1);  // Small delay
    furi_hal_hid_kb_release(HID_KEYBOARD_A);
}
```

---

### Bluetooth HID API

```c
// Get BT instance
Bt* bt = furi_record_open(RECORD_BT);

// Set BT HID profile
bt_set_profile(bt, BtProfileHidKeyboard);

// Check connection
if(furi_hal_bt_is_active()) {
    // Type key
    furi_hal_bt_hid_kb_press(HID_KEYBOARD_A);
    furi_delay_ms(1);
    furi_hal_bt_hid_kb_release(HID_KEYBOARD_A);
}

// Close record when done
furi_record_close(RECORD_BT);
```

---

## Common Pitfalls & Gotchas

### 1. NFC and RFID Cannot Poll Simultaneously

**Hardware limitation**: Only one radio can be active at a time.

**Pattern for combo modes**:
```c
// Start first reader
hid_device_nfc_start(app->nfc, false);

// When first tag found, stop NFC, start RFID
hid_device_nfc_stop(app->nfc);
hid_device_rfid_start(app->rfid);
```

---

### 2. HID Character Mapping

Some characters require Shift modifier:

```c
// Handle uppercase and special chars
if(c >= 'A' && c <= 'Z') {
    furi_hal_hid_kb_press(HID_KEYBOARD_LEFT_SHIFT);
    furi_hal_hid_kb_press(key);
    furi_hal_hid_kb_release(key);
    furi_hal_hid_kb_release(HID_KEYBOARD_LEFT_SHIFT);
} else {
    furi_hal_hid_kb_press(key);
    furi_hal_hid_kb_release(key);
}
```

---

### 3. Scene Manager Navigation

**Pattern**: Always use scene manager for navigation

```c
// Push new scene
scene_manager_next_scene(app->scene_manager, HidDeviceSceneSettings);

// Pop to previous scene
scene_manager_previous_scene(app->scene_manager);

// Clean up in on_exit handler
bool hid_device_scene_settings_on_event(void* context, SceneManagerEvent event) {
    // Handle events
}

void hid_device_scene_settings_on_exit(void* context) {
    // CRITICAL: Clean up scene resources here
    variable_item_list_reset(app->variable_item_list);
}
```

---

### 4. Notification Coordination

**Pattern**: Use all three feedback types for best UX

```c
// Success notification
hid_device_led_notify(app, true);       // Green LED
hid_device_haptic_notify(app, true);    // Vibration
hid_device_speaker_notify(app, true);   // Beep

// Failure notification
hid_device_led_notify(app, false);      // Red LED
hid_device_haptic_notify(app, true);    // Vibration (same as success)
hid_device_speaker_notify(app, false);  // Error beep
```

---

## File Structure Reference

```
flipper_wedge/
├── application.fam                 # App manifest (dependencies, version, API)
├── hid_device.c/h                  # Main app entry, lifecycle, state machine
├── helpers/
│   ├── hid_device_hid.c/h          # HID interface (USB/BT abstraction)
│   ├── hid_device_hid_worker.c/h   # HID worker thread (non-blocking)
│   ├── hid_device_nfc.c/h          # NFC worker, NDEF parsing (Type 2/4/5)
│   ├── hid_device_rfid.c/h         # RFID worker (EM4100, HID Prox, Indala)
│   ├── hid_device_format.c/h       # UID formatting, delimiter logic
│   ├── hid_device_storage.c/h      # Settings persistence (FlipperFormat API)
│   ├── hid_device_log.c/h          # SD card scan logging
│   ├── hid_device_haptic.c/h       # Vibration feedback
│   ├── hid_device_led.c/h          # LED feedback (green/red)
│   ├── hid_device_speaker.c/h      # Audio feedback (beep/error)
│   └── hid_device_debug.c/h        # Debug logging utilities
├── scenes/
│   ├── hid_device_scene.c/h        # Scene manager definitions
│   ├── hid_device_scene_menu.c     # Main menu scene (mode selection)
│   ├── hid_device_scene_startscreen.c  # Scanning screen scene (main UI)
│   ├── hid_device_scene_settings.c     # Settings screen scene
│   └── hid_device_scene_bt_pair.c      # BT pairing instructions scene
├── views/
│   └── hid_device_startscreen.c/h  # Custom scanning view (status, UID display)
└── icons/                          # PNG assets (converted to .icon by fbt)
```

---

## Related Documentation

- **[CLAUDE.md](../CLAUDE.md)** - Main maintenance guide
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Command reference and quick fixes
- **[FIRMWARE_COMPATIBILITY.md](FIRMWARE_COMPATIBILITY.md)** - Firmware versions and compatibility
- **[API_MIGRATION_LOG.md](API_MIGRATION_LOG.md)** - Historical API changes

---

*This document captures the essential architecture patterns that make the app work correctly. Preserve these patterns during maintenance and firmware updates.*
