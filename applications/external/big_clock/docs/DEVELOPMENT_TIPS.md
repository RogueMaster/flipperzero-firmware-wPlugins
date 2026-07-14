# Flipper Zero App Development Tips & Tricks

Lessons learned from developing Big Clock and Reality Dimension Clock.

## Backlight & Brightness Control

### The Problem

Flipper Zero's notification system aggressively manages backlight. When you try to control brightness directly:

```c
// NAIVE APPROACH - causes flicker!
furi_hal_light_set(LightBacklight, brightness_value);
```

This gets overridden by the system on:
- Any input event (button press/release)
- Notification messages
- System timers

Even using `sequence_display_backlight_enforce_on` doesn't help because it locks the backlight to the **system's** brightness setting, not yours.

### The Solution: NightStand Clock Approach

The NightStand Clock (by @nymda) figured out the correct approach: modify the notification app's internal brightness setting, then trigger a backlight update.

**Step 1: Define internal structures**

The `NotificationApp` struct isn't exposed in the public FAP API. Define it yourself based on the firmware source:

```c
/* Based on flipperdevices/flipperzero-firmware notification_app.h */

typedef struct {
    uint8_t value_last[2];
    uint8_t value[2];
    uint8_t index;
    uint8_t light;
} NotificationLedLayer;

typedef struct {
    uint8_t version;
    float display_brightness;    // <-- This is what we need!
    float led_brightness;
    float speaker_volume;
    uint32_t display_off_delay_ms;
    int8_t contrast;
    bool vibro_on;
} NotificationSettings;

typedef struct {
    FuriMessageQueue* queue;
    FuriPubSub* event_record;
    FuriTimer* display_timer;
    NotificationLedLayer display;
    NotificationLedLayer led[3];
    uint8_t display_led_lock;
    NotificationSettings settings;  // <-- Access via this
} NotificationAppInternal;
```

**Step 2: Cast and access**

```c
// In your app state struct:
typedef struct {
    // ... your fields ...
    NotificationAppInternal* notification;
    float original_brightness;  // Save to restore on exit
} MyAppState;

// When opening notification service:
state->notification = (NotificationAppInternal*)furi_record_open(RECORD_NOTIFICATION);
state->original_brightness = state->notification->settings.display_brightness;
```

**Step 3: Set brightness properly**

```c
static void set_brightness(MyAppState* state, uint8_t percent) {
    // Set the internal setting (0.0 to 1.0)
    state->notification->settings.display_brightness = (float)percent / 100.0f;

    // Trigger backlight update - system now uses OUR value
    notification_message((NotificationApp*)state->notification, &sequence_display_backlight_on);
}
```

**Step 4: Cleanup on exit**

```c
// Restore original brightness
state->notification->settings.display_brightness = state->original_brightness;

// Return to auto mode (respects system timeout)
notification_message((NotificationApp*)state->notification, &sequence_display_backlight_enforce_auto);

furi_record_close(RECORD_NOTIFICATION);
```

### What NOT to do

| Approach | Problem |
|----------|---------|
| `furi_hal_light_set()` alone | System overrides on input events |
| High-frequency timer to reapply | Wastes CPU, still flickers on button release |
| `enforce_on` + `furi_hal_light_set()` | enforce_on uses system brightness, ignores your setting |
| Reapply in input callback | Runs after system already reset it |

## Always-On Backlight

For clock/display apps where you want backlight always on:

```c
// At startup - lock backlight on
notification_message((NotificationApp*)state->notification, &sequence_display_backlight_enforce_on);

// At exit - return to auto (respects user's timeout setting)
notification_message((NotificationApp*)state->notification, &sequence_display_backlight_enforce_auto);
```

**Note:** `enforce_on` increments a lock counter, `enforce_auto` decrements it. They must be balanced.

## Code Organization

### Recommended Structure

```c
/* ============================================================================
 * INCLUDES
 * ============================================================================ */

/* ============================================================================
 * INTERNAL STRUCTURES (if accessing firmware internals)
 * ============================================================================ */

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/* ============================================================================
 * TYPES
 * ============================================================================ */

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/* ============================================================================
 * DRAWING FUNCTIONS
 * ============================================================================ */

/* ============================================================================
 * INPUT HANDLING
 * ============================================================================ */

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */
```

