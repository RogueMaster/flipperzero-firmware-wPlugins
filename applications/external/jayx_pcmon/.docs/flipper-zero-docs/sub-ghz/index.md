<!-- Source: https://docs.flipper.net/zero/sub-ghz -->
<!-- Mirrored: 2026-07-14 -->

# Sub-GHz - Flipper Zero Documentation

## Sub-GHz

Flipper Zero can receive and transmit radio frequencies between 300-928 MHz using its built-in module. This capability allows users to read, save, and emulate remote controls for gates, barriers, radio locks, switches, wireless doorbells, and smart lights. The documentation notes that "Flipper Zero can help you learn if your security is compromised."

### Sub-GHz Menu

The application offers several key functions:

- **Read** - Decodes signals based on known protocols and saves static signals
- **Read Raw** - Captures signals in raw format, including unknown protocols
- **Saved** - Manages previously recorded signals for emulation and renaming
- **Add Manually** - Creates virtual remote controls for pairing with readers
- **Frequency Analyzer** - Determines transmission frequencies
- **Region Information** - Displays regional frequency allowances
- **Radio Settings** - Switches between internal and external antennas

### Sub-GHz Hardware

The system uses "a CC1101 transceiver and a radio antenna with maximum range of 50 meters." The hardware operates across three frequency bands: 300-348 MHz, 387-464 MHz, and 779-928 MHz. The documentation mentions the application also supports external radio modules based on the CC1101 transceiver.
