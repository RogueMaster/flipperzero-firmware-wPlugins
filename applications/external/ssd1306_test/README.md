# SSD1306 Test - Flipper Zero OLED Diagnostic Tool

A Flipper Zero application for testing and diagnosing external SSD1306-based 128x64 I2C OLED displays. Supports both monochrome and yellow-bar (dual-color) display variants. Built for Momentum firmware.

## Features

**Clock + Status Board**
- Large 7-segment style clock (HH:MM) with blinking colon and seconds display
- Status bar with battery level, charging indicator, day of week, and date
- Yellow-bar aware layout: on dual-color displays, the status bar renders as dark text on a filled yellow background; on mono displays, a clean line-separated header
- Shows Flipper device name at the bottom
- Updates every second automatically

**Test Patterns**
- All White / All Black (dead pixel detection)
- Checkerboard, Horizontal Lines, Vertical Lines
- Border (edge pixel verification)
- Color Zone Test (highlights the yellow/blue boundary on dual-color displays)
- Page Fill (fills each page sequentially to verify page addressing)
- Dithered Gradient
- Shapes and Text rendering

**Brightness Control**
- Adjustable contrast from 0 to 255 with real-time preview

**Display Commands**
- Invert display
- Horizontal and vertical flip
- Force all pixels on (bypasses RAM)
- Display power on/off
- Variant toggle (monochrome vs yellow-bar)

**Hardware Scrolling**
- Horizontal scroll (left/right)
- Diagonal scroll (vertical + horizontal)
- Adjustable scroll speed (8 levels)

**Display Info**
- Auto-detects I2C address (0x3C and 0x3D)
- Live connection status
- Display variant indicator

## Wiring

Connect the SSD1306 OLED to the Flipper Zero GPIO header:

| OLED Pin | Flipper Zero Pin | GPIO Header |
|----------|-----------------|-------------|
| GND      | GND             | Pin 18      |
| VCC      | 3.3V            | Pin 9       |
| SCL      | C0              | Pin 16      |
| SDA      | C1              | Pin 15      |

> **Note:** Use the 3.3V pin, not 5V. The Flipper Zero GPIO is 3.3V logic. Most SSD1306 modules work fine at 3.3V. If your module requires 5V for its power rail, you may use the 5V pin (pin 1) for VCC only, but the I2C data lines (SCL/SDA) must remain 3.3V-tolerant.

## Building

This project uses [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool) to compile without needing the full firmware source tree. ufbt downloads the SDK and ARM cross-compiler automatically.

### macOS

Requires Python 3 and pip. Install via [Homebrew](https://brew.sh/) if needed (`brew install python`).

```sh
# Install ufbt
pip3 install ufbt

# Clone and enter the repo
git clone https://github.com/martinbogo/ssd1306_test.git
cd ssd1306_test

# Pull the Momentum firmware SDK
make setup

# Build
make
```

The compiled FAP will be at `dist/ssd1306_test.fap`.

### Linux (Debian/Ubuntu)

```sh
# Install prerequisites
sudo apt update
sudo apt install -y python3 python3-pip git

# Install ufbt
pip3 install ufbt

# Clone and enter the repo
git clone https://github.com/martinbogo/ssd1306_test.git
cd ssd1306_test

# Pull the Momentum firmware SDK
make setup

# Build
make
```

If `pip3 install` warns about the PATH, either add `~/.local/bin` to your PATH or use `pipx install ufbt` instead.

For USB access to the Flipper without root, add a udev rule:

```sh
sudo tee /etc/udev/rules.d/42-flipperzero.rules << 'UDEV'
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="5740", TAG+="uaccess"
UDEV
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Windows

Requires Python 3. Download from [python.org](https://www.python.org/downloads/) and ensure "Add Python to PATH" is checked during install.

```powershell
# Install ufbt
pip install ufbt

# Clone and enter the repo
git clone https://github.com/martinbogo/ssd1306_test.git
cd ssd1306_test

# Pull the Momentum firmware SDK
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json

# Build
ufbt
```

On Windows the Makefile requires `make` (available via [chocolatey](https://chocolatey.org/): `choco install make`, or via MSYS2/Git Bash). Alternatively, run `ufbt` directly as shown above.

## Deploying to the Flipper Zero

### USB deploy (all platforms)

Connect your Flipper Zero via USB. Close any other application using the serial port (qFlipper, Flipper Lab, serial terminals).

```sh
# macOS / Linux
make launch

# Windows (or any platform without make)
ufbt launch
```

This uploads the FAP and immediately launches it on the Flipper.

### Manual SD card install

Copy `dist/ssd1306_test.fap` to your Flipper Zero's SD card:

```
SD Card/apps/GPIO/ssd1306_test.fap
```

Then navigate to **Apps > GPIO > SSD1306 Test** on the Flipper.

## VSCode Integration

To set up full IntelliSense (autocomplete, go-to-definition into Flipper SDK headers):

```sh
make vscode
```

This generates `.vscode/c_cpp_properties.json` and `compile_commands.json` pointing at the Momentum SDK include paths and the ARM cross-compiler.

## Makefile Targets

| Target       | Description                                |
|--------------|--------------------------------------------|
| `make`       | Build the FAP                              |
| `make launch`| Build and deploy to connected Flipper      |
| `make clean` | Remove build artifacts                     |
| `make setup` | Install ufbt and pull Momentum SDK         |
| `make update`| Update Momentum SDK to latest              |
| `make vscode`| Regenerate VSCode IntelliSense config      |
| `make lint`  | Lint source files                          |
| `make format`| Auto-format source files                   |

## Using the Official Flipper SDK

If you prefer to build against the official Flipper Zero firmware instead of Momentum, replace the SDK URL:

```sh
ufbt update --channel=release
```

The app uses standard Flipper Zero SDK APIs and is compatible with both firmwares.

## License

MIT
