# Hardware Connections Guide

This document provides comprehensive hardware connection information for ESP Serial Flasher across different communication interfaces. For platform-specific setup instructions, see the [Platform Setup Guide](platform-setup.md).

## Overview

ESP Serial Flasher supports multiple communication interfaces between host and target devices:

- **UART/Serial**: Standard serial communication (most common)
- **SDIO**: High-speed SD card interface
- **SPI**: Serial Peripheral Interface
- **USB CDC ACM**: USB Communication Device Class

Each interface has specific hardware requirements and connection patterns detailed below.

## UART/Serial Interface

### Description

The UART interface is the most commonly used method for ESP device programming. It uses a standard 4-pin connection for basic communication plus power.

### Connection Pattern

The standard UART connection requires 4 signal pins:

| Signal |   Host Pin   |  Target Pin  | Description                           |
| :----: | :----------: | :----------: | :------------------------------------ |
|   TX   | Host UART TX |  Target RX0  | Data transmission from host to target |
|   RX   | Host UART RX |  Target TX0  | Data reception from target to host    |
| RESET  |  Host GPIO   | Target RESET | Target device reset control           |
|  BOOT  |  Host GPIO   | Target BOOT  | Boot mode selection (download mode)   |

### General Requirements

- **Power Supply**: Both devices need independent power sources or shared power rails
- **Voltage Levels**: Ensure compatible logic levels (3.3V recommended)
- **Ground Connection**: Common ground between host and target
- **USB Cables**: For host programming and target power (if needed)

### Interface Limitations

- **ESP8266**: Does not support baud rate changes after connection
- **Speed**: Limited by UART baud rate

## SDIO Interface

### Description

SDIO (Secure Digital Input Output) interface provides high-speed communication using the SD card protocol. Currently supported for specific chip combinations.

### Connection Pattern

The SDIO connection requires 5 pins plus UART for monitoring:

| Signal  |   Host Pin    |   Target Pin    | Description                 |
| :-----: | :-----------: | :-------------: | :-------------------------- |
|   CLK   | Host SDIO CLK |   Target CLK    | Clock signal                |
|   CMD   | Host SDIO CMD |   Target CMD    | Command/response line       |
|   D0    | Host SDIO D0  |    Target D0    | Data line 0                 |
|  RESET  |   Host GPIO   |  Target RESET   | Target device reset control |
|  BOOT   |   Host GPIO   |   Target BOOT   | Boot mode selection         |
| UART_RX | Host UART RX  | Target UART0_TX | Monitor output from target  |
| UART_TX | Host UART TX  | Target UART0_RX | Send commands to target     |

**4-bit Mode Pins (Optional)**: Additional pins for 4-bit SDIO interface:

| Signal |   Host Pin   | Target Pin | Description              |
| :----: | :----------: | :--------: | :----------------------- |
|   D1   | Host SDIO D1 | Target D1  | Data line 1 (4-bit mode) |
|   D2   | Host SDIO D2 | Target D2  | Data line 2 (4-bit mode) |
|   D3   | Host SDIO D3 | Target D3  | Data line 3 (4-bit mode) |

### Hardware Requirements

> [!IMPORTANT]
> SDIO pins CMD and DAT0-3 **may require pullup resistors** depending on your hardware setup.

**Pullup Guidelines**:

- **Resistor Values**: 10kΩ to 47kΩ when needed
- **Pins**: CMD, D0 (required), D1, D2, D3 (when using 4-bit mode)
- **Power Rail**: Connect pullups to target device's VDD (3.3V)
- **Note**: Some development boards may work without external pullups, but adding them improves signal integrity

For detailed pullup requirements, see [ESP-IDF SD Pullup Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sd_pullup_requirements.html).

**Clock Edge Control**:

- **Sampling and Driving Clock Edge**: Based on your hardware design, you may need to configure the SDIO sampling and driving clock edge control
- **Configuration**: These settings control when data is sampled and driven relative to the clock edge
- **Hardware Dependent**: Required values depend on your specific board layout, trace lengths, and signal integrity requirements
- **Documentation**: Clock edge control details can be found in the target chip's datasheet under the Boot Configurations section

## SPI Interface

### Description

SPI (Serial Peripheral Interface) provides high-speed communication for loading applications directly into RAM. Supports both standard SPI and Quad-SPI modes.

### Connection Pattern

The SPI connection requires multiple pins for basic functionality:

