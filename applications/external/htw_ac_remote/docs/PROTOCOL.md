# HTW AC IR Protocol

## Physical Layer

### Carrier
- **Frequency**: 38 kHz
- **Modulation**: Pulse-distance (mark/space)

### Timing

| Element | Duration |
|---------|----------|
| Leader MARK | 4400 µs |
| Leader SPACE | 4380 µs |
| Bit MARK | 560 µs |
| Bit 0 SPACE | 510 µs |
| Bit 1 SPACE | 1590 µs |
| Stop MARK | 560 µs |
| Inter-frame Gap | 5180 µs |

## Frame Structure

### Transmission Format

Each command is sent as **two identical frames** for reliability:

```
[Leader] [48 bits] [Stop] [Gap] [Leader] [48 bits] [Stop]
```

### Data Format

- **Frame size**: 48 bits = 6 bytes
- **Bit order**: LSB-first within each byte
- **Byte order**: Sequential (b0 → b1 → b2 → b3 → b4 → b5)

### Checksum Types

#### INV3 (Three pairs with inversions)
```
[b0] [INV(b0)] [b2] [INV(b2)] [b4] [INV(b4)]
```

Used for: State commands, Power Off, Swing, Toggle buttons

#### INV2 (Two pairs with inversions)
```
[b0] [INV(b0)] [b2] [INV(b2)] [b4] [b5]
```

Used for: Timer commands (b4, b5 are NOT inverses)

## Command Families

### Family A (0x4D) - Main Commands

```
b0 = 0x4D
b1 = 0xB2 (INV of 0x4D)
```

Used for: State, Power Off, Swing, Timers

### Family B (0xAD) - Toggle Functions

```
b0 = 0xAD
b1 = 0x52 (INV of 0xAD)
b2 = 0xAF
b3 = 0x50 (INV of 0xAF)
b4 = CODE
b5 = INV(CODE)
```

## State Command (Power ON + Parameters)

Sets mode, temperature, and fan speed. Sending any state command turns the AC ON.

### Format (INV3, Family 0x4D)

```
b0 = 0x4D
b1 = 0xB2
b2 = FAN
b3 = INV(FAN)
b4 = MODE_TEMP
b5 = INV(MODE_TEMP)
```

### FAN Byte

| Fan Speed | Code |
|-----------|------|
| Auto (Cool/Heat/Fan) | 0xFD |
| Low | 0xF9 |
| Medium | 0xFA |
| High | 0xFC |
| Forced (Auto/Dry modes) | 0xF8 |

### MODE_TEMP Byte

Upper nibble = Mode, Lower nibble = Temperature code

#### Mode Nibbles

| Mode | Nibble |
|------|--------|
| Cool | 0x0 |
| Auto | 0x1 |
| Dry | 0x2 |
| Heat | 0x3 |
| Fan-only | Special: MODE_TEMP = 0x27 |

#### Temperature Codes

| Temp °C | Code |
|---------|------|
| 17 | 0x0 |
| 18 | 0x8 |
| 19 | 0xC |
| 20 | 0x4 |
| 21 | 0x6 |
| 22 | 0xE |
| 23 | 0xA |
| 24 | 0x2 |
| 25 | 0x3 |
| 26 | 0xB |
| 27 | 0x9 |
| 28 | 0x1 |
| 29 | 0x5 |
| 30 | 0xD |

### Examples

- **Cool 22°C, Fan Auto**: `4D B2 FD 02 0E F1`
- **Heat 24°C, Fan Auto**: `4D B2 FD 02 32 CD`
- **Fan-only, Fan Low**: `4D B2 F9 06 27 D8`

## Power OFF

Fixed frame (INV3, Family 0x4D):

```
4D B2 DE 21 07 F8
```

## Swing Toggle

Fixed frame (INV3, Family 0x4D):

```
4D B2 D6 29 07 F8
```

## Function Toggles (Family 0xAD)

| Function | CODE | Full Frame |
|----------|------|------------|
| LED | 0xA5 | AD 52 AF 50 A5 5A |
| Turbo | 0x45 | AD 52 AF 50 45 BA |
| Fresh | 0xC5 | AD 52 AF 50 C5 3A |
| Clean | 0x55 | AD 52 AF 50 55 AA |

