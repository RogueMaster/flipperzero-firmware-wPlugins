# How to try it out

Ordered from "no hardware at all" to "a real counterfeit hunt". Each step takes a minute.

## Before you start: how to connect anything safely

**Ground first, signals last.** The order matters and it is always the same:

1. **GND** (pin 8) — ties both boards to the same reference. Until this wire is in, any current
   from a signal or supply line has to return through the chip's ESD protection diodes, which
   is how modules get damaged or latch up.
2. **3.3 V** (pin 9) — power the module before you talk to it.
3. **SDA** (pin 15), then **SCL** (pin 16) — signals last, into an already-powered chip. Driving
   a data line into an unpowered chip forward-biases its input protection diode and
   back-powers it through the pin, which is out of spec.

Unplugging is the exact reverse: signals, then power, then ground last.

The app's wiring screen lists the pins in this order on purpose — just work down the list.

Two more rules: the Flipper's I/O is **3.3 V and not 5 V tolerant**, and pin 1 (+5 V) is off
unless OTG is enabled — never feed it into a sensor's signal pins. If you are wiring a bare
chip rather than a breakout board, you need 4.7 kΩ pull-up resistors from SDA and SCL to 3.3 V;
almost every breakout module already has them.

---

## Step 1 — no hardware needed (30 seconds)

Open **Fake Chip Detector** from `Apps → GPIO`, then **How to wire** with nothing plugged in.

You should see four broken, dashed wires and "Waiting for sensor…". Press BACK, then run
**Scan bus**. Expected result: *"No pull-ups on the bus / sensor unpowered or not wired"*.

That confirms the app is measuring the lines rather than guessing.

## Step 2 — one jumper wire

With the app on the wiring screen, connect a single jumper from **pin 15 to any GND pin**
(8, 11 or 18).

The SDA row should switch to a fault marker and the hint should read *"Line stuck low"*,
naming pin 15. Move the jumper to **pin 16** and it should name pin 16 instead.

Now connect a jumper from **pin 15 straight to pin 16** — the app should report
*"SDA and SCL are shorted!"*. Nothing else on the Flipper can tell you that.

**The same jumper proves the serial path.** Leave it between pin 15 and pin 16, press BACK to the
menu and open **UART self-test**, then press OK. Expected result: *"Passed"*, with *"6 bytes back,
clean"* under it. The app sent six bytes out of pin 15 and read them back on pin 16 over the
LPUART — the same path it uses to listen for a talking sensor after an I²C sweep finds nothing.

Pull the jumper out and run it again: *"Failed"* and *"nothing came back"*. Leave a module wired
up and it declines rather than reporting a fault — *"Not run"*, *"Pull-ups on pins 15/16"* —
because the test drives pin 15 and will not fight a sensor for the line.

## Step 3 — plug into the wrong pins on purpose

Wire any I²C module's SDA/SCL to **pins 6 and 7** instead of 15/16 (keep GND and 3.3 V correct).
The app sweeps the other header pins for pull-ups and should tell you it found them on the
wrong pin.

## Step 4 — a real module

Judging by the apps on your Flipper, you probably have some of these already. Identify them by
the silkscreen text on the board:

| Board silkscreen | What it is | Address | Expected verdict |
|---|---|---|---|
| `GY-291`, `ADXL345` | accelerometer | 0x53 | `ADXL345/343 GENUINE` (ID 0xE5) |
| `GY-521`, `MPU-6050` | 6-axis IMU | 0x68 | `MPU6050 GENUINE` (ID 0x68) — or `MPU6500`, which is the usual relabel |
| `GY-BMP280`, `GY-BME280` | pressure / humidity | 0x76 or 0x77 | **the interesting one** — see below |
| `GY-271`, `HMC5883L` | magnetometer | 0x1E or 0x0D | almost certainly `QMC5883L`, not the HMC5883L printed on it |
| `ZS-042`, `DS3231` | RTC module | 0x68 **and** 0x57 | two devices: `DS3231 DETECTED (no ID reg)` plus the onboard `AT24Cxx EEPROM` |
| `0.96" OLED`, `SSD1306` | display | 0x3C | `SSD1306/SH1106 DETECTED (no ID reg)` |
| `INA219` / `INA226` breakout | current monitor | 0x40 | INA226 → `GENUINE`; INA219 → `DETECTED (no ID reg)`, it has no ID register |
| `GY-VL53L0XV2` | laser distance | 0x29 | `VL53L0X GENUINE` (ID 0xEE) |
| `AHT20`, `GY-213V` | temp/humidity | 0x38 | `AHT10/AHT20 DETECTED (no ID reg)` |

Product pages if you want to match a photo to a board:

- ADXL345 — https://www.analog.com/en/products/adxl345.html
- MPU-6050 — https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/
- BMP280 / BME280 — https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/ and https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/
- DS3231 — https://www.analog.com/en/products/ds3231.html
- SSD1306 — https://www.adafruit.com/product/326
- INA219 / INA226 — https://www.ti.com/product/INA219 and https://www.ti.com/product/INA226
- VL53L0X — https://www.st.com/en/imaging-and-photonics-solutions/vl53l0x.html
- QMC5883L vs HMC5883L — https://qstcorp.com/upload/pdf/202202/13-52-04%20QMC5883L%20Datasheet%20Rev.%20A(1).pdf

