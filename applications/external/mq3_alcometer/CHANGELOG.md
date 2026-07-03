
---

## CHANGELOG.md

```markdown
# Changelog

All notable changes to MQ-3 Alcohol Meter for Flipper Zero.

## [1.0.0] - 2026-06-09

### Added
- Initial release
- Automatic zero calibration (Z) during warmup
- 5-level intoxication scale with visual indicator
- Peak value detection with 10-second display
- Approximate sober time calculation
- Measurement log to SD card (`measuring.log`)
- Log viewer with scrolling (UP/DOWN)
- Log clear function with confirmation dialog
- Heater auto-shutdown after 60 seconds idle
- OTG 5V power management
- Voltage ring buffer averaging (20 samples)
- EMA smoothing for mg/L readings (α = 0.15)
- Dead zone filter (±1% around zero)
- Vibration alerts for each intoxication level
- Animated progress bar during measurement
- Warmup quality indicator (Cold/Warming/Ready/Optimal)
- Auto re-calibration when voltage drops below zero

### Technical
- MQ-3 datasheet formula: `log10(ppm) = (log10(Rs/R0) - 0.62) / -0.66`
- Voltage divider: R1=10kΩ + R2=9kΩ (trimmer)
- Zero calibration EMA: α = 0.05
- mg/L EMA smoothing: α = 0.15
- ADC sampling: 20 samples × 5ms
- Measurement duration: 10 seconds
- Warmup duration: 30 seconds
- Heater timeout: 60 seconds

### File Structure
MQ-3_Alcometer/
├── alcohol_sensor_app.c # Main application source
├── application.fam # Build manifest
├── README.md # Documentation
├── CHANGELOG.md # This file
└── LICENSE # MIT License