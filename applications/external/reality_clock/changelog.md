# Changelog

All notable changes to Reality Clock will be documented in this file.

## [4.1] - 2026-01-24

**Fixed**
- **Brightness stability bug** - Fixed brightness reverting to 100% after ~1 hour of use
  - Root cause: Firmware notification system resets brightness after internal timeout
  - Solution: Periodic brightness reapplication every 60 seconds (power efficient)

**Changed**
- **Start at system brightness** - App now respects system backlight setting on startup
  - Previously forced 100% brightness when opened
  - Now reads and rounds to nearest 5% step
- **5% brightness increments** - Finer control with 21 levels (was 10% / 11 levels)

**Technical**
- Added `brightness_refresh_time` field for periodic refresh tracking
- Added `BRIGHTNESS_REFRESH_MS` constant (60 seconds)
- Removed initial `apply_brightness()` call to preserve system setting

---

## [4.0] - 2026-01-20

**Fixed**
- **Flicker-Free Brightness Control** - Completely rewrote brightness system
  - Now uses NightStand Clock approach (by @nymda)
  - Directly modifies notification app's internal brightness setting
  - No more 200Hz timer hack or interrupt-context brightness calls
  - Smooth, flicker-free brightness adjustment

**Changed**
- Simplified status text: "HOME DIMENSION" -> "HOME"
- Removed global brightness variable and timer callback
- Cleaner input callback (no brightness reapplication in interrupt context)
- Brightness now properly restored to original value on app exit

**Technical**
- Defined `NotificationAppInternal` struct to access `settings.display_brightness`
- Uses `sequence_display_backlight_on` to apply brightness after setting
- Uses `sequence_display_backlight_enforce_on` for always-on mode
- Properly paired with `sequence_display_backlight_enforce_auto` on exit

---

## [3.0] - 2026-01-20

**MAJOR: Real Hardware Sensors**

This release marks the transition from simulated readings to **real multi-band electromagnetic analysis**. The Reality Dimension Clock now uses actual Flipper Zero hardware to measure dimensional stability.

**Added**
- **Real SubGHz RSSI Measurement** - Three-band analysis using CC1101 radio
  - 315 MHz (Band 1) - Lower frequency reference point
  - 433.92 MHz (Band 2) - Mid frequency anchor
  - 868.35 MHz (Band 3) - Upper frequency reference
- **Internal Temperature Sensor** - STM32 ADC-based die temperature monitoring
- **Adaptive Baseline System** - Exponential Moving Average (EMA) tracking
  - Slow EMA (a=0.05) for long-term baseline adaptation
  - Fast EMA (a=0.15) for short-term trend detection
  - Baseline now tracks "current reality" - wherever you ARE is home
- **SD Card Logging** (optional, compile-time flag)
  - CSV format for data analysis
  - Timestamps, all RSSI values, PHI metrics, stability scores
  - Auto-sync every 100 samples
- **Enhanced Stability Algorithm**
  - Stability based on baseline tracking quality, not fixed reference
  - Sensor noise is NOT interpreted as dimensional instability
  - Only sudden jumps that baseline can't track indicate instability

**Changed**
- **No More Simulated Data** - All readings are real hardware measurements
- Removed fake LF/HF band simulation code
- PHI calculation uses normalized dB values to prevent underflow
- Stability thresholds adjusted for real-world sensor behavior:
  - HOME: >98% stability (very stable readings)
  - STABLE: >95% stability (mostly stable)
  - UNSTABLE: >90% stability (some fluctuation)
  - FOREIGN: <90% stability (significant deviation)

**Technical Details**
- SubGHz radio: `furi_hal_subghz_set_frequency_and_path()` for band switching
- RSSI acquisition: 500us stabilization delay per reading
- ADC: 64x oversampling, 247.5 cycle sampling time for accuracy
- Band mapping: 315->LF, 433->HF, 868->UHF (for display consistency)

**Why This Matters**

Previous versions simulated dimensional readings using entropy and timing jitter. Version 3.0 measures **actual electromagnetic propagation characteristics** across three distinct frequency bands. If fundamental physical constants (e0, u0, c) were to vary, the ratio between these bands would shift - and this device would detect it.

---

## [2.1] - 2026-01-19

**Added**
- **Settings Menu** - Press OK to open settings instead of direct calibration
  - CALIBRATE option to reset dimensional baseline
  - BRIGHTNESS slider (0-100%) with persistence during session
- **QR Code Info Screen** - Navigate right from Details to access
  - Scannable QR code linking to GitHub source repository
  - 2x scaled display for reliable scanning
- **Brightness Control** - Hardware backlight control via furi_hal_light_set
  - High-frequency (200Hz) timer to maintain brightness
  - Input callback reapplication to minimize flicker

**Known Issues**
- Brief brightness flicker may occur on button press/release due to Flipper Zero's notification system overriding direct hardware backlight control

## [2.0] - 2026-01-19

**Added**
- **Rolling 1000-sample buffer** for ultra-stable readings
  - Each band (LF, HF, UHF) maintains a circular buffer of last 1000 readings
  - PHI calculated from averaged buffer values, not raw readings
  - Guarantees consistent HOME DIMENSION status
- **Sci-Fi UI redesign** for homescreen
  - Decorative corner brackets
  - Large blocky "E-137" dimension number
  - Clean "[HOME DIMENSION]" status display
  - Horizontal accent lines
- **Dynamic sample rates**
  - 5Hz (200ms) during calibration for fast startup
  - 1Hz (1000ms) during normal operation

**Changed**
- Homescreen now shows only: title, E-137, and status (no clutter)
- Screens 1 (Home) and 2 (Bands) are NOT scrollable
- Only screen 3 (Details) supports scrolling
- Calibration requires 100 samples (20 seconds at 5Hz)
- Match thresholds: HOME >90%, STABLE >75%, UNSTABLE >50%

**Technical**
- State includes raw readings AND averaged readings
- Buffer uses running sum for O(1) average calculation
- Press OK to recalibrate (clears all buffers)

## [1.3] - 2026-01-19

**Added**
- New default DIMENSION screen showing large E-137 coordinate
- Three-screen navigation: DIMENSION (default) -> BANDS -> DETAILS

**Changed**
- Drastically reduced sensor fluctuation for stable HOME readings
  - LF band: +/-4dB -> +/-0.4dB
  - HF band: +/-3dB -> +/-0.3dB
  - UHF band: +/-5dB -> +/-0.5dB
  - IR band: +/-2.5dB -> +/-0.3dB
  - Temperature: +/-2C -> +/-0.2C
- Improved dimension status display with clear E-137 home indicator

## [1.2] - 2026-01-19

**Added**
- Rolling baseline system: 100-sample batches
- After each batch, baseline updates automatically
- Real battery voltage/current readings via FuelGauge IC
- Batch counter and total sample tracking

**Changed**
- Sample rate reduced to 1 per second for stability
- Thresholds: HOME >95%, STABLE >85%, DRIFT >70%, FOREIGN <70%

## [1.1] - 2026-01-19

**Added**
- Multi-screen interface (LEFT/RIGHT navigation)
- Scrollable details screen
- Status classification with generous thresholds

## [1.0] - 2025-01-19

**Added**
- Initial release
- Multi-band electromagnetic ratio analysis (LF/HF/UHF)
- Dimensional stability index (PHI) calculation
- Baseline calibration with 50-sample averaging
- Real-time drift percentage display
- Status classification (STABLE/FLUCTUATING/ANOMALY)
- Sub-GHz RSSI measurement via CC1101 radio
- Simulated LF and HF band readings
- Recalibration via OK button
- Academic paper documenting theoretical framework
