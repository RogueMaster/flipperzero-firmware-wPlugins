# Multi-Sensor Support Design
**Date:** 2026-02-14
**Status:** Draft for Approval

## Overview

Add support for TMP119 sensor and Temptress dual-sensor implant to VK Thermo Flipper application.

## Sensor Specifications

### Single-Sensor Implants (VivoKey Thermo)

| Sensor | I2C Address | Device ID (0x0F) | Temperature Conversion | One-Shot Configuration |
|--------|------------|------------------|----------------------|----------------------|
| **TMP112** | 0x48 | No register 0x0F | `(raw >> 4) * 0.0625` | Config[SD=1, OS=1] |
| **TMP117** | 0x48 | 0x0117 | `raw * 0.0078125` | Config[MOD=11] = 0xC00 |
| **TMP119** | 0x48 | 0x0119 | `raw * 0.0078125` | Config[MOD=11] = 0xC00 |

**Configuration Register:** 0x01 for all sensors

### Dual-Sensor Implant (Temptress)

- **Sensors:** Two TMP117
- **I2C Addresses:** 0x49 and 0x4A
- **Device ID:** 0x0117 (both)
- **Temperature Conversion:** `raw * 0.0078125` (each)
- **One-Shot Configuration:** Config[MOD=11] = 0xC00

**Unique behavior:**
- Poll both sensors with same timestamp
- Store both temperatures separately in CSV
- Display average on graph (with option to show both)

---

## Architecture Changes

### 1. Sensor Detection Strategy

**Multi-address probing:**
```
For each address in [0x48, 0x49, 0x4A]:
  1. Set register pointer to Device ID (0x0F)
  2. Read 2 bytes
  3. If successful and value matches known ID → identify sensor type
  4. If NAK/fail → assume TMP112 if address is 0x48
```

**Sensor identification:**
- **TMP119**: Device ID = 0x0119
- **TMP117**: Device ID = 0x0117
- **TMP112**: No Device ID register (NAK on 0x0F)
- **Temptress**: Two sensors at 0x49 and 0x4A, both return 0x0117

**Device type determination:**
```
if (sensor_at_0x49 AND sensor_at_0x4A):
    device_type = DeviceTypeTemptress
elif (sensor_at_0x48):
    device_type = DeviceTypeVkThermo
else:
    error: No compatible sensor found
```

---

### 2. Data Model Changes

**Current structure (single temperature):**
```c
typedef struct {
    uint8_t uid[VK_THERMO_NFC_UID_LEN];
    float temperature_celsius;
    VkThermoSensorType sensor_type;
    bool valid;
} VkThermoNfcData;
```

**Proposed structure (dual temperature support):**
```c
typedef enum {
    DeviceTypeVkThermo,     // VivoKey Thermo (single sensor: TMP112/117/119)
    DeviceTypeTemptress,    // Temptress (dual TMP117 sensors) - NOT a VivoKey product
} DeviceType;

typedef struct {
    uint8_t uid[VK_THERMO_NFC_UID_LEN];
    DeviceType device_type;
    VkThermoSensorType sensor_type;  // TMP112/117/119

    // Temperature data
    float temperature_celsius;        // Primary/average temp
    float temperature2_celsius;       // Secondary temp (Temptress only)
    bool has_dual_temps;             // true for Temptress

    bool valid;
} ThermoNfcData;
```

**Log entry structure:**
```c
typedef struct {
    uint8_t uid[VK_THERMO_UID_LENGTH];
    uint32_t timestamp;
    float temperature_celsius;        // Primary/average
    float temperature2_celsius;       // Secondary (0.0 if N/A)
    bool has_dual_temps;
    bool valid;
} VkThermoLogEntry;
```

---

### 3. CSV Format Changes

**Current format (single temperature):**
```csv
timestamp,celsius,fahrenheit
1706918400,36.52,97.74
```

**New format (dual temperature support):**
```csv
timestamp,celsius,fahrenheit,celsius2,fahrenheit2
1706918400,36.52,97.74,0.00,0.00
1706918460,36.48,97.66,36.51,97.72
```

**Notes:**
- Second temp columns = 0.00 for single-sensor implants
- Both temps present for Temptress
- Backwards compatible: old CSVs load correctly (missing columns default to 0)

