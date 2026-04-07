# Changelog

All notable changes to the Flight Monitor project will be documented in this file.

## Version 1.0.0 - 2026-04-06

**Initial Release**

**Features:**
- **Real-time flight data display:** Altitude, speed, vertical speed, throttle, flaps, gear status, pitch/roll
- **Multiple view modes:** Main dashboard, throttle focus, flaps focus, orientation display
- **Configurable alarm system:**
  - Altitude warning (default: 120m when descending)
  - Gear warning (default: 150m with gear down)
  - Gear retraction alert (default: 200m with gear down at speed >150 km/h)
  - Stall speed warning (configurable threshold)
  - Overspeed warning (configurable threshold)
- **Safety features:**
  - Engine failure detection (power <100HP + falling + altitude >100m)
  - G-force alerts with vibration (pitch >60 deg or roll >70 deg at speed >200 km/h)
  - Automatic crash detection (data unchanging -> alarms stop)
- **Professional UI:**
  - 3-second startup splash screen with airplane logo
  - Settings menu with scroll bar (BME680-style)
  - Persistent configuration saved to /ext/apps_data/flight_monitor/settings.cfg
  - Clean status messages ("Waiting for data", "No flight data")
- **War Thunder integration:**
  - BLE connectivity via custom Python server
  - 100ms data refresh rate (10 Hz)
  - JSON API polling from localhost:8111/state
- **Python server tools:**
  - GUI server with tkinter interface
  - IP configuration with persistence
  - Auto-start functionality
  - Auto-dependency installation (requests, bleak)
  - Visual status indicators and logging

**Technical Details:**
- Platform: Flipper Zero (f7 firmware)
- Language: C (Flipper app), Python 3.x (server)
- BLE Protocol: Custom binary format (20 bytes)
- Data rate: 100ms refresh interval
- Configuration: Binary file with 5 user-adjustable thresholds
- Icon: Custom 10x10 airplane sprite

**Developer:** Created by Dr.Mosfet for the War Thunder and Flipper Zero communities.

**Version History:**

- **v1.0.0** (2026-04-06): Initial public release with full feature set
