# CaiXianlin Shock Collar Remote for Flipper Zero

A Flipper Zero application to control CaiXianlin shock collar.

![Screenshot](screenshots/0-home-vibrate.png)

**WARNING:** This application is intended for **educational and research purposes only**.

**NOTE:** I don't endorse use of these devices on any animals. You do you, but I'd never use this device on an animal.

## Features

- **Send** shock, vibrate, or beep commands
- **Adjustable strength** (0–100)
- **Channels** (0–2)
- **Clone/Listen mode** – Capture Station ID and channel from an existing remote controller / hub
- **Persistent settings** – Station ID and channel are saved

## Controls

**Main Screen:**

| Button           | Action                           |
|------------------|----------------------------------|
| **OK (hold)**    | Transmit signal                  |
| **←** / **→**    | Change mode (Shock/Vibrate/Beep) |
| **↑** / **↓**    | Adjust strength (0–100)          |
| **Back (short)** | Exit app                         |
| **Back (hold)**  | Open setup screen                |

**Setup Screen:**

| Button        | Action                         |
|---------------|--------------------------------|
| **↑** / **↓** | Navigate menu                  |
| **←** / **→** | Change channel (when selected) |
| **OK**        | Select option                  |
| **Back**      | Return to main screen          |

## Protocol Specification

**RF Parameters:**

| Parameter  | Value                          |
|------------|--------------------------------|
| Frequency  | 433.92 MHz                     |
| Modulation | ASK/OOK                        |
| Preset     | FuriHalSubGhzPresetOok270Async |

**Bit Encoding:**

| Element | HIGH Duration   | LOW Duration   | Ratio |
|---------|-----------------|----------------|-------|
| Sync    | ~1250 µs (5 TE) | ~750 µs (3 TE) | 5:3   |
| Bit 1   | ~750 µs (3 TE)  | ~250 µs (1 TE) | 3:1   |
| Bit 0   | ~250 µs (1 TE)  | ~750 µs (3 TE) | 1:3   |

Where TE (Time Element) ≈ 250 µs

Based on [https://wiki.openshock.org/hardware/shockers/caixianlin](https://wiki.openshock.org/hardware/shockers/caixianlin)
and captured signals from the controller hub. The app is using integer ratios matching the behavior of a real
controller, rather than strictly following the protocol as specified in the wiki.

**Packet Structure (42 bits):**

```
[SYNC] [STATION_ID:16] [CHANNEL:4] [MODE:4] [STRENGTH:8] [CHECKSUM:8] [END:2]
```

| Field      | Bits | Range   | Description                                    |
|------------|------|---------|------------------------------------------------|
| Station ID | 16   | 0-65535 | Transmitter identifier (collar paired to this) |
| Channel    | 4    | 0-2     | Channel number                                 |
| Mode       | 4    | 1-3     | 1=Shock, 2=Vibrate, 3=Beep                     |
| Strength   | 8    | 0-100   | Intensity (should be 0 for Beep)               |
| Checksum   | 8    | 0-255   | 8-bit sum of all preceding bytes               |
| End        | 2    | 00      | Always 00                                      |

**Checksum Calculation:**

```c
checksum = (STATION_ID_high_byte + STATION_ID_low_byte + Channel + Mode + Strength) & 0xFF
```

## First Launch

On the first launch, the setup screen will appear. You can:
1. Manually enter the Station ID (if you know it)
2. Set the channel (0-2)
3. Use **Listen for Remote** to clone an existing remote
4. Press **Done** to start using the app

Settings are automatically saved and will be restored on the next launch.

## License

MIT License – see [LICENSE](LICENSE) file for details.

## Acknowledgments

- Protocol analysis based on [https://wiki.openshock.org/hardware/shockers/caixianlin](https://wiki.openshock.org/hardware/shockers/caixianlin) and reverse engineering of 
  captured signals from the controller hub
- Flipper Zero firmware team for the excellent SDK
