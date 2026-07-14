<!-- Source: https://docs.flipper.net/zero/basics/power -->
<!-- Mirrored: 2026-07-14 -->

# Power - Flipper Zero Documentation

## Power

Flipper Zero features an integrated lithium-ion polymer rechargeable battery with 2100 mAh capacity. The device can operate for up to one month without recharging. This guide covers powering on, powering off, charging, and power modes.

### Powering On

Press and hold the back button for three seconds to power on your Flipper Zero.

### Charging

Connect the provided USB Type-C cable to the charging port and a power source. Full charge takes approximately two hours.

To view battery information, navigate to: **Main Menu > Settings > Power > Battery Info**

### Powering Off

To power off your device, go to: **Main Menu > Settings > Power > Power Off** and confirm by pressing the right button.

### Power Modes

Flipper Zero operates in two modes with different power consumption rates:

#### Active Mode
When running applications or establishing connections, the device consumes up to 30 mA with backlight. Active transceivers can increase consumption to 400 mA, or 2 A with external modules connected.

#### Sleep Mode
When no applications run or connections exist, the device enters sleep mode consuming approximately 1.5 mA.

**Two sleep modes available:**

- **Default**: ~1.5 mA consumption, longer battery life (may experience occasional crashes)
- **Legacy**: ~9 mA consumption, shorter battery life but greater stability

Switch to legacy mode via: **Main Menu > Settings > System** and set sleep method to Legacy.

### Tips for Maximizing Battery Performance

**For longer battery life:**
- Update to the latest firmware version
- Optimize settings (Bluetooth, display, system parameters)

**For longer battery lifespan:**
- Avoid temperatures outside 0°C to 40°C (32°F to 104°F)
- Store the device at approximately 50% charge when powered off for extended periods

### Power System Specifications

| Parameter | Value |
|-----------|-------|
| Battery capacity | 2100 mAh |
| Battery life | 1 month |
| Charge time | 2 hours |
| Power connector | USB Type-C |
| Charging voltage and current | 5V, 1A |
| Battery operating voltage | 3.3-4.2V |
| Pin 1 voltage (USB power) | 5V |
| Maximum USB cable power supply current | 3A |
