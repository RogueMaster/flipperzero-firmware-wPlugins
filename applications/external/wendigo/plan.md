# Initial Prototype

### Menu
* Setup
    * [X] Enable BLE
    * [X] Enable BD
    * [X] Enable WiFi
    * Channels
        * All
        * Selected
            * 1: On, Off
            * 2: On, Off
            * 3: etc.
    * MAC Address
* Scan
    * [X] Start
    * [X] Stop
    * Status
* Devices
    * List of discovered devices
        * [X] BDA
        * Name
        * RSSI
        * Protocol
        * Last seen
        * UUID / Services
        * Pair/Bond
        * Selected?
* Selected Devices
    * List of selected devices
        * As above
* Track selected devices
    * RSSI metrics
* [X] About

### Next steps
* Start scan -> disable setup
* Stop scan -> enable setup

#### THEN commence work on esp32-wendigo
* Perform sufficient initialisation of all radios to be able to get/set MACs
* Base on console application again
* Mock up a protocol to accept commands from FZ and output device info at random intervals

#### Push application logic to Flipper this time
* Activate UART interface on launch
* Start scan passes the configured settings to ESP32 and processes responses indefinitely
* Use Flipper LED to indicate progress - light blue new BLE dovice, dark blue new BT device, green new wifi device, yellow updated info for existing device, white idle (not scanning), purple scanning
* Data from ESP32 processed into an object model to represent devices
* Scan status summarises settings

#### Iterate
* Replace mocks with real device data
* Refine data model and develop device list and detail UIs based on available data
* Ability to select devices
* Tracking view collating RSSI for selected devices
