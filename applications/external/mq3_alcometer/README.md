# MQ-3 Alcohol Meter for Flipper Zero

Breathalyzer application for Flipper Zero using MQ-3 alcohol sensor with automatic zero calibration and intoxication level detection.

## Features

- **Automatic Zero Calibration** - Self-calibrates during warmup, adapts to any MQ-3 sensor
- **5-Level Intoxication Scale** - Sober, Light, Medium, Heavy, Critical
- **Peak Detection** - Captures maximum value during measurement
- **Sober Time Estimation** - Approximate time until fully sober
- **Measurement Log** - Saves results to SD card with date/time
- **Log Viewer** - Browse measurement history with scrolling
- **Heater Management** - Auto-shutdown after 60s idle
- **Voltage Filtering** - Ring buffer + EMA smoothing for stable readings
- **Vibration Alerts** - Tactile feedback for each intoxication level

## Hardware Connection

| MQ-3 Module | Flipper Zero GPIO |
|-------------|-------------------|
| VCC (5V)    | Pin 1 (OTG 5V)    |
| GND         | Pin 8 (GND)       |
| AOUT        | Pin 4 (PC3/ADC)   |
| HEATER      | Pin 2 (PA7)       |

**Important:** Module includes 10kΩ + 9kΩ trimmer voltage divider. AOUT signal must not exceed 2.048V.

## Installation

1. Download the `.fap` file from [Releases](https://github.com/serjantlk/MQ-3-Alcometer/releases)
2. Copy to `apps/GPIO/` on your Flipper Zero SD card using qFlipper
. Launch from `Apps → GPIO → MQ-3 Alcometer`

## Usage

### Controls

| Button | Action |
|--------|--------|
| OK | Start warmup / Begin measurement / New test |
| BACK | Return to main screen / Exit |
| LEFT | View measurement log (from main or result screen) |
| UP/DOWN | Scroll log entries |

### Workflow

1. Press **OK** to start sensor warmup (30 seconds)
2. Wait for warmup to complete (progress bar fills)
3. Press **OK** to begin 10-second measurement
4. Blow into the sensor during measurement
5. View result with peak concentration and sober time
6. Press **OK** for new measurement or **BACK** to exit

### Log Viewer

- Access from main screen or result screen with **LEFT** button
- Scroll with **UP/DOWN** buttons
- Clear log with **OK** button (confirmation required)
- Log format: `DD.MM.YY HH:MM - X.XXX mg/l`
- File location: `/ext/apps_data/alcohol_sensor/measuring.log`

## Technical Details

### Calibration

Zero calibration (Z) occurs during the last 5 seconds of warmup using Exponential Moving Average (EMA):
Z = α × V_new + (1 - α) × Z_old
α = 0.05

R0 calculated as: `R0 = (Vc × R2 / Z) - R1 - R2`

### Concentration Formula

Based on MQ-3 datasheet power function:
log10(ppm) = (log10(Rs/R0) - 0.62) / -0.66
mg/L = ppm × 0.00188


### Signal Processing

- 20 samples per measurement (5ms interval)
- Ring buffer of 20 measurements for voltage averaging
- Noise filter: values < 0.05V discarded
- EMA smoothing for mg/L: α = 0.15
- Dead zone: ±1% around Z or < 0.01V difference

### Intoxication Levels

| Level | mg/L | Alert |
|-------|------|-------|
| Sober (S) | < 0.16 | None |
| Light (L) | 0.16 - 0.50 | 1 vibration |
| Medium (M) | 0.50 - 1.00 | 2 vibrations |
| Heavy (H) | 1.00 - 2.00 | 3 vibrations |
| Critical (C) | > 2.00 | 5 vibrations |

## Building from Source

```bash
git clone https://github.com/serjantlk/MQ-3-Alcometer.git
cd MQ-3-Alcometer
ufbt