---

### 4. Single-Shot Mode Implementation

**Before each temperature read, configure sensor for one-shot conversion:**

**TMP112:**
```c
// Step 1: Write config register (0x01): SD=1, OS=1
// NOTE: Device returns to shutdown after each conversion - must reconfigure before each read
uint8_t config[2] = {0x81, 0x00};  // SD=1 (bit 8), OS=1 (bit 7)
vk_thermo_nfc_i2c_write(poller, uid, 0x48, {0x01, config[0], config[1]}, 3);

// Step 2: Poll config register until OS bit = 0 (conversion complete)
do {
    vk_thermo_nfc_i2c_read_register(poller, uid, 0x48, 0x01, config, 2);
} while (config[0] & 0x80);  // OS bit

// Step 3: Read temperature from register 0x00
```

**TMP117 / TMP119:**
```c
// Step 1: Write config register (0x01): MOD[1:0]=11 (one-shot)
// NOTE: Device returns to shutdown after each conversion - must reconfigure before each read
uint8_t config[2] = {0x0C, 0x00};  // MOD=11 in bits 10-11 (0xC00)
vk_thermo_nfc_i2c_write(poller, uid, addr, {0x01, config[0], config[1]}, 3);

// Step 2: Poll config register until Data_Ready = 1 (bit 13)
do {
    vk_thermo_nfc_i2c_read_register(poller, uid, addr, 0x01, config, 2);
    uint16_t config_word = (config[0] << 8) | config[1];
} while (!(config_word & 0x2000));  // Data_Ready bit

// Step 3: Read temperature from register 0x00
```

**Temptress (both sensors):**
```c
// Step 1: Configure both sensors in parallel (no wait between writes)
uint8_t config[2] = {0x0C, 0x00};
vk_thermo_nfc_i2c_write(poller, uid, 0x49, {0x01, config[0], config[1]}, 3);
vk_thermo_nfc_i2c_write(poller, uid, 0x4A, {0x01, config[0], config[1]}, 3);

// Step 2: Poll both until Data_Ready on both sensors
while (!is_ready(0x49) || !is_ready(0x4A)) {
    furi_delay_ms(5);
}

// Step 3: Read both temps with same timestamp
temp1 = read_temp(0x49);
temp2 = read_temp(0x4A);
timestamp = furi_hal_rtc_get_timestamp();
```

---

### 5. NFC Reading Flow (Updated)

**New sensor reading logic:**

```
1. Enable Energy Harvesting (same as current)

2. Sensor Detection Phase:
   for addr in [0x48, 0x49, 0x4A]:
       try_read_device_id(addr) → store results

   determine_implant_type(detected_sensors)

3. Single-Shot Configuration:
   if (Temptress):
       configure_one_shot(0x49)
       configure_one_shot(0x4A)
   else:
       configure_one_shot(0x48)

4. Temperature Reading:
   if (Temptress):
       temp1 = read_temp(0x49)
       temp2 = read_temp(0x4A)
       timestamp = now()
       data.temperature_celsius = (temp1 + temp2) / 2
       data.temperature2_celsius = temp2  // or temp1, store both
       data.has_dual_temps = true
   else:
       temp = read_temp(0x48)
       data.temperature_celsius = apply_conversion(temp, sensor_type)
       data.has_dual_temps = false

5. Store to log with timestamp
```

---

### 6. Graph Display Logic

**Current:** Single line graph per UID

**Updated:**

**For single-sensor implants (VK Thermo):**
- Display single temperature line (no change)

**For Temptress:**
- **Default view:** Display average of both sensors
- **Optional:** Comparison mode toggle shows both sensor lines
  - Sensor 1: Solid line
  - Sensor 2: Dashed line
  - Average: Dotted line (optional)

**Implementation:**
```c
if (entry->has_dual_temps) {
    float avg = entry->temperature_celsius;  // Already averaged
    if (comparison_mode) {
        // Calculate original temps from average and stored temp2
        float temp1 = 2 * avg - entry->temperature2_celsius;
        float temp2 = entry->temperature2_celsius;
        draw_temp_line(temp1, LineStyleSolid);
        draw_temp_line(temp2, LineStyleDashed);
    } else {
        draw_temp_line(avg, LineStyleSolid);
    }
} else {
    draw_temp_line(entry->temperature_celsius, LineStyleSolid);
}
```

