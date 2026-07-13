<!-- Source: https://docs.flipper.net/zero/video-game-module/gpio -->
<!-- Mirrored: 2026-07-14 -->

# GPIO on Video Game Module

## Video Game Module Pinout

The Video Game Module features a 14-pin GPIO breakout from the RP2040 microcontroller for external device interaction. It also includes an 18-pin connector for Flipper Zero compatibility.

### Pin Allocation

**Reserved for Internal Use:**
- GPIO pins 8-15 and 18-20 are used for video output and are not accessible via connectors
- GPIO pins 23-25 and 29 are used internally by the Raspberry Pi Pico board

**Exposed Pins:**
- GPIO pins 0-7, 16, 17, 21, 22, and 26-28 are accessible at the connectors
- Pins 2-7 connect to the motion tracking sensor with limited functionality

## Available Peripherals

In standalone mode, the RP2040 microcontroller provides access to:

- **Video Out Port** — outputs video and audio to TV/monitor; supports Display Data Channel for data exchange
- **USB-C Port** — functions as USB device or host (USB Power Delivery not supported)
- **Motion Tracking Sensor** — TDK ICM42688-P IMU with 3-axis gyroscope and accelerometer
- **GPIO Breakout** — 11 fully functional pins with ESD protection; pins 13, 14, and 2-7 have limited functionality
- **RGB LED** — controllable via RP2040 GPIO pins

## Power Supply Options

The module can be powered via:
- 5V USB-C when operating as standalone device
- 3.3V (pin 9) or 5V (pin 1) when connected to Flipper Zero