### State Management

Keep all mutable state in a single struct:

```c
typedef struct {
    bool is_running;

    // UI state
    uint8_t current_screen;
    int8_t scroll_offset;

    // App-specific data
    // ...

    // System handles (if needed for cleanup)
    NotificationAppInternal* notification;
    float original_brightness;
} MyAppState;

static MyAppState* state_alloc(void) {
    MyAppState* state = malloc(sizeof(MyAppState));
    memset(state, 0, sizeof(MyAppState));

    state->is_running = true;
    // Initialize defaults...

    return state;
}

static void state_free(MyAppState* state) {
    free(state);
}
```

## Input Handling

### Keep Callbacks Simple

```c
// Input callback just queues events - no logic here
static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}
```

### Process in Main Loop

```c
static void process_input(MyAppState* state, InputEvent* event) {
    // Filter event types early
    if(event->type != InputTypePress && event->type != InputTypeRepeat) {
        return;
    }

    // Handle based on current screen/state
    switch(state->current_screen) {
        case SCREEN_MENU:
            handle_menu_input(state, event);
            break;
        // ...
    }
}
```

## Rolling Buffers for Stable Readings

For sensor data that fluctuates, use rolling buffers:

```c
#define BUFFER_SIZE 1000

typedef struct {
    float values[BUFFER_SIZE];
    uint16_t write_idx;
    uint16_t count;
    float sum;  // Running sum for O(1) average
} RollingBuffer;

static void buffer_add(RollingBuffer* buf, float value) {
    if(buf->count >= BUFFER_SIZE) {
        buf->sum -= buf->values[buf->write_idx];  // Remove old
    } else {
        buf->count++;
    }

    buf->values[buf->write_idx] = value;
    buf->sum += value;
    buf->write_idx = (buf->write_idx + 1) % BUFFER_SIZE;
}

static float buffer_average(RollingBuffer* buf) {
    return buf->count > 0 ? buf->sum / (float)buf->count : 0.0f;
}
```

## Hardware Access

### SubGHz Radio

```c
// Wake radio from sleep
furi_hal_subghz_reset();
furi_hal_subghz_idle();

// Set frequency and read RSSI
furi_hal_subghz_set_frequency_and_path(433920000);  // 433.92 MHz
furi_hal_subghz_rx();
furi_delay_us(500);  // Let RSSI stabilize
float rssi = furi_hal_subghz_get_rssi();
furi_hal_subghz_idle();

// On exit - put to sleep
furi_hal_subghz_sleep();
```

### Internal Temperature Sensor

```c
// Acquire ADC handle
FuriHalAdcHandle* adc = furi_hal_adc_acquire();

// Configure for temperature (needs slow sampling)
furi_hal_adc_configure_ex(
    adc,
    FuriHalAdcScale2048,
    FuriHalAdcClockSync64,
    FuriHalAdcOversample64,
    FuriHalAdcSamplingtime247_5);

// Read temperature
uint16_t raw = furi_hal_adc_read(adc, FuriHalAdcChannelTEMPSENSOR);
float temp_c = furi_hal_adc_convert_temp(adc, raw);

// Release when done
furi_hal_adc_release(adc);
```

## Common Pitfalls

1. **Don't forget to close records** - Every `furi_record_open()` needs `furi_record_close()`
2. **Don't forget to free memory** - malloc → free, alloc → free
3. **Balance notification locks** - `enforce_on` and `enforce_auto` must pair up
4. **Check for NULL** - Especially for optional handles like ADC
5. **Use UNUSED()** - For callback parameters you don't use: `UNUSED(ctx);`

## Build System

Always use Poetry in this repo:

```bash
# Build
poetry run python -m ufbt

# Build + install + run
poetry run python -m ufbt launch

# Clean build artifacts
poetry run python -m ufbt -c
```

Or use the just commands:

```bash
just build reality-clock
just install reality-clock
just clean reality-clock
```

## References

- [NightStand Clock](https://github.com/nymda/FlipperNightStand) - Original brightness fix approach
- [Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware) - Official source
- [notification_app.h](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/applications/services/notification/notification_app.h) - Internal structures
