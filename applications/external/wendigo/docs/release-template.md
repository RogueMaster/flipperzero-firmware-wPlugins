**This is an interim release** - It provides discovery and display of WiFi Access Points and Stations (client devices), but implementation revealed some significant problems with how Flipper-Wendigo manages its UART buffer and parses Wendigo packets (sent from ESP32 to Flipper). This results in a large volume of Wendigo packets being corrupted or dropped, particularly when all radios are enabled or the spectrum is very busy. Disabling device types you are not interested in from the setup menu will minimise this.

### This release contains
* 2.4GHz WiFi Access Point and Station scanning and discovery, extracting information from the following 802.11 packet types:
  * Beacon
  * Probe Request
  * Probe Response
  * Request-to-Send (RTS)
  * Clear-to-Send (CTS)
  * Deauthentication
  * Disassociation
  * Data
* 2.4GHz WiFi channel hopping, with configurable channels
* Device type filters to select specific device types for display
* Semaphore in ESP32-Wendigo to prevent packets being interleaved
* Updated Version command to display both Flipper and ESP32 version information
* (Temporary change only) **Scanning is stopped when displaying the device list**
  * This is to work around current problems with UART buffer management and Wendigo packet parsing; dynamic device list updates will return in v0.3.0

**Full Changelog**: https://github.com/chris-bc/wendigo/compare/v0.1.0...v0.2.0

### What's Next in v0.3.0?
* Rewritten Flipper-Wendigo UART buffer management and packet parsing
* Re-enabled dynamic updates to device list
* Display WiFi AP authentication mode (WEP, WPA, etc.)
* Possibly other features from the feature list and/or roadmap available in [the Wendigo README](https://github.com/chris-bc/wendigo/blob/main/README.md)

### Installation from binaries
* Extract wendigo-esp32-0.2.0.tgz and run ```flash.sh``` to flash your ESP32
  * This binary is built for the standard ESP32 chipset. If using a different ESP32 model you'll need to compile it yourself as described below.
* Copy ```wendigo.fap``` to your Flipper Zero
  * There are several ways to do this, one is to copy it to /apps/GPIO/ESP32/ on the SD Card

### Installation from source

1. Check out the tagged version of the Wendigo repository:

```sh
git clone https://github.com/chris-bc/wendigo.git
cd wendigo
git checkout tags/v0.2.0
```

These instructions assume that Wendigo has been cloned to ```~/wendigo```.

#### ESP32-Wendigo

2. If you don't already have it, clone and install ESP-IDF, Espressif's IoT Development Framework:

```sh
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
```

These instructions assume that ESP-IDF has been cloned to ```~/esp-idf```.

3. Connect your ESP32, compile and flash ESP32-Wendigo:

```sh
cd ~/esp-idf
source ./export.sh
cd ~/wendigo/esp32
idf.py set-target <ESP32 chipset>
idf.py build flash
```

```<ESP32 chipset>``` can be ```esp32```, ```esp32-s2```, ```esp32-c5```, etc.

#### Flipper-Wendigo

4. If you don't already have it, clone your favourite Flipper Zero firmware and install the build toolchain. Here I'm using Momentum Firmware:

```sh
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware
./fbt
```

These instructions assume that Flipper Zero firmware has been downloaded to ```~/Momentum-Firmware```.

5. Link Flipper-Wendigo into the firmware:

```sh
ln -s ~/wendigo/Flipper ~/Momentum-Firmware/applications_user/wendigo
```

##### Option 1: The downloaded firmware is already installed on the Flipper Zero and you only need to flash Wendigo

6. Connect your Flipper Zero and install Wendigo:

```sh
cd ~/Momentum-Firmware
source `./fbt -s env`
./fbt launch APPSRC=wendigo
```

##### Option 2: Flipper Zero is not running the same firmware and firmware version that you downloaded

6. Connect your Flipper Zero and install the firmware, including Wendigo:

```sh
cd ~/Momentum-Firmware
source `./fbt -s env`
./fbt firmware_all flash_usb_full
```

