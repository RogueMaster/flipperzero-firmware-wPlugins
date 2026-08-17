# Supported chips

Every part **Fake Chip Detector** knows how to recognise, and exactly what it reads to
do it. Generated from [`chip_db.c`](chip_db.c) — the app and this table cannot disagree.

- **Register** — the ID register the app reads, with the datasheet name where the
  datasheet gives one. A four-digit register index means the chip takes a 16-bit
  register address (ST time-of-flight parts and Goodix touch controllers do).
- **Expected** — the value a genuine part returns. A mask means only those bits are
  compared; the rest are revision or configuration bits that legitimately vary.
- **Width** — how many bytes the value itself is.
- **Live test** — an ID register is one byte, and one byte is what a relabeller can
  copy. Where a module exists, the app offers to make the part *do* its job and prove
  it. See [LIVE_TESTS.md](LIVE_TESTS.md) for how to write one.
- Several rows in one cell mean the app checks all of them. Every one has to match
  before it will say GENUINE.

If your chip is missing, the app says so plainly rather than calling it a fake — see
[Adding a chip](#adding-a-chip) below.

## Chips with a factory ID register (51)

These can be verified. A mismatch here is real evidence that the part is not what the
label claims.

| Chip | What it is | I2C address | Register | Expected | Width | Live test | Notes |
|---|---|---|---|---|---|---|---|
| **BNO055** | 9-axis IMU + fusion | 0x28, 0x29 | `0x00` CHIP_ID<br>`0x01` ACC_ID (BMA280)<br>`0x02` MAG_ID (BMM150)<br>`0x03` GYR_ID (BMG160) | `0xA0`<br>`0xFB`<br>`0x32`<br>`0x0F` | 8-bit<br>8-bit<br>8-bit<br>8-bit | Prove it finds north |  |
| **BMP280** | Pressure sensor | 0x76, 0x77 | `0xD0` | `0x58` | 8-bit | — |  |
| **BME280** | Press/temp/humidity | 0x76, 0x77 | `0xD0` | `0x60` | 8-bit | — |  |
| **BMP180** | Pressure sensor | 0x77 | `0xD0` | `0x55` | 8-bit | — |  |
| **BMP388** | Pressure sensor | 0x76, 0x77 | `0x00` | `0x50` | 8-bit | — |  |
| **BMP390** | Pressure sensor | 0x76, 0x77 | `0x00` | `0x60` | 8-bit | — |  |
| **BME680** | Air quality + climate | 0x76, 0x77 | `0xD0`<br>`0xF0` | `0x61`<br>`0x00` | 8-bit<br>8-bit | — |  |
| **BME688** | Air quality + climate | 0x76, 0x77 | `0xD0`<br>`0xF0` | `0x61`<br>`0x01` | 8-bit<br>8-bit | — |  |
| **DPS310** | Pressure sensor | 0x76, 0x77 | `0x0D` | `0x10` | 8-bit | — |  |
| **CCS811** | Air quality (VOC) | 0x5A, 0x5B | `0x20` | `0x81` | 8-bit | — | EOL part, clones common |
| **ENS160** | Air quality (VOC) | 0x52, 0x53 | `0x00`<br>`0x01` | `0x60`<br>`0x01` | 8-bit<br>8-bit | — |  |
| **HDC1080** | Temp + humidity | 0x40 | `0xFE`<br>`0xFF` | `0x5449`<br>`0x1050` | 16-bit<br>16-bit | — |  |
| **MPU6050** | 6-axis IMU | 0x68, 0x69 | `0x75` | `0x68` | 8-bit | Tip it and watch gravity | TDK EOL, old stock |
| **MPU6500** | 6-axis IMU | 0x68, 0x69 | `0x75` | `0x70` | 8-bit | Tip it and watch gravity | often sold as MPU9250 |
| **MPU9250** | 9-axis IMU | 0x68, 0x69 | `0x75` | `0x71` | 8-bit | Tip it and watch gravity | TDK EOL, often faked |
| **MPU6886** | 6-axis IMU | 0x68, 0x69 | `0x75` | `0x19` | 8-bit | — |  |
| **ICM20948** | 9-axis IMU | 0x68, 0x69 | `0x00` | `0xEA` | 8-bit | — |  |
| **ICM42605** | 6-axis IMU | 0x68, 0x69 | `0x75` | `0x42` | 8-bit | — |  |
| **ICM42688P** | 6-axis IMU | 0x68, 0x69 | `0x75` | `0x47` | 8-bit | — |  |
| **BMI160** | 6-axis IMU | 0x68, 0x69 | `0x00` | `0xD1` | 8-bit | — |  |
| **BMI270** | 6-axis IMU | 0x68, 0x69 | `0x00` | `0x24` | 8-bit | — |  |
| **BMI088 gyro** | Gyroscope | 0x68, 0x69 | `0x00` | `0x0F` | 8-bit | — |  |
| **BMI088 accel** | Accelerometer | 0x18, 0x19 | `0x00` | `0x1E` | 8-bit | — |  |
| **LSM6DS3** | 6-axis IMU | 0x6A, 0x6B | `0x0F` | `0x69` | 8-bit | — |  |
| **LSM6DS3TR-C** | 6-axis IMU | 0x6A, 0x6B | `0x0F` | `0x6A` | 8-bit | — |  |
| **LSM6DSO/OX** | 6-axis IMU | 0x6A, 0x6B | `0x0F` | `0x6C` | 8-bit | — | DSO and DSOX share the ID |
| **LSM6DSV16X** | 6-axis IMU | 0x6A, 0x6B | `0x0F` | `0x70` | 8-bit | — |  |
| **QMI8658** | 6-axis IMU | 0x6A, 0x6B | `0x00` | `0x05` | 8-bit | — |  |
| **LIS3DH/2DH12** | Accelerometer | 0x18, 0x19 | `0x0F` | `0x33` | 8-bit | — | same ID as LIS2DH12 |
| **ADXL345/343** | Accelerometer | 0x53, 0x1D | `0x00` | `0xE5` | 8-bit | Tip it and watch gravity |  |
| **ADXL355** | Accelerometer | 0x1D, 0x53 | `0x00`<br>`0x01`<br>`0x02` | `0xAD`<br>`0x1D`<br>`0xED` | 8-bit<br>8-bit<br>8-bit | — |  |
| **LIS3MDL** | Magnetometer | 0x1C, 0x1E | `0x0F` | `0x3D` | 8-bit | — |  |
| **LIS2MDL** | Magnetometer | 0x1E | `0x4F` | `0x40` | 8-bit | — |  |
| **MMC5603** | Magnetometer | 0x30 | `0x39` | `0x10` | 8-bit | — |  |
| **HMC5883L** | Magnetometer | 0x1E | `0x0A`<br>`0x0B`<br>`0x0C` | `0x48`<br>`0x34`<br>`0x33` | 8-bit<br>8-bit<br>8-bit | — | EOL since 2016, mostly fake |
| **QMC5883L** | Magnetometer | 0x0D | `0x0D` | `0xFF` | 8-bit | — |  |
| **VL53L0X** | Laser rangefinder | 0x29 | `0xC0` | `0xEE` | 8-bit | — |  |
| **VL53L1X** | Laser rangefinder | 0x29 | `0x010F` MODEL_ID<br>`0x0110` MODULE_TYPE | `0xEA`<br>`0xCC` | 8-bit<br>8-bit | — |  |
| **VL6180X** | Laser rangefinder | 0x29 | `0x0000` | `0xB4` | 8-bit | Watch it measure |  |
| **TCS34725** | Colour sensor | 0x29 | `0x92` | `0x44` | 8-bit | — |  |
| **TSL2591** | Light sensor | 0x29 | `0xB2` | `0x50` | 8-bit | — |  |
| **APDS9960** | Gesture + colour | 0x39 | `0x92` | `0xAB` | 8-bit | Wave your hand at it |  |
| **LTR-390UV** | UV + light sensor | 0x53 | `0x06` | `0xB0` (mask `0xF0`) | 8-bit | — |  |
| **MAX30102** | Heart rate / SpO2 | 0x57 | `0xFF` | `0x15` | 8-bit | — | 0x11 here = MAX30100 relabel |
| **INA226** | Current monitor | 0x40-0x4F | `0xFE`<br>`0xFF` | `0x5449`<br>`0x2260` | 16-bit<br>16-bit | — |  |
| **INA260** | Current monitor | 0x40-0x4F | `0xFE`<br>`0xFF` | `0x5449`<br>`0x2270` | 16-bit<br>16-bit | — |  |
| **INA228** | Current monitor | 0x40-0x4F | `0x3E` | `0x5449` | 16-bit | — |  |
| **TMP117** | Temperature sensor | 0x48-0x4B | `0x0F` | `0x0117` (mask `0x0FFF`) | 16-bit | — |  |
| **LPS22HB** | Pressure sensor | 0x5C, 0x5D | `0x0F` | `0xB1` | 8-bit | — |  |
| **LPS25HB** | Pressure sensor | 0x5C, 0x5D | `0x0F` | `0xBD` | 8-bit | — |  |
| **CST816S** | Touch controller | 0x15 | `0xA7` | `0xB4` | 8-bit | — | sleeps until touched |

## Chips recognised by address only (29)

These parts carry no ID register at all — there is nothing to read, so no honest tool
can confirm which one it is. The app reports them as DETECTED rather than pretending
to a verdict it cannot support.

This is exactly where a live test earns its keep. For a chip in the table above, a
live test is a second opinion; for one down here it is the *only* evidence that can
ever exist, because asking the part to do its job is the one question left to ask.

| Chip | What it is | I2C address | Live test | Notes |
|---|---|---|---|---|
| **DS3231** | Real-time clock | 0x68 | Watch the clock run |  |
| **DS1307** | Real-time clock | 0x68 | — |  |
| **PCF8563** | Real-time clock | 0x51 | — |  |
| **SSD1306/SH1106** | OLED display | 0x3C, 0x3D | Make the screen blink | SH1106 fakes undetectable |
| **AHT10/AHT20** | Temp + humidity | 0x38 | Breathe on it |  |
| **BH1750** | Light sensor | 0x23, 0x5C | Cover it with your hand |  |
| **SHT3x/SHT4x** | Temp + humidity | 0x44, 0x45 | Breathe on it | grade relabels undetectable |
| **SCD4x** | CO2 sensor | 0x62 | — |  |
| **SGP30** | Air quality (VOC) | 0x58 | — |  |
| **SGP40/41** | Air quality (VOC) | 0x59 | — |  |
| **SCD30** | CO2 sensor | 0x61 | — |  |
| **Si7021/HTU21D** | Temp + humidity | 0x40 | — |  |
| **MLX90614** | IR thermometer | 0x5A | Point it at your hand |  |
| **MLX90640** | Thermal camera | 0x33 | — |  |
| **AS5600** | Magnetic angle | 0x36 | — |  |
| **MAX17048** | Battery fuel gauge | 0x36 | — |  |
| **ADS1115** | ADC | 0x48-0x4B | — |  |
| **INA219** | Current monitor | 0x40-0x4F | — |  |
| **MCP23017** | GPIO expander | 0x20-0x27 | — |  |
| **PCF8574** | GPIO expander | 0x20-0x27 | — |  |
| **PCF8574A** | GPIO expander | 0x38-0x3F | — |  |
| **MCP4725** | DAC | 0x60-0x67 | — |  |
| **PCA9685** | PWM / servo driver | 0x40 | — |  |
| **TCA9548A** | I2C multiplexer | 0x70-0x77 | — |  |
| **AT24Cxx** | EEPROM memory | 0x50-0x57 | — |  |
| **MS5611** | Pressure sensor | 0x76, 0x77 | — |  |
| **VEML6070** | UV sensor | 0x38, 0x39 | — |  |
| **MAX44009** | Light sensor | 0x4A, 0x4B | — |  |
| **BNO085** | 9-axis IMU + fusion | 0x4A, 0x4B | — | SHTP protocol, no WHO_AM_I |

## Addresses more than one chip answers on (49)

**An I2C address does not name a part.** It is seven bits chosen by the manufacturer,
and plenty of unrelated chips chose the same ones. This is why the app probes rather
than looks up: for every candidate registered at the address that answered, it reads
that candidate's ID registers and keeps the one with the most matches. A scan that
reports one part at a crowded address has already ruled the others out.

| Address | Chips that use it |
|---|---|
| `0x18` | **BMI088 accel** (Accelerometer), **LIS3DH/2DH12** (Accelerometer) |
| `0x19` | **BMI088 accel** (Accelerometer), **LIS3DH/2DH12** (Accelerometer) |
| `0x1D` | **ADXL345/343** (Accelerometer), **ADXL355** (Accelerometer) |
| `0x1E` | **LIS3MDL** (Magnetometer), **LIS2MDL** (Magnetometer), **HMC5883L** (Magnetometer) |
| `0x20` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x21` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x22` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x23` | **BH1750** (Light sensor), **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x24` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x25` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x26` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x27` | **MCP23017** (GPIO expander), **PCF8574** (GPIO expander) |
| `0x29` | **BNO055** (9-axis IMU + fusion), **VL53L0X** (Laser rangefinder), **VL53L1X** (Laser rangefinder), **VL6180X** (Laser rangefinder), **TCS34725** (Colour sensor), **TSL2591** (Light sensor) |
| `0x36` | **AS5600** (Magnetic angle), **MAX17048** (Battery fuel gauge) |
| `0x38` | **AHT10/AHT20** (Temp + humidity), **PCF8574A** (GPIO expander), **VEML6070** (UV sensor) |
| `0x39` | **APDS9960** (Gesture + colour), **PCF8574A** (GPIO expander), **VEML6070** (UV sensor) |
| `0x3C` | **SSD1306/SH1106** (OLED display), **PCF8574A** (GPIO expander) |
| `0x3D` | **SSD1306/SH1106** (OLED display), **PCF8574A** (GPIO expander) |
| `0x40` | **HDC1080** (Temp + humidity), **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **Si7021/HTU21D** (Temp + humidity), **INA219** (Current monitor), **PCA9685** (PWM / servo driver) |
| `0x41` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x42` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x43` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x44` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **SHT3x/SHT4x** (Temp + humidity), **INA219** (Current monitor) |
| `0x45` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **SHT3x/SHT4x** (Temp + humidity), **INA219** (Current monitor) |
| `0x46` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x47` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x48` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **TMP117** (Temperature sensor), **ADS1115** (ADC), **INA219** (Current monitor) |
| `0x49` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **TMP117** (Temperature sensor), **ADS1115** (ADC), **INA219** (Current monitor) |
| `0x4A` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **TMP117** (Temperature sensor), **ADS1115** (ADC), **INA219** (Current monitor), **MAX44009** (Light sensor), **BNO085** (9-axis IMU + fusion) |
| `0x4B` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **TMP117** (Temperature sensor), **ADS1115** (ADC), **INA219** (Current monitor), **MAX44009** (Light sensor), **BNO085** (9-axis IMU + fusion) |
| `0x4C` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x4D` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x4E` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x4F` | **INA226** (Current monitor), **INA260** (Current monitor), **INA228** (Current monitor), **INA219** (Current monitor) |
| `0x51` | **PCF8563** (Real-time clock), **AT24Cxx** (EEPROM memory) |
| `0x52` | **ENS160** (Air quality (VOC)), **AT24Cxx** (EEPROM memory) |
| `0x53` | **ENS160** (Air quality (VOC)), **ADXL345/343** (Accelerometer), **ADXL355** (Accelerometer), **LTR-390UV** (UV + light sensor), **AT24Cxx** (EEPROM memory) |
| `0x57` | **MAX30102** (Heart rate / SpO2), **AT24Cxx** (EEPROM memory) |
| `0x5A` | **CCS811** (Air quality (VOC)), **MLX90614** (IR thermometer) |
| `0x5C` | **LPS22HB** (Pressure sensor), **LPS25HB** (Pressure sensor), **BH1750** (Light sensor) |
| `0x5D` | **LPS22HB** (Pressure sensor), **LPS25HB** (Pressure sensor) |
| `0x61` | **SCD30** (CO2 sensor), **MCP4725** (DAC) |
| `0x62` | **SCD4x** (CO2 sensor), **MCP4725** (DAC) |
| `0x68` | **MPU6050** (6-axis IMU), **MPU6500** (6-axis IMU), **MPU9250** (9-axis IMU), **MPU6886** (6-axis IMU), **ICM20948** (9-axis IMU), **ICM42605** (6-axis IMU), **ICM42688P** (6-axis IMU), **BMI160** (6-axis IMU), **BMI270** (6-axis IMU), **BMI088 gyro** (Gyroscope), **DS3231** (Real-time clock), **DS1307** (Real-time clock) |
| `0x69` | **MPU6050** (6-axis IMU), **MPU6500** (6-axis IMU), **MPU9250** (9-axis IMU), **MPU6886** (6-axis IMU), **ICM20948** (9-axis IMU), **ICM42605** (6-axis IMU), **ICM42688P** (6-axis IMU), **BMI160** (6-axis IMU), **BMI270** (6-axis IMU), **BMI088 gyro** (Gyroscope) |
| `0x6A` | **LSM6DS3** (6-axis IMU), **LSM6DS3TR-C** (6-axis IMU), **LSM6DSO/OX** (6-axis IMU), **LSM6DSV16X** (6-axis IMU), **QMI8658** (6-axis IMU) |
| `0x6B` | **LSM6DS3** (6-axis IMU), **LSM6DS3TR-C** (6-axis IMU), **LSM6DSO/OX** (6-axis IMU), **LSM6DSV16X** (6-axis IMU), **QMI8658** (6-axis IMU) |
| `0x76` | **BMP280** (Pressure sensor), **BME280** (Press/temp/humidity), **BMP388** (Pressure sensor), **BMP390** (Pressure sensor), **BME680** (Air quality + climate), **BME688** (Air quality + climate), **DPS310** (Pressure sensor), **TCA9548A** (I2C multiplexer), **MS5611** (Pressure sensor) |
| `0x77` | **BMP280** (Pressure sensor), **BME280** (Press/temp/humidity), **BMP180** (Pressure sensor), **BMP388** (Pressure sensor), **BMP390** (Pressure sensor), **BME680** (Air quality + climate), **BME688** (Air quality + climate), **DPS310** (Pressure sensor), **TCA9548A** (I2C multiplexer), **MS5611** (Pressure sensor) |

Reading this the other way: a chip whose neighbours all have ID registers is safe to
identify by probing, and one sharing an address with an address-only part is not — the
app will say DETECTED rather than guess between them.

### Chips that can sit at more than one address (39)

A pin on the module picks which. If a scan finds nothing, the pin is worth checking
before the wiring is: the app searches every address in this list, but only the ones
in it.

| Chip | Addresses | Notes |
|---|---|---|
| **BNO055** | `0x28`, `0x29` |  |
| **BMP280** | `0x76`, `0x77` |  |
| **BME280** | `0x76`, `0x77` |  |
| **BMP388** | `0x76`, `0x77` |  |
| **BMP390** | `0x76`, `0x77` |  |
| **BME680** | `0x76`, `0x77` |  |
| **BME688** | `0x76`, `0x77` |  |
| **DPS310** | `0x76`, `0x77` |  |
| **CCS811** | `0x5A`, `0x5B` | EOL part, clones common |
| **ENS160** | `0x52`, `0x53` |  |
| **MPU6050** | `0x68`, `0x69` | TDK EOL, old stock |
| **MPU6500** | `0x68`, `0x69` | often sold as MPU9250 |
| **MPU9250** | `0x68`, `0x69` | TDK EOL, often faked |
| **MPU6886** | `0x68`, `0x69` |  |
| **ICM20948** | `0x68`, `0x69` |  |
| **ICM42605** | `0x68`, `0x69` |  |
| **ICM42688P** | `0x68`, `0x69` |  |
| **BMI160** | `0x68`, `0x69` |  |
| **BMI270** | `0x68`, `0x69` |  |
| **BMI088 gyro** | `0x68`, `0x69` |  |
| **BMI088 accel** | `0x18`, `0x19` |  |
| **LSM6DS3** | `0x6A`, `0x6B` |  |
| **LSM6DS3TR-C** | `0x6A`, `0x6B` |  |
| **LSM6DSO/OX** | `0x6A`, `0x6B` | DSO and DSOX share the ID |
| **LSM6DSV16X** | `0x6A`, `0x6B` |  |
| **QMI8658** | `0x6A`, `0x6B` |  |
| **LIS3DH/2DH12** | `0x18`, `0x19` | same ID as LIS2DH12 |
| **ADXL345/343** | `0x53`, `0x1D` |  |
| **ADXL355** | `0x1D`, `0x53` |  |
| **LIS3MDL** | `0x1C`, `0x1E` |  |
| **LPS22HB** | `0x5C`, `0x5D` |  |
| **LPS25HB** | `0x5C`, `0x5D` |  |
| **SSD1306/SH1106** | `0x3C`, `0x3D` | SH1106 fakes undetectable |
| **BH1750** | `0x23`, `0x5C` |  |
| **SHT3x/SHT4x** | `0x44`, `0x45` | grade relabels undetectable |
| **MS5611** | `0x76`, `0x77` |  |
| **VEML6070** | `0x38`, `0x39` |  |
| **MAX44009** | `0x4A`, `0x4B` |  |
| **BNO085** | `0x4A`, `0x4B` | SHTP protocol, no WHO_AM_I |

The BNO055 is the one to know about, because its datasheet and every breakout board
disagree. Bosch BST-BNO055-DS000 Table 4-7 calls `0x29` the *default* and `0x28` the
alternative, selected by the COM3 pin: HIGH gives `0x29`, LOW gives `0x28`. Boards tie
COM3 low, so in practice almost every module answers on `0x28` and everyone calls that
the default. Both are in the database.

That same chip is also the clearest case of an address proving nothing: `0x29` is
shared with three ST time-of-flight rangefinders and two light sensors, and the app
separates them by reading four ID registers rather than one — CHIP_ID plus the
BMA280, BMM150 and BMG160 sub-IDs, which clones get wrong far more often than they get
CHIP_ID wrong.

## Chips that can be strapped off the I2C bus (7)

These parts have a pin that decides whether they speak I2C at all. Set the wrong way —
by the board, by the factory, or by one glitch on the pad — the part is healthy, powered
and completely invisible to any scan, because in that state it does not have an I2C
address to answer on. An empty scan is not evidence that a chip is dead.

| Chip | Pad | I2C needs it | Otherwise it speaks | After strapping |
|---|---|---|---|---|
| **BNO055** | `PS1` | LOW | UART | power-cycle it |
| **BMP280** | `CSB` | HIGH | SPI | power-cycle it |
| **BME280** | `CSB` | HIGH | SPI | power-cycle it |
| **BME680** | `CSB` | HIGH | SPI | power-cycle it |
| **LIS3DH/2DH12** | `CS` | HIGH | SPI | takes effect at once |
| **LSM6DS3** | `CS` | HIGH | SPI | takes effect at once |
| **ADXL345/343** | `CS` | HIGH | SPI | takes effect at once |

The last column is not a detail. A latched pin is sampled at reset and nowhere else, so
strapping the pad and rescanning changes nothing and looks like proof the part is
broken. Bosch put it plainly for the BMP280, BME280 and BME680: once `CSB` has been
pulled down even once, *"the I2C interface is disabled until the next power-on-reset"*.

The list is short because a row that could not be checked against a datasheet is not
here. A wrong entry would send someone to tie a pin the wrong way round, which is worse
than no entry at all. Address-select pins are deliberately excluded: the sweep covers
`0x08`-`0x77`, so they cannot hide a part.

## 1-Wire parts (15)

A different bus, on **pin 17**, and a weaker guarantee. Every 1-Wire part carries a
64-bit ROM code burned in at the factory, but any microcontroller can replay one, so
finding the expected ID proves a device is *present* — never that it is authentic. The
app says so on screen and never reports a 1-Wire part as GENUINE.

What it does prove is which **part** answered: the family code (the low byte of the ROM)
selects the command set and register layout, so a DS18S20 or DS1822 sold as a DS18B20 is
a fact here, not a suspicion. Temperature parts are taken one step further — the app runs
a real conversion and checks the scratchpad CRC, so it reports a working measurement
rather than mere presence.

| Family code | Part | What it is | Measured |
|---|---|---|---|
| `0x01` | **DS1990A/DS2401** | Serial number key | — |
| `0x04` | **DS2404** | Clock + memory | — |
| `0x05` | **DS2405** | Addressable switch | — |
| `0x10` | **DS18S20** | Temperature sensor | temperature |
| `0x1D` | **DS2423** | RAM + counter | — |
| `0x20` | **DS2450** | 4-channel ADC | — |
| `0x22` | **DS1822** | Temperature sensor | temperature |
| `0x26` | **DS2438** | Battery monitor | — |
| `0x28` | **DS18B20** | Temperature sensor | temperature |
| `0x29` | **DS2408** | 8-channel switch | — |
| `0x2D` | **DS2431** | 1Kb EEPROM | — |
| `0x3A` | **DS2413** | Dual switch | — |
| `0x3B` | **DS1825/MAX31826** | Temperature sensor | temperature |
| `0x42` | **DS28EA00** | Temperature sensor | temperature |
| `0x43` | **DS28EC20** | 20Kb EEPROM | — |

Family codes are from Analog Devices application note AN937 and the parts' datasheets.

## Adding a chip

Add an `IdCheck` array and one `ChipEntry` row to `chip_db.c`, rebuild, then re-run
`python tools/gen_supported_chips.py` from the repository root to regenerate this file —
that regeneration step is the only thing keeping the table honest. The rule
the database is held to: **every constant must come from the manufacturer datasheet or
the vendor's own driver.** A wrong expected value makes the app accuse a genuine sensor
of being counterfeit, which is far worse than not supporting the part at all. Anything
that could not be pinned down to a primary source was deliberately left out.

Cite the source in a comment, the way the existing entries do.