---

### 7. Debug Logging

**Device identification should only appear in debug logs:**

Current behavior:
```c
FURI_LOG_I(TAG, "Sensor: %s, Raw: 0x%04X, Temp: %.2f C",
    sensor == VkThermoSensorTmp117 ? "TMP117" : "TMP112",
    (uint16_t)raw, (double)celsius);
```

**Updated behavior:**
```c
// Only log device details in debug mode
if (debug_mode) {
    FURI_LOG_D(TAG, "Implant: %s, Sensor(s): %s at addr 0x%02X",
        implant_type_str(implant_type),
        sensor_type_str(sensor_type),
        i2c_address);
}

// Always log temperature result
FURI_LOG_I(TAG, "Temperature: %.2f°C", (double)celsius);
```

**Device names (debug only):**
- "VK Thermo (TMP112)"
- "VK Thermo (TMP117)"
- "VK Thermo (TMP119)"
- "Temptress (dual TMP117)"

---

## Migration Strategy

### CSV File Migration

**Auto-migration on first load:**
```c
// Detect old format (3 columns)
if (columns == 3) {
    // timestamp,celsius,fahrenheit
    entry.temperature_celsius = celsius;
    entry.temperature2_celsius = 0.0f;
    entry.has_dual_temps = false;
}
// New format (5 columns)
else if (columns == 5) {
    // timestamp,celsius,fahrenheit,celsius2,fahrenheit2
    entry.temperature_celsius = celsius;
    entry.temperature2_celsius = celsius2;
    entry.has_dual_temps = (celsius2 != 0.0f);
}
```

**Save in new format:**
```c
fprintf(file, "%lu,%.2f,%.2f,%.2f,%.2f\n",
    entry->timestamp,
    entry->temperature_celsius,
    celsius_to_fahrenheit(entry->temperature_celsius),
    entry->has_dual_temps ? entry->temperature2_celsius : 0.0f,
    entry->has_dual_temps ? celsius_to_fahrenheit(entry->temperature2_celsius) : 0.0f);
```

---

## UI/UX Considerations

### Scan View

**Display during read:**
- "Reading..." (same as current)
- Subtitle: "Tag found, please hold still" (same)

**Display after successful read:**
- Single sensor: "36.5°C" (no change)
- Temptress: "36.5°C" (displays average, no visual distinction)

**Device identification:**
- **Not shown in UI** (users see implant by UID)
- Only in debug logs

### Log View

**Entry display:**
```
Single sensor:
  36.52°C / 97.74°F

Temptress:
  36.52°C / 97.74°F  ← average displayed
  (36.48°C / 36.56°C)  ← both temps in parentheses
```

### Graph View

**Default:**
- Displays average for Temptress (looks like single-sensor)

**Comparison mode toggle (OK button):**
- For Temptress: Shows both sensor lines + average
- For single-sensor: No change (toggle does nothing)

---

## Testing Requirements

### Before Release:

1. **Hardware testing** with actual implants:
   - ✅ TMP112 (VivoKey Thermo v1)
   - ⏳ TMP117 (VivoKey Thermo v2) - need hardware
   - ⏳ TMP119 (VivoKey Thermo v3) - need hardware
   - ⏳ Temptress - need hardware

2. **CSV compatibility:**
   - ✅ Load old 3-column CSV
   - ⏳ Load new 5-column CSV
   - ⏳ Save new format correctly

3. **Sensor detection:**
   - ⏳ Correctly identify each sensor type
   - ⏳ Correctly identify Temptress vs single TMP117
   - ⏳ Handle missing sensors gracefully

4. **Single-shot mode:**
   - ⏳ Verify conversion completes before read
   - ⏳ Verify temperature accuracy

5. **Graph display:**
   - ⏳ Average displayed correctly for Temptress
   - ⏳ Comparison mode shows both sensors

---

## Implementation Checklist

### Phase 1: Core Sensor Support
- [ ] Add TMP119 sensor type enum
- [ ] Add Temptress implant type enum
- [ ] Update VkThermoNfcData structure (dual temps)
- [ ] Update VkThermoLogEntry structure
- [ ] Implement multi-address sensor detection
- [ ] Implement sensor type identification
- [ ] Implement implant type determination

