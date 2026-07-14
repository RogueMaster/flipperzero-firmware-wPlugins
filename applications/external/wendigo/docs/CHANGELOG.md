## Wendigo Release History

### v0.5.0

* Because crowded areas get poor results running BLE and WiFi simultaneously, restructured menu to simplify running one or the other.
* Partial implementation of dditional 802.11 packets
  * Association request
  * Association response
  * Reassociation request
  * Reassociation response
  * Currently only extracting basic device information, needs further attention to extract SSID and potentially other attributes.
* View and change the ESP32 WiFi MAC and Bluetooth Device Address (BDA - Bluetooth's equivalent of a MAC)
* Probe ESP32 for supported features, including:
  * Bluetooth Classic
  * Bluetooth Low Energy
  * WiFi (2.4GHz)
  * WiFi (5GHz)
  * Bluetooth UUID dictionary (ESP32-Wendigo can be configured to exclude this if storage is limited)
* MAC get/set functions will only get or set the MAC for a supported interface
* Refactoring all functions to use this will take some time.

The stability of this release isn't brilliant - After a few minutes of scanning the application "hangs". I was initially reluctant to release this, but I wanted to merge the MAC features into the main branch before starting to debug it, and I have a feeling this problem was present in v0.4.0. Hopefully v0.5.1 will follow shortly.

**Full Changelog**: https://github.com/chris-bc/wendigo/compare/v0.4.0...v0.5.0

### v0.4.0

* Introduced mutexes to manage concurrency during packet transimission and reception
  * Concurrency issues - multiple threads sending or receiving packets at once - were the reason for packet corruption and all the headaches I've had trying to work around it.
  * New mutex to guarantee only a single ESP32-Wendigo thread can transmit a packet at a time
  * New mutex to guarantee that a single thread will work on Flipper-Wendigo's UART buffer at a time
  * I'd like to manage concurrency of Flipper-Wendigo's device cache as well, but want to implement a read-write lock for that, and that'll take a little time
  * This change has eliminated practically all instability of both apps - I've still made mistakes, but you have to look harder to find them!
* A WiFi station's Preferred Network List (PNL), commonly called "saved networks", are collected and displayed
  * Device list display for stations includes a new menu option ```N networks``` (where N is a number).
  * Select this to view a list of the SSIDs the station has sent probe requests for.
* New menu option to view all probed networks and the number of probed SSIDs
  * Selecting this option will display all probed SSIDs and the number of devices searching for each SSID.
    * From this view, selecting an SSID will display all stations that probed for that SSID.
    * This required introducing a second data model. It gets a mutex too!
* Navigate from a WiFi Station to its AP by selecting the AP menu option and pushing ```OK```.
  * This is a new Device List display, but displaying only the AP
  * You can select and view attributes in the same way as the device list
    * You can even view the AP's stations, select the station you were previously looking at, view its AP, view the AP's stations, ...
  * Use the ```back``` button to restore the previous view
* Navigate from a WiFi Access Point to its Stations by selecting the menu option labelled ```N stations```.
  * A new Device List will be displayed containing the Stations of the selected AP
  * As above, this is a fully-functional device list.

That just about wraps it up for 2.4GHz WiFi! The AP authentication mode is a joke and needs to be fixed, and there are some tweaks and quality-of-life improvements I'd like to make, but I should start thinking about the next feature to tackle. If anyone is reading this and wants to make a request I'm all ears - Bluetooth service discovery; sending beacon and deauth packets; displaying status information with the LED; 5GHz WiFi; MAC spoofing; detailed device view; sorting the device list; ...

**Full Changelog**: https://github.com/chris-bc/wendigo/compare/v0.3.0...v0.4.0

### v0.3.0

* Completely rewritten UART receiver buffer in Flipper-Wendigo
* Completely rewritten packet parsing in Flipper-Wendigo
* 2.4GHz WiFi Station and Access Point scanning
* Dynamic updating of the device list
* Access point authentication mode is displayed
  * Security elements of the packets aren't being interpreted correctly, the displayed authentication mode is wrong and needs to be fixed.

**Full Changelog**: https://github.com/chris-bc/wendigo/compare/v0.2.0...v0.3.0

### v0.2.0

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
* Updated ```version``` command to display both Flipper and ESP32 version information
* (Temporary change only) **Scanning is stopped when displaying the device list**
  * This is to work around current problems with UART buffer management and Wendigo packet parsing; dynamic device list updates will return in v0.3.0.

**Full Changelog**: https://github.com/chris-bc/wendigo/compare/v0.1.0...v0.2.0

### v0.1.0

* Discover and display summary information for Bluetooth Classic and Low Energy devices
* Displayed information includes MACA, device name, RSSI, device type (Classic or LE), Class of Device and time since last seen
* Devices can be "tagged" to display only devices of interest
* Scanning Status view that provides information and metrics about the scanner's capability and discovered devices.
