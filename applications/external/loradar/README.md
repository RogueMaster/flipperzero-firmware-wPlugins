# LoRadar - The Flipper Application to localize stratospheric ballons via LoRa

LoRadar is an app that allows users to localize stratospheric balloons using their Flipper Zero and LoRa modules.

The Flipper receives LoRa APRS-IS frames directly from the balloons and forwards it to a [companion mobile app](https://github.com/nahel-b/flipperApp) via Bluetooth.
The mobile app then records the balloon positions, displays them on a map, and can optionally forward the data to a backend server.


> ⚠️ Currently tested on Flipper Zero Firmware SDK **v1.2.0**  
> Compatibility with other versions is not guaranteed – firmware update may be required.

## App structure
```
loradar/
├── application.fam # Flipper app manifest file
├── lora_app.c / .h # Main app entry and global logic
├── bt_transmitter.c / .h
├── lora_config.c / .h
├── lora_custom_event.h
├── lora_receiver.c / .h
├── lora_receiver_i.h 
├── lora_transmitter.c / .h
├── lora_transmitter_i.h # (private header)
├── lora_state_manager.c / .h
├── ring_buffer.c / .h
├── uart_helper.c / .h
├── lora_10px.png # App icon
├── helpers/ # Utility code (e.g., drawing, timing)
├── scenes/ # UI scene management (Flipper UI structure)
├── tests/ # Unit tests
├── doc/ # Project documentation including UML diagrams
└── README.md
```

## Supported LoRa Modules

| Module Name          | Chipset       | Compatible | Notes                                               |
|----------------------|---------------|------------|-----------------------------------------------------|
| Grove - Wio-E5       | STM32WLE5JC   | ✅         | Tested and working over UART with Flipper GPIO      |


More modules can be added (see the contribute guideline)

## Connection Instructions

### Grove-Wio-E5 Module
To connect the Grove-Wio-E5 module to the Flipper zero, simply connect the pins as follows: 

**Pinout: Grove cable on Flipper GPIO**
- `Red` → `3V3` (pin 9)
- `Black` → `GND` (pin 11)
- `White` → `TX` (pin 13)
- `Yellow` → `RX` (pin 14)

**Default baudrate**: 9600

## Build Instructions

There are two build modes: **Debug** (default) and **Release**.

```sh
./fbt COMPACT=1 APPSRC={app_id}      # Debug mode
./fbt COMPACT=1 DEBUG=0 APPSRC={app_id}  # Release mode
```

The `{app_id}` corresponds to the `app_id` specified in the `application.fam` file. 
This project contains two apps: the main application and a test suite.

To build the main app in **Release mode**, use:

```sh
./fbt COMPACT=1 DEBUG=0 APPSRC=lora_app
```

## Deploying the App

To upload and launch the compiled app via USB, run:

```sh
./fbt DEBUG=0 launch APPSRC=lora_app
```

To **build** and **flash** all FAP apps in one command:

```sh
./fbt DEBUG=0 fap_deploy  # Release mode
```

## Debugging

To view log traces from the device's CLI, use:

```sh
./fbt cli
```


## TODO

- [x] Basic UART communication with Wio-E5
- [x] Display LoRa response in Flipper UI
- [x] Bluetooth connection with mobile app
- [x] Map display of balloon location in app
- [x] Add settings menu in Flipper app
- [ ] Add an internal APRS-IS trames parser
- [ ] Update the app to work with new SDK firmware.



## License

This project is open source and available under the [MIT License](./LICENSE).