## Timer Commands

### Time Values (34 Steps)

| Step | Time | Base ARG | ON Flag | OFF Page Mask |
|------|------|----------|---------|---------------|
| 1 | 0.5h | 0x80 | 0x00 | 0x00 |
| 2 | 1.0h | 0xC0 | 0x00 | 0x00 |
| 3 | 1.5h | 0xA0 | 0x00 | 0x00 |
| 4 | 2.0h | 0xE0 | 0x00 | 0x00 |
| 5 | 2.5h | 0x90 | 0x00 | 0x00 |
| 6 | 3.0h | 0xD0 | 0x00 | 0x00 |
| 7 | 3.5h | 0xB0 | 0x00 | 0x00 |
| 8 | 4.0h | 0xF0 | 0x00 | 0x00 |
| 9 | 4.5h | 0x88 | 0x00 | 0x00 |
| 10 | 5.0h | 0xC8 | 0x00 | 0x00 |
| 11 | 5.5h | 0xA8 | 0x00 | 0x00 |
| 12 | 6.0h | 0xE8 | 0x00 | 0x00 |
| 13 | 6.5h | 0x98 | 0x00 | 0x00 |
| 14 | 7.0h | 0xD8 | 0x00 | 0x00 |
| 15 | 7.5h | 0xB8 | 0x00 | 0x00 |
| 16 | 8.0h | 0xF8 | 0x00 | 0x00 |
| 17 | 8.5h | 0x80 | 0x04 | 0x80 |
| 18 | 9.0h | 0xC0 | 0x04 | 0x80 |
| 19 | 9.5h | 0xA0 | 0x04 | 0x80 |
| 20 | 10.0h | 0xE0 | 0x04 | 0x80 |
| 21 | 11h | 0xD0 | 0x04 | 0x80 |
| 22 | 12h | 0xF0 | 0x04 | 0x80 |
| 23 | 13h | 0xC8 | 0x04 | 0x80 |
| 24 | 14h | 0xE8 | 0x04 | 0x80 |
| 25 | 15h | 0xD8 | 0x04 | 0x80 |
| 26 | 16h | 0xF8 | 0x04 | 0x80 |
| 27 | 17h | 0xC0 | 0x02 | 0x40 |
| 28 | 18h | 0xE0 | 0x02 | 0x40 |
| 29 | 19h | 0xD0 | 0x02 | 0x40 |
| 30 | 20h | 0xF0 | 0x02 | 0x40 |
| 31 | 21h | 0xC8 | 0x02 | 0x40 |
| 32 | 22h | 0xE8 | 0x02 | 0x40 |
| 33 | 23h | 0xD8 | 0x02 | 0x40 |
| 34 | 24h | 0xF8 | 0x02 | 0x40 |

### Timer ON Format (INV2, Family 0x4D)

```
b0 = 0x4D
b1 = 0xB2
b2 = FAN
b3 = INV(FAN)
b4 = CMD = MODE_TEMP ^ 0xC0
b5 = ARG = Base_ARG | ON_Flag
```

### Timer OFF Format (INV2, Family 0x4D)

```
b0 = 0x4D
b1 = 0xB2
b2 = Base_ARG
b3 = INV(Base_ARG)
b4 = MODE_TEMP | Page_Mask
b5 = 0xFE
```

### Timer Examples

- **Timer ON, Cool 22°C, Fan Low, 0.5h**: `4D B2 F9 06 CE 80`
- **Timer OFF, Dry 22°C, 0.5h**: `4D B2 80 7F 2E FE`

## Test Vectors

For encoder validation:

| Command | Expected Frame |
|---------|---------------|
| Cool 22°C, Fan Auto | 4D B2 FD 02 0E F1 |
| Heat 24°C, Fan Auto | 4D B2 FD 02 32 CD |
| Fan-only, Fan Low | 4D B2 F9 06 27 D8 |
| Power Off | 4D B2 DE 21 07 F8 |
| Swing | 4D B2 D6 29 07 F8 |
| LED | AD 52 AF 50 A5 5A |
| Turbo | AD 52 AF 50 45 BA |
| Fresh | AD 52 AF 50 C5 3A |
| Clean | AD 52 AF 50 55 AA |