## Step 5 — the actual counterfeit test

If you have a board labelled **BME280**, this is the whole point of the app.

BMP280 (pressure only) and BME280 (pressure **and humidity**) are pin-compatible, live at the
same address, and look identical. Sellers ship the cheaper BMP280 die on boards printed
"BME280" constantly. One register tells them apart:

- reg `0xD0` = **0x58** → BMP280. If the board says BME280, you were sold the wrong part.
- reg `0xD0` = **0x60** → genuine BME280.

Scan, press OK on the 0x76 entry, and the detail screen shows the register, the expected value
and the verdict. Press RIGHT on the results list to save a log to
`/ext/apps_data/fake_chip_detector/` — that is the file to attach to a refund dispute.

The same trick catches a "MPU9250" that is really an MPU6500 (reg `0x75`: 0x71 vs 0x70) and a
"HMC5883L" that is really a QMC5883L (different address entirely: 0x0D instead of 0x1E).

## Step 6 — the live test

Scan a part that has one, answer *yes* to "is this what you bought?", and the **ALL GOOD**
screen offers a live test under `OK`. It proves the sensor does not merely identify itself but
actually works. Chips with no test show the same screen without the offer; that is normal, most
parts have none.

Every test is one you could run at a pickup counter before paying, using nothing but your hand
and your breath. A tick appears in the corner and the Flipper chimes the moment a test passes.

| Board | What to do | What proves it |
|---|---|---|
| `AHT10`, `AHT20`, `GY-213V` | breathe on it | humidity climbs 15+ points |
| `SHT30`, `SHT31`, `SHT40` | breathe on it | same; the app also says which family answered |
| `BH1750`, `GY-302` | hold it to the light, then cover it | it must reach 30+ counts lit **and** fall to ≤3 covered — the dark floor its datasheet specifies |
| `GY-521` / `MPU-6050`, `GY-291` / `ADXL345` | lay it flat, then tip it on its side | gravity has to move to a different axis; magnitude stays near 1.00 g |
| `APDS-9960` | wave a hand over it | proximity count rises 50+ |
| `GY-906` / `MLX90614` | point it at your palm | object temperature runs 5 °C over the ambient the same chip reports |
| `ZS-042` / `DS3231` | nothing at all | the seconds advance by exactly one, three times running |
| `0.96" OLED` / `SSD1306` | look at the panel | it blinks fully lit, then dark |
| `BNO055` | rotate it in a figure-8 | live compass; MAG CAL climbs to 3/3 |
| `VL6180X` | hold a hand ~5 cm away | the distance follows it; passes after 30 mm of movement |

Two of these are worth doing deliberately, because they are the ones the app could otherwise
never say anything about:

- **DS3231 and AHT20 have no ID register.** Before this, the app could only report "something is
  at this address". The verdict screen now says **IT ANSWERS / No ID to check** rather than
  claiming the part is genuine, and the live test is the only proof available.
- **The DS3231 test writes nothing at all.** Reading the clock cannot disturb it — only writing
  the seconds register resets the countdown chain — so it is safe on an RTC already keeping
  time you care about.

The OLED test is the one exception to "the app decides": a display has no readback, so every
command is acknowledged whether or not the panel lights. It blinks the screen and leaves the
verdict to you, and deliberately never claims a pass.

`Up` still opens the written report from that screen, and `Right` still opens the hex detail.

[LIVE_TESTS.md](fake_chip_detector/LIVE_TESTS.md) covers writing a test for a part that has
none.

## Step 7 — 1-Wire, if you have a DS18B20

A DS18B20 probe — the stainless steel one on a cable, or a bare TO-92 — wires to **pin 17**
(data), pin 8 (GND) and pin 9 (3.3 V), with a 4.7 kΩ resistor from data to 3.3 V if the probe
does not already have one. Then run **Scan 1-Wire**.

The app reads the 64-bit ROM, decodes the family code and runs a real temperature conversion:

- family `0x28` is a DS18B20, `0x10` a DS18S20, `0x22` a DS1822 — a DS18S20 sold as a DS18B20 is
  caught here, and they are not interchangeable: the DS18S20 reports in half degrees.
- the temperature has to be plausible for the room. The app rejects a scratchpad of all zeros
  rather than reporting it as a working sensor at exactly 0.0 °C, because the checksum over
  eight zero bytes is itself zero and proves nothing.

A 1-Wire ROM can be replayed by any microcontroller, so this proves **which part answered**,
never that the part is authentic. The app says so on the screen.

## If nothing is found

The app tells you which of these it is rather than making you guess:

- *"No pull-ups on the bus"* — the module has no power, or SDA/SCL are not connected.
- *"Line stuck low"* — a short to ground, or a hung chip. Unplug and re-seat.
- *"Bus looks electrically OK"* but no devices — SDA and SCL are probably swapped, the chip is
  at an address outside 0x08–0x77, or it is dead.