### Phase 2: Single-Shot Mode
- [ ] Implement TMP112 one-shot configuration
- [ ] Implement TMP117/119 one-shot configuration
- [ ] Add conversion wait delays
- [ ] Integrate into existing read flow

### Phase 3: Dual-Sensor Support
- [ ] Implement dual-sensor reading (Temptress)
- [ ] Calculate and store average
- [ ] Store both temperatures
- [ ] Use same timestamp for both readings

### Phase 4: Storage Updates
- [ ] Update CSV format (5 columns)
- [ ] Implement CSV migration logic
- [ ] Update log save/load functions
- [ ] Test backwards compatibility

### Phase 5: UI Updates
- [ ] Update log view (show both temps for Temptress)
- [ ] Update graph averaging logic
- [ ] Implement graph comparison mode toggle
- [ ] Update debug logging

### Phase 6: Testing
- [ ] Test with TMP112 hardware
- [ ] Test with TMP117 hardware (if available)
- [ ] Test with TMP119 hardware (if available)
- [ ] Test with Temptress hardware (if available)
- [ ] Test CSV migration
- [ ] Verify cross-firmware compatibility

---

## Open Questions

1. **Temptress temperature storage - which temp goes in which field?**
   - Option A: temp1 → temperature_celsius, temp2 → temperature2_celsius
   - Option B: average → temperature_celsius, temp2 → temperature2_celsius
   - **Recommendation:** Option B (average in primary field for consistency)

2. **Graph comparison mode for Temptress:**
   - Should it be a separate toggle or tied to existing comparison mode?
   - **Recommendation:** Use existing OK button toggle, only active for Temptress

3. **CSV column order:**
   - Keep fahrenheit columns or switch to celsius-only?
   - **Recommendation:** Keep current pattern (celsius, fahrenheit, celsius2, fahrenheit2)

4. **Sensor detection timeout:**
   - How long to wait for device ID read?
   - **Recommendation:** 10ms per address (same as current I2C timeout)

5. **Single-shot conversion wait time:**
   - Use fixed delay or poll for completion?
   - **Recommendation:** Fixed delay (15.5ms for TMP117/119, 26ms for TMP112)

---

## Risk Assessment

### Low Risk:
- ✅ TMP119 is identical to TMP117 (same registers, same conversion)
- ✅ All sensors share I2C protocol (no new commands)
- ✅ CSV format is backwards compatible

### Medium Risk:
- ⚠️ Sensor detection logic (need robust multi-address probing)
- ⚠️ Single-shot timing (must wait for conversion)
- ⚠️ Dual temperature data flow (new code paths)

### High Risk:
- ❌ No hardware for testing TMP117/119/Temptress
- ❌ Temptress detection ambiguity (two TMP117 vs one)

### Mitigation:
- Use existing TMP112 for initial development
- Add detailed debug logging for sensor detection
- Keep existing single-sensor code path unchanged
- Add feature flag for Temptress support (disable if issues found)

---

## Alternative Approaches Considered

### 1. Separate data structures for single vs dual sensor
**Rejected:** Increases complexity, harder to maintain

### 2. Store raw temps instead of average
**Rejected:** Breaks existing UI assumptions, user sees different values

### 3. Auto-detect mode (continuous vs one-shot)
**Rejected:** Always use one-shot for consistency and power efficiency

### 4. Show both temps in main display for Temptress
**Rejected:** Confusing for users, average is more useful

---

## Success Criteria

**Must have:**
- ✅ Detects and reads all four sensor types
- ✅ Single-shot mode works correctly
- ✅ CSV format supports dual temperatures
- ✅ Backwards compatible with existing CSVs
- ✅ Compiles on Official/Unleashed/Momentum
- ✅ Debug logging shows device identification

**Nice to have:**
- ⭐ Graph comparison mode for Temptress
- ⭐ Hardware testing on all sensor types
- ⭐ Unit tests for sensor detection logic

**Out of scope:**
- ❌ User-configurable sensor types
- ❌ Manual sensor calibration
- ❌ Historical data format conversion tool
