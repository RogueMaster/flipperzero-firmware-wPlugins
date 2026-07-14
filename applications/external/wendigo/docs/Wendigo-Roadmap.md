## Wendigo Roadmap

This section is a running list of current priorities.

* [ ] ESP32 tag command has a radio arg, doesn't need it - parse_command_tag()
* [ ] Add "m" command as a shortcut to mac
* [ ] BUG: Wendigo "hangs" after several minutes of scanning
  * [ ] Scanning for a short period, stopping it, and spending a long time exploring discovered devices doesn't cause any issues so this is definitely related to scanning
  * [ ] Could the UART thread be deadlocked?
  * [ ] Run with debugger and trace log to find clues
* [ ] Device List scene uses plurals when it shouldn't
  * [ ] 1 Networks
  * [ ] 1 Stations
* [ ] Device List scene doesn't remember selected options
  * [ ] e.g. Selecting a STA, viewing its probed networks, and returning to the device list will display the option "WiFi STA" rather than "x Networks".
  * [ ] Because Device Lists are often nested, the approach used elsewhere isn't suitable
  * [ ] Add ```selected_device_index``` and ```selected_option_index[deviceCount]``` to DeviceListInstance, to allow selected devices and options to be maintained through nested device lists
* [ ] Create a new thread to parse Wendigo packets, using a Message Queue for concurrency management
  * [ ] Hand over responsibility from the UART Worker between buffer processing and calling ```parsePacket()```
  * [ ] When a complete packet is found we already move it into its own byte array
  * [ ] Package the packet, along with its length, into a struct and add the struct to a message queue
  * [ ] A new worker, running on a new thread, will wake up when an item is in the queue and this thread will parse the packet and make necessary changes to the data model.
  * [ ] This reduces the time the UART receiver is doing things other than receiving UART.
* [ ] Channel command overwrites enabled channels when setting channels
  * [ ] This approach is preferable when the client is always an application, but inconvenient in interactive mode
  * [ ] Make it possible to select/deselect a subset of channels at a time
  * [ ] Don't change the current implementation (too much) because it's a simple integration for Flipper-Wendigo
* [X] Scan menu option doesn't need "Start" when it's started or "Stop" when it's stopped - Use a single menu option that changes its text, similar to Tag/Untag.
* [X] Combined Bluetooth packet for BT Classic and LE devices
* [X] Combined Bluetooth data model for Flipper
* [X] BT Classic and LE device transmission from ESP32 to Flipper
* [X] BT Classic and LE packet parsing and caching on Flipper Zero
* [X] Enable/Disable radios
* [X] Scan Start/Stop
* [X] Scanning status
  * [X] Supported radios
  * [X] Status of radios
  * [X] Devices found
* [X] ESP Version (popup)
* [X] Display devices
  * [X] Display device name instead of BDA when present
  * [X] Dynamically update device list on new/updated devices
  * [X] Show attributes in device list (options menu)
    * [X] RSSI
    * [X] Device type
    * [X] CoD
    * [X] Tag/Untag (status and command in one)
    * [X] Age since last seen - Implemented but nonsense results.
  * [X] Device tagging and displaying only tagged devices
* [ ] WiFi Features
  * [X] WiFi packet
  * [X] ESP32 WiFi scanning
  * [X] ESP32 WiFi packet transmission
  * [X] Flipper WiFi packet parsing
  * [X] Flipper WiFi data model
    * [X] Move data model definitions into a common header file, included by both ESP32 and Flipper apps
    * [X] Standardise data model as a struct with common attributes, and a union of specialised structs.
  * [X] Channel hopping
  * [X] Capture a STA's Preferred Network List
    * [X] Extract PNL SSID's from probe requests
    * [X] Maintain PNL in data model
    * [X] Transmit PNL in packet to Flipper-Wendigo
    * [X] Parse PNL out of packet in Flipper-Wendigo
    * [X] Reconstruct PNL in Flipper-Wendigo's data model
    * [X] Display PNL in device list
      * [X] New STA option "%d SSIDs"
      * [X] New var_item_list-based scene wendigo_scene_network_list
      * [X] Displays new scene
    * [X] Display preferred networks from all devices in a single view
      * [X] Display the number of devices probing for each SSID
      * [X] Display all devices that have probed for a particular SSID
  * [ ] Support for 5GHz channels (ESP32-C5)
  * [X] Display different options in wendigo_scene_device_list
    * [X] For STA
      * [X] RSSI
      * [X] Tag/Untag
      * [X] ScanType
      * [X] AP (SSID if present, otherwise MAC)
      * [X] Channel
      * [X] LastSeen
    * [X] For AP
      * [X] RSSI
      * [X] Tag/Untag
      * [X] ScanType
        * [X] AuthMode
      * [X] Channel
      * [X] LastSeen
* [ ] Bluetooth Services
  * [ ] ESP32 service discovery
  * [ ] ESP32 service transmission
  * [ ] Flipper service parsing
  * [ ] Flipper service data model
* [X] Settings
  * [X] Retrieve and change MACs
  * [X] Enable/Disable channels
* [ ] Display device details
  * [ ] Update details when device updated
  * [ ] Use canvas view so properties can be layed out to (hopefully) make everything visible without scrolling
* [ ] "Display Settings" menu
  * [X] Enable/Disable device types
  * [ ] Sort results by
    * [ ] Name
    * [ ] LastSeen
    * [ ] RSSI
    * [ ] Device Type
* [ ] Other Features
  * [ ] Focus Mode
  * [ ] Use FZ LED to indicate events
  * [ ] variable_item_list (Flipper API) uses uint8_t to get/set selected item index, meaning the device list is limited to 255 devices.
    * [ ] Add some fanciness to allow display of a larger number of devices.
  * [ ] Ability to flash ESP32 firmware from Flipper?
* [ ] Flipper Zero logging
  * [ ] Develop a Flipper-based logging mechanism and support for multiple streams of data from ESP32
  * [ ] Logs which go directly to a buffer or file;
  * [ ] Device info which goes directly to display
  * [ ] Refactor all logging, error handling and status functionality
    * [ ] In FZ mode dispatch to FZ logging mechanism
* [ ] Improve FZ memory management if needed
  * [ ] Continuous scanning no longer seems to exhaust memory, but further testing is needed.
  * [ ] Hopefully find a FreeRTOS hook or config so I can provide a function when low on memory
  * [ ] Instead, prune device cache when low on memory
    * [ ] Configurable techniques as with Gravity - prune based on RSSI, time since last seen, or tagged status
    * [ ] Allow different techniques for different radios
  * [ ] If FreeRTOS or FZ hook can't be found, estimate an upper bound for device cache through trial & error

See the [open issues](https://github.com/chris-bc/wendigo/issues) for a full list of proposed features (and known issues).
