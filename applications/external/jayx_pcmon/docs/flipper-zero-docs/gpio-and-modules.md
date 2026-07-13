<!-- Source: https://docs.flipper.net/zero/gpio-and-modules -->
<!-- Mirrored: 2026-07-14 -->

# GPIO & Modules - Flipper Zero Documentation

## GPIO & modules

Flipper Zero can be utilized for hardware exploration, firmware flashing, debugging, and fuzzing. The device connects to external hardware via built-in GPIO pins, allowing you to control peripherals with buttons, execute custom code, and display debug messages on screen. It also functions as a USB-to-UART/SPI/I2C converter.

### GPIO Pinout

Flipper Zero features 18 pins on the top side, comprising power supply pins and I/O pins. Power pins can supply external modules, while input/output (I/O) pins are "3.3V tolerant for input and output."

I/O pins connect external modules to the STM32WB55 microcontroller through 51-ohm resistors, with all pins featuring electrostatic discharge (ESD) protection.

**+3.3V Power (Pin 9)**
- Output enabled by default
- Maximum load: 1.2A
- Temporarily disabled during firmware updates and microSD card mounting

**+5V Power (Pin 1)**
- Supplied by built-in battery or USB cable
- Output not enabled by default
- To enable: navigate to GPIO in main menu, select "5v on gpio," and toggle on
- Maximum load: 1.2A

### Input/Output Pins

Total power consumption from I/O pins must not exceed 5W to avoid triggering battery protection mode. Each pin can source up to 20mA.

### 3.3V and 5V Tolerance

The STM32WB55 MCU's I/O interface operates at 3.3V. Refer to STMicroelectronics application notes for detailed voltage specifications.

### GPIO Menu

Access the GPIO application from the main menu to configure USB-UART functionality, test individual pins, and enable/disable the +5V power supply.

**Features:**
- USB-UART bridge functionality
- Manual control for testing GPIO pin output
- Configurable pins: PA7, PA6, PA4, PB3, PB2, PC3, PC1, PC0
- 5V power supply toggle for Pin 1

### Installing External Modules

**Without silicone case:** Insert the module completely into the GPIO pin holes until flush—you may need to apply additional force.

**With silicone case:** Insert the module fully to eliminate gaps between the case and module.