|  Signal  |   Host Pin    |  Target Pin  | Description                       |
| :------: | :-----------: | :----------: | :-------------------------------- |
|   CLK    | Host SPI CLK  |  Target CLK  | Clock signal                      |
|    CS    |  Host SPI CS  |  Target CS   | Chip select                       |
|   MISO   | Host SPI MISO | Target MISO  | Master in, slave out              |
|   MOSI   | Host SPI MOSI | Target MOSI  | Master out, slave in              |
|  RESET   |   Host GPIO   | Target RESET | Target device reset control       |
| STRAP_B0 |   Host GPIO   | Target STRAP | Strapping bit 0 for download mode |
| STRAP_B1 |   Host GPIO   | Target STRAP | Strapping bit 1 for download mode |
| STRAP_B2 |   Host GPIO   | Target STRAP | Strapping bit 2 for download mode |
| STRAP_B3 |   Host GPIO   | Target STRAP | Strapping bit 3 for download mode |

**Quad-SPI Pins (Optional)**: Additional pins for faster transfer rates:

| Signal |    Host Pin     |  Target Pin   | Description             |
| :----: | :-------------: | :-----------: | :---------------------- |
| QUADWP | Host SPI QUADWP | Target QUADWP | Quad mode write protect |
| QUADHD | Host SPI QUADHD | Target QUADHD | Quad mode hold          |

### Interface Notes

- **Strapping Pins**: Specific to each chip - consult target's Technical Reference Manual
- **Quad-SPI**: Enables higher data throughput when supported

## USB CDC ACM Interface

### Description

USB CDC ACM (Communication Device Class Abstract Control Model) uses the target device's USB interface for programming. No additional hardware connections required.

### Connection Pattern

**Single USB Connection**: Host to target via USB cable only.

### Hardware Requirements

- **USB OTG Adapter**: Required for host device
- **Power Considerations**:
  - USB connector on most boards cannot supply sufficient power
  - **Separate power connection required** for target device
- **USB Cables**: Quality USB cables for reliable communication

### Compatibility

**USB CDC ACM Support**:

- **Host**: Requires USB Host peripheral support
- **Target**: Requires USB Serial/JTAG peripheral support

### Interface Limitations

- **Baud Rate**: USB CDC ACM does not support baud rate changes
- **Download Mode**: Manual entry required if target firmware uses USB OTG peripheral
- **Power Supply**: Target requires independent power source

### USB Host Driver Requirements

When using ESP-IDF as host platform:

- **ESP-IDF Version**: v5.5 or newer required
- **USB Host Driver**: Must be initialized and configured
- **Task Management**: FreeRTOS task required for USB event handling
- **Device Management**: Proper connection/disconnection handling needed

## General Hardware Considerations

### Power Supply

- **Independent Power**: Each device should have its own stable power source
- **Voltage Compatibility**: Ensure matching logic levels (typically 3.3V)
- **Current Capacity**: Sufficient current for both devices during operation
- **Ground Connection**: Shared ground reference between all devices

### Signal Integrity

- **Cable Length**: Keep connections as short as practical
- **Cable Quality**: Use quality jumper cables or custom harnesses
- **Interference**: Avoid running power and signal cables in parallel
- **Termination**: Follow interface-specific requirements (e.g., SDIO pullups)

### Development Boards

- **Pin Availability**: Ensure required pins are accessible
- **Conflicting Features**: Check for onboard peripherals that might interfere
- **Reset Circuits**: Some boards have reset circuits that may affect timing
- **USB Connectors**: Verify USB connector type and power delivery capability

### Debugging Connections

Many examples support additional UART connections for debugging:

- **Separate UART**: Dedicated debug UART independent of programming interface
- **USB-to-UART Bridge**: For host platforms without direct PC connection
- **Monitor Output**: Real-time target device output during and after programming

## Troubleshooting

### Common Connection Issues

1. **No Communication**: Check power, ground, and pin assignments
2. **Intermittent Failures**: Verify cable quality and length
3. **Speed Issues**: Confirm baud rate support and cable integrity
4. **Reset Problems**: Check reset circuit and timing

### Interface-Specific Issues

- **SDIO**: Verify pullup resistors are installed and correct values
- **SPI**: Confirm strapping pins are correctly connected
- **USB**: Check power supply and USB host driver configuration
- **UART**: Verify TX/RX are not swapped

For platform-specific build and setup issues, refer to the [Platform Setup Guide](platform-setup.md).
