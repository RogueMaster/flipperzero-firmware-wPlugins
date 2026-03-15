# flipper-air-stats

> **Beta** — functional but expect bugs and config format changes.

Flipper Zero app for monitoring CO2, temperature, humidity and pressure via GPIO.

## Sensors

| Sensor | Interface | Measures |
|--------|-----------|----------|
| MH-Z19C | PWM (pin 3, PA6) | CO2 ppm |
| MH-Z19C | UART LPUART1 (pins 15/16) | CO2 ppm |
| BME280/BME680/etc | I2C (pins 15/16) | Temperature, humidity, pressure |

## Wiring

### MH-Z19C — PWM mode

```
Flipper pin 1  (5V)  → sensor Vin
Flipper pin 3  (PA6) ← sensor PWM out
Flipper pin 8  (GND) → sensor GND
```

### MH-Z19C — UART mode (LPUART1)

```
Flipper pin 1  (5V)  → sensor Vin
Flipper pin 15 (C1)  → sensor RX   [TX from Flipper]
Flipper pin 16 (C0)  ← sensor TX   [RX to Flipper]
Flipper pin 8  (GND) → sensor GND
```

### BME280 (I2C, default addr 0x76)

```
Flipper pin 9  (3.3V) → VCC
Flipper pin 15 (C1/SCL) → SCL
Flipper pin 16 (C0/SDA) → SDA
Flipper pin 8  (GND)  → GND
```

> BME280 and UART CO2 cannot be used simultaneously — both use pins 15/16.
> Default config: BME280 (I2C) + MH-Z19C (PWM). UART mode disables I2C climate sensor.

## Settings (per sensor, in Edit menu)

**CO2 sensor:**
- CO2 Type — switch between PWM and UART
- CO2 offset — manual correction ±1000 ppm, step 50 ppm
- Avg points — PWM averaging window 1–30 (1 = raw)

**Climate sensor:**
- Sensor — select type (BME280, BME680, DHT22, etc.)
- Temp. offset — manual correction ±10 °C, step 0.1 °C
- Temperature / Pressure / Humidity units
- Heat index on/off

## Build

```bash
ufbt fap_air_stats
```

Deploy to device:
```
/ext/apps/air_stats.fap
```

## Signal processing

**PWM mode:** keeps a rolling buffer of N readings (configurable 1–30, default 5, set in sensor Edit menu). Each new value is added; buffer is sorted and min/max are dropped; the middle values are averaged (trimmed mean). Until 3+ readings accumulate, median is used instead. Setting N=1 disables averaging entirely. Poll interval: 100 ms.

**UART mode:** no averaging — each poll returns a raw reading directly from the sensor. Poll interval: 5 s (sensor internal measurement cycle).

CO2 offset (if set) is applied in software after averaging, before display.

## Notes

- PWM CO2 algorithm from [flipper-zero-mh-z19](https://github.com/meshchaninov/flipper-zero-mh-z19)
- PWM accuracy: ±(40 ppm + 3%) per MH-Z19C datasheet
- Disconnect detection: PWM — no edge for 3 s clears CO2 reading; UART — no response clears CO2 reading
