# Flipper Zero apps

This folder holds the two apps that run on the Flipper.

- [expo_app/](expo_app/) is the FliPort data collector. You use it to record labeled motion (drop, idle, walking, fidget) to the SD card. Those recordings are what the model trains on.

- [drop-dect/](drop-dect/) is the live detector. It loads the trained model, watches the motion sensor, and sounds a siren when it decides the device has been dropped. It comes as a normal app and as a background service for custom firmware.

Both read the same ICM42688P motion sensor over SPI at 100 Hz, and both share the same driver in their `driver/` folder.

Start with the collector to gather data, train on the PC (see [../model/readme.md](../model/readme.md)), then run the detector. Each folder has its own readme.
