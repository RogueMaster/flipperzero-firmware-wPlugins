# Bad Duck3 - DuckyScript 3.0 for Flipper Zero

The first DuckyScript 3.0 implementation for Flipper Zero with USB and Bluetooth HID support.

## Features

### DuckyScript 3.0 Language

Full implementation of DuckyScript 3.0 control flow:

- **Variables**: `VAR $name = value` with integers and strings
- **Loops**: `LOOP N`/`END_LOOP`, `WHILE (condition)`/`END_WHILE`
- **Conditionals**: `IF (condition)`/`ELSE`/`END_IF`
- **Control**: `BREAK`, `CONTINUE`
- **Built-in Variables**: `$_RANDOM`, `$_LINE`
- **Operators**: `+ - * / %` (arithmetic), `< > <= >= == !=` (comparison), `&& ||` (logical)

### Dual HID Interface

Switch between USB and Bluetooth HID modes:

| Interface | Description |
|-----------|-------------|
| **USB** | Standard USB HID - plug and play |
| **BLE** | Bluetooth Low Energy HID - wireless |

Toggle with the **Right** button on the main screen when idle.

### Full Configuration

Access configuration with the **Left** button:

**Keyboard Layout**
- Select from 15+ keyboard layouts
- Proper character mapping for international keyboards

**BLE Settings**
- **Device Name**: Custom Bluetooth device name
- **MAC Address**: Spoof BLE MAC address
- **Pairing Mode**: None, PIN code, PIN + confirm
- **Remove Pairing**: Clear bonded devices

**USB Settings**
- **VID/PID**: Custom Vendor/Product IDs
- **Manufacturer**: Custom manufacturer string
- **Product Name**: Custom product string

## Usage

1. Place `.txt` DuckyScript files in `/ext/badusb/` on your Flipper
2. Open **Bad Duck3** from Apps > USB
3. Select a script file
4. Configure interface (USB/BLE) and settings as needed
5. Press **OK** to run

### Controls

| Button | Idle State | Running State | Paused State |
|--------|------------|---------------|--------------|
| **OK** | Run script | Stop | End |
| **Left** | Config menu | - | - |
| **Right** | Toggle USB/BLE | Pause | Resume |
| **Back** | Exit | - | - |

## DuckyScript 3.0 Examples

### Loop with Counter

```
DELAY 2000
VAR $i = 1
WHILE ($i <= 5)
    STRING Iteration:
    VAR $i = ($i + 1)
    ENTER
    DELAY 300
END_WHILE
STRING Done!
ENTER
```

### Conditional Logic

```
VAR $mode = 1

IF ($mode == 1)
    STRING USB Mode
ELSE
    STRING BLE Mode
END_IF
ENTER
```

### Random Delays

```
REM Human-like typing with random delays
LOOP 10
    STRING Hello World
    ENTER
    VAR $wait = $_RANDOM
    VAR $wait = ($wait % 500)
    VAR $wait = ($wait + 100)
    DELAY $wait
END_LOOP
```

### Nested Loops

```
VAR $row = 0
WHILE ($row < 3)
    VAR $col = 0
    WHILE ($col < 3)
        STRING *
        VAR $col = ($col + 1)
    END_WHILE
    ENTER
    VAR $row = ($row + 1)
END_WHILE
```

## Script Configuration Commands

Scripts can override HID settings:

```
REM USB Configuration
ID 1234:5678
MFR_NAME CustomManufacturer
PROD_NAME CustomProduct

REM BLE Configuration
BLE_NAME MyKeyboard
BLE_MAC 11:22:33:44:55:66
BLE_PAIRING PIN_CONFIRM
```

## Limits

| Resource | Limit |
|----------|-------|
| Nested loops | 8 |
| Nested IF blocks | 8 |
| Variables | 32 |
| Variable name | 16 chars |
| String value | 128 chars |

## Credits

- DuckyScript 3.0 spec by [Hak5](https://docs.hak5.org/hak5-usb-rubber-ducky/)
- Flipper Zero Unleashed by [DarkFlippers](https://github.com/DarkFlippers/unleashed-firmware)
- Author: [@dutchpatriot](https://github.com/dutchpatriot)
- AI-assisted development with Claude Code

## License

GPL-3.0 (same as Unleashed firmware)

## Disclaimer

For authorized security testing and educational purposes only. Always obtain proper authorization before testing on systems you do not own.
