# Build your own CAN Commander (ESP32-S3 + SN65HVD230)

This guide builds a single-bus CAN Commander from two cheap modules: an ESP32-S3 development board and an SN65HVD230 CAN transceiver. It runs the same firmware as the official board and works with the CAN Commander Flipper app.

**What you get:** one CAN bus (`can0`) with every tool: read, filter, write, speed, value/bit tracking, unique IDs, reverse engineering, OBD-II PIDs/VIN/DTCs, DBC decode, custom inject and replay. It also has the USB and Wi-Fi command lines.

**What you don't get:** the second bus (`can1`, the left port on the official board) and the status LEDs (you can add an LED, see [Optional: status LED](#optional-status-led)). With one bus, `bus=both` simply means `can0`.

## Parts

| Part | Notes |
|---|---|
| ESP32-S3 dev board with native USB | e.g. ESP32-S3-DevKitC-1. Any S3 board that breaks out GPIO1, 2, 17 and 18 works. |
| SN65HVD230 CAN transceiver module | 3.3 V transceiver. Do not use a 5 V-only transceiver (e.g. TJA1050 modules) without level shifting. |
| Jumper wires | 8 to 10 |
| OBD-II pigtail or breakout (for vehicles) | Needed for in-car use only. |
| Flipper Zero | Optional; the board also works on its own over USB or Wi-Fi. |

## Wiring

All signals are 3.3 V. The pins below are the ones the build in the next section uses.

**Transceiver to ESP32-S3**

| SN65HVD230 module | ESP32-S3 |
|---|---|
| 3V3 | 3V3 |
| GND | GND |
| CTX (also labelled D or TXD) | GPIO2 |
| CRX (also labelled R or RXD) | GPIO1 |

If your module has an `Rs` (or `S`) pin, tie it to GND. Pulling it high puts the transceiver into standby, and nothing will be transmitted.

**Flipper Zero to ESP32-S3**

| Flipper GPIO header | ESP32-S3 |
|---|---|
| Pin 13 (TX) | GPIO18 (Flipper UART RX) |
| Pin 14 (RX) | GPIO17 (Flipper UART TX) |
| Pin 8, 11 or 18 (GND) | GND |
| Pin 1 (5V) | 5V (see power note below) |

TX goes to RX in both directions.

**CAN bus**

| SN65HVD230 | Vehicle OBD-II port | Bench bus / other device |
|---|---|---|
| CANH | Pin 6 | CAN-H |
| CANL | Pin 14 | CAN-L |

OBD-II pins 6/14 are the standard high-speed CAN bus (usually 500 kbit/s). Some vehicles have a second bus on other pins (for example Ford MS-CAN on pins 3/11 at 125 kbit/s); move the two CAN wires to read those.

**Termination:** many SN65HVD230 modules include a 120 Ω resistor between CANH and CANL. To check, measure across CANH/CANL with the module unpowered: about 120 Ω means it is fitted. A vehicle bus is already terminated at both ends, so remove that resistor (or cut its trace) for in-car use. Keep it only when you build a two-device bench bus that has no other termination.

**Power:** when connected to a Flipper, the CAN Commander app switches on the Flipper's 5V pin and powers the board. When the board is plugged into a computer over USB at the same time, leave the Flipper 5V wire disconnected (keep GND, TX and RX). Some boards don't isolate their 5V pin from USB power. Don't power the board from the Flipper's 3.3V pin; it can't supply the ESP32's Wi-Fi current.

### Why GPIO17/18 for the Flipper?

The official board uses GPIO43/44. On boards like the ESP32-S3-DevKitC-1, GPIO43/44 are wired to the onboard USB-to-UART chip (the port labelled "UART"), which would talk over the Flipper. If your board has native USB only (no USB-to-UART chip on GPIO43/44, e.g. Seeed XIAO ESP32S3), you can wire the Flipper to GPIO43/44 and skip the two Flipper pin settings below.

## Build and flash

You need the Arduino CLI (or Arduino IDE 2) with the **esp32** core 3.3.x installed (Boards Manager: "esp32 by Espressif Systems"). This configuration needs no extra libraries.

### Arduino CLI

From the firmware folder:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --build-property "compiler.cpp.extra_flags=-DBOARDCFG_CAN1_DRIVER=0 -DBOARDCFG_NEOPIXEL_ENABLED=0 -DBOARDCFG_FLIPPER_UART_TX_PIN=17 -DBOARDCFG_FLIPPER_UART_RX_PIN=18" .

arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc -p <PORT> .
```

Use the same `--fqbn` and flags for both commands. `<PORT>` is shown by `arduino-cli board list` (for example `/dev/cu.usbmodem101` on macOS or `COM5` on Windows).

### Arduino IDE

1. Open `CAN_Commander_Main.ino`.
2. Open `src/BoardConfig.h` and add these lines just below `#define BOARDCFG_CAN1_DRIVER_MCP2515 2`:
   ```cpp
   // My DIY board: ESP32-S3 + SN65HVD230, single bus
   #define BOARDCFG_CAN1_DRIVER BOARDCFG_CAN1_DRIVER_NONE
   #define BOARDCFG_NEOPIXEL_ENABLED 0
   #define BOARDCFG_FLIPPER_UART_TX_PIN 17
   #define BOARDCFG_FLIPPER_UART_RX_PIN 18
   ```
3. Tools > Board > esp32 > **ESP32S3 Dev Module**.
4. Tools > **USB CDC On Boot: Enabled** and **USB Mode: Hardware CDC and JTAG**.
5. Select the port and click Upload.

If the upload doesn't start, hold the board's BOOT button, tap RESET, release BOOT, and upload again; then press RESET once the upload finishes.

The build stops with an explanation if USB CDC On Boot is off or if two functions are assigned to the same pin.

## First test

1. Connect to the board's **native USB** port (labelled "USB" on the DevKitC) with any serial terminal at 115200 baud: Arduino Serial Monitor, `screen`, PuTTY, etc.
2. Type `ver`, then `status`. You should see:
   ```text
   CAN Commander v2.3
   owner=none active_tool=none dbc_signals=0
   can1 backend=disabled state=disabled
   wifi ap=down autostart=0 ssid=CAN_Commander clients=0 cli_port=7777
   ```
3. Connect the CAN wires to the vehicle, turn the ignition on, and listen without transmitting:
   ```text
   bus set bus=can0 bitrate=500000 mode=listen
   tool start read_all bus=can0
   ```
   Frames should start scrolling. Stop with `tool stop`.
4. Before using tools that transmit (`write`, `custom_inject`, `replay`, `obd_pid`), switch back to normal mode:
   ```text
   bus set bus=can0 bitrate=500000 mode=normal
   ```

Listen mode never drives the bus, which makes it the safe way to check bitrate and wiring on an unfamiliar vehicle. Normal mode acknowledges frames and can transmit.

## Using it with the Flipper

1. Wire the Flipper as above and open **Apps > GPIO > CAN Commander**.
2. The app powers the board, connects and checks the firmware version.
3. Every tool defaults to `can0`, so the tools work as-is. Anything set to `can1` reports `bus can1 unavailable`.

The Flipper app README calls `can0` the "right port" and `can1` the "left port": on this board, your one transceiver is the right port.

## Optional: status LED

The ESP32-S3-DevKitC-1 has an onboard RGB LED (GPIO48 on v1.0 boards, GPIO38 on v1.1). To use it as the status light, install the **Adafruit NeoPixel** library and replace `-DBOARDCFG_NEOPIXEL_ENABLED=0` with:

```text
-DBOARDCFG_NEOPIXEL_ENABLED=1 -DBOARDCFG_NEOPIXEL_PIN=48 -DBOARDCFG_NEOPIXEL_COUNT=1
```

With one LED you get the status light only (no RX/TX activity blink). See "Status LEDs" in `README.md` for the colors.

## Troubleshooting

| Symptom | Check |
|---|---|
| No frames in listen mode | Ignition on? Try `bitrate=250000` or `125000`. Swap CANH/CANL. Check `Rs` is low. |
| No frames at any bitrate, and transmits always fail | CTX/CRX swapped or on the wrong GPIO; transceiver missing 3V3 or GND. |
| `TX failed` events when transmitting | Still in listen mode, or no other device on the bus to acknowledge (a lone transmitter is never acknowledged). |
| Flipper says "Not connected" | TX/RX swapped, missing GND, or firmware built without the GPIO17/18 settings. |
| Board resets when Wi-Fi starts | Power: use the 5V pin or USB, not 3.3V. |
| Nothing in the serial monitor | Use the native USB port and make sure USB CDC On Boot was enabled when building. |

## Safety

Transmitting on a vehicle bus can set fault codes, light warnings or move actuators. Start in listen mode, test on a bench or a parked vehicle, and stop transmitting tools with `tool stop` when you're done. The Wi-Fi command line is off by default; if you turn it on, set your own password first (`wifi set pass=...`).
