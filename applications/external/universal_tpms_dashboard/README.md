# Universal TPMS Dashboard

A Flipper Zero Momentum app for monitoring, pairing, configuring, and transmitting TPMS sensor frames.

> **BETA TESTING — WORK IN PROGRESS**
>
> This application is currently in beta testing. Features and protocol support are still being tested and may be incomplete or unreliable. Some protocols are verified, while others remain experimental. Compatibility with all sensors and vehicles is not guaranteed.
>
> Use for research and authorized testing only. Do not rely on this application for vehicle safety or roadworthiness decisions.

This app is a porting of the existing Nissan TPMS Dashboard to the sensors present in TPMS-Editor

https://github.com/skylovervibecoder/Flipper-Zero-Nissan-TPMS-Dashboard

![rx](rx.png)

------------

![tx](tx.png)

## Features

- **Monitor:** Receive supported TPMS transmissions and display sensor pressure and temperature.
- **Transmit:** Send TPMS frames for individual wheels or all wheels.
- **Pair Sensors:** Discover supported sensors and assign them to vehicle wheels.
- **Settings:** Configure sensor IDs, protocols, radio profiles, and vehicle settings.
- **Vehicle Profiles:** Save, load, switch, and delete vehicle configurations.
- **Automatic Detection:** Scan supported protocols and radio profiles.
- **TPMS-Editor Support:** Read compatible `.tpms` profiles for additional transmission parameters.
- **Legacy Compatibility:** Migrate compatible configurations from the original Nissan TPMS Dashboard.

## Supported Protocols

**Native:** Nissan / Renault.

**Verified:** Renault, Toyota / Pacific, Schrader, Schrader EG53MA4, VDO / PSA / FCA, Ford.

**Under Testing:** PMV-107J, VDO TG1C, Q85, Mercedes Sprinter.

**Experimental:** BMW Gen2 / Gen3, Elantra / Honda, Hyundai VDO, Truck, Renault 0435R, Honda TRW, Porsche, Kia.

Protocol support may vary by sensor model, frequency, and transmission format. Experimental protocols may not work reliably.

## Main Menu

- **Monitor** — Listen for supported TPMS transmissions.
- **Transmit** — Transmit configured TPMS frames.
- **Pair Sensors** — Detect sensors and assign them to wheels.
- **Settings** — Configure IDs, protocols, radio profiles, and vehicles.
- **About** — Display application information.

## Wheel Positions

- `FL` — Front Left
- `FR` — Front Right
- `RL` — Rear Left
- `RR` — Rear Right

## Controls

### Main Menu
- Up / Down: Navigate.
- OK: Open the selected section.
- Back: Exit the application.
- Left / Right: Switch between saved vehicle profiles.

### Monitor
- Left / Right: Select a wheel.
- Hold OK: Reset the selected wheel's displayed data.
- Back: Return to the main menu.

### Transmit
- Left / Right: Select a wheel or `RESET`.
- Up / Down: Adjust pressure by 1 kPa.
- Hold Up / Down: Adjust pressure by 5 kPa.
- OK: Transmit the selected wheel.
- Hold OK: Transmit all wheels.
- `RESET` + OK: Apply default pressures of 230 kPa front and 210 kPa rear, then transmit all wheels.

### Pair Sensors
- Up / Down: Select a detected sensor.
- Left / Right: Assign it to a wheel.
- OK: Save assignments when all four wheels are assigned.
- Hold OK: Clear the detected sensor list.
- Back: Return to the main menu.

### Settings
- Up / Down: Select a setting.
- OK: Open or apply an option.
- Left / Right: Change available protocol or radio options.
- Save: Save the current vehicle configuration.
- Save As: Save under a new vehicle name.
- Load Vehicle: Load an existing profile.
- Left on a selected vehicle: Delete that profile.
- ID Editor: Edit sensor ID digits.

**Note:** Changing the protocol resets sensor IDs to default placeholders. Pair the sensors again or enter their IDs manually.

## Radio Profiles

- **AUTO:** Attempts to cycle through supported radio profiles in Monitor or Pair mode.
- **Fixed Profile:** Uses the selected radio profile.
- **Native Nissan / Renault:** Uses 433.92 MHz FSK.

Automatic detection cannot guarantee compatibility with every sensor.

## File Storage

Vehicle configurations:

`/ext/apps_data/universal_tpms_dashboard/cars/`

Application configuration:

`/ext/apps_data/universal_tpms_dashboard/config.txt`

TPMS-Editor profiles:

`/ext/apps_data/tpms_editor/`

Legacy Nissan Dashboard data:

`/ext/apps_data/nissan_dashboard/`

Vehicle profiles use the `.car` extension. Compatible legacy configurations may be migrated from the original application directory.

## Installation

1. Set up a compatible Flipper Zero Momentum development environment.
2. Compile the application with its required source files, including `utd_proto.c` and the corresponding header.
3. Install the compiled application on your Flipper Zero.
4. Open Universal TPMS Dashboard.
5. Configure the protocol and radio profile in Settings.
6. Pair sensors or enter their IDs manually.
7. Save the vehicle configuration before testing.

Build requirements may vary depending on the Momentum firmware version and development environment.

## Beta Testing Notice

This project is actively being developed and tested. Known limitations may include incomplete protocol decoding, incorrect readings, inconsistent automatic detection, transmission incompatibilities, and bugs in configuration handling.

Features and protocol implementations may change between beta releases. Bug reports, reproducible test cases, known sensor models, and documented captures are welcome.

Do not use this application as a replacement for the vehicle's original TPMS monitoring system.

## Credits

- Original project: Nissan TPMS Dashboard
- Protocol reference: TPMS-Editor-EN by `altruista86`
- Core ProtoView reference: `antirez`
- Application development: Marcopiiit

## Disclaimer

This project is intended for educational, development, and authorized testing purposes. Use radio transmission features only where legally permitted and with equipment you are authorized to test. The developer is not responsible for damage, misuse, or consequences resulting from use of this software.

**Version status: Beta — testing and development ongoing.**
