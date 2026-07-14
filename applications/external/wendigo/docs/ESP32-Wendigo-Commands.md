## Wendigo Interactive Mode

<a id="about"></a>
### About

While Wendigo is first and foremost a Flipper Zero application, and a Flipper Zero is required to make effective use of many of its features, the Wendigo ESP32 firmware provides an *Interactive Mode* that can be used to execute all of the same commands that are used by the Wendigo Flipper Zero application.

When using ESP32-Wendigo in this way, please bear in mind that Flipper-Wendigo performs a large amount of processing on the data provided by ESP32-Wendigo. Using Interactive Mode allows you to get all the same data that Flipper-Wendigo has access to, but some information requires further processing that is not performed by ESP32-Wendigo.

<a id="getting-started"></a>
### Getting Started

This document assumes you have an ESP32 that has been flashed with Wendigo firmware.

<a id="reading-commands"></a>
#### Reading Commands

This document presents commands and their syntax using the standard notation for syntax; optional items are enclosed in square brackets (```[]```) and named elements are enclosed in angle brackets (```<>```). For example, the syntax for the ```wifi``` command is:
* ```w[ifi] [ 0 | 1 | 2 ]```

Or, more descriptively:
* ```w[ifi] [<RadioStatus>]```
* ```RadioStatus ::= 0 | 1 | 2```

This means:
* The command can be run by entering the command ```wifi``` or simply ```w```;
* The command takes one optional parameter
* If provided, the parameter can be the digit 0, 1, or 2.

The [Commands](ESP32-Wendigo-Commands.md#commands) section of this document describes all available commands.

<a id="connecting"></a>
#### Connecting to ESP32-Wendigo

* Connect the ESP32 to your computer, phone, or other device of choice with a USB cable.
* Use your favourite serial terminal (Screen, Minicom, ESP-IDF, etc.) to connect to the ESP32.
* Enter the command ```i[nteractive] 1``` to enable Interactive Mode. You should be presented with a message similar to:

  ```I (3783) WENDIGO: Enabling Interactive Mode...Done.```

At this point you can run any Wendigo command. Refer to the [Commands](ESP32-Wendigo-Commands.md#commands) section to explore your nearby radio spectra, but as a starting point try these:
* ```b[le] 1``` (Start BLE scanning)
* Wait for around a minute and then run ```b[le] 0``` (disable BLE scanning)
* ```s[tatus]``` (Display status information about the device and discovered devices)

<a id="finding-serial"></a>
#### Finding ESP32's serial port

In order to connect your serial console to ESP32-Wendigo you need to know which serial port your operating system has assigned to the device. How this is done varies across operating systems, this section describes techniques to identify the correct serial port on Linux, MacOS and Windows.

<a id="finding-serial-linux"></a>
##### Linux

* Start a terminal emulator
* Before connecting the ESP32 run ```ls /dev/tty*```
* Connect the ESP32 and run this command again. A new entry should be displayed that wasn't present before
  * It will probably be named ```/dev/ttyUSB0``` or ```/dev/ttyACM0```
  * This is the serial port to use when connecting the serial console.

<a id="finding-serial-macos"></a>
##### MacOS

* Run ```/Applications/Utilities/Terminal.app```
* Before connecting the ESP32 run ```ls /dev/cu.*```
* Connect the ESP32 and run this command again. A new entry should be displayed that wasn't present before
  * It will probably be named ```/dev/cu.usbserial-XXXX``` or ```/dev/cu.SLAB_USBtoUART```.
  * This is the serial port to use when connecting the serial console.

<a id="finding-serial-windows"></a>
##### Windows

* Connect the ESP32
* Open Device Manager: Press the Windows key, type "device manager" and press Enter.
* Expand the ```Ports (COM & LPT)``` section
* This should contain an entry similar to ```Silicon Labs CP210x USB to UART Bridge (COM5)```
* In this example, the serial port being used is ```COM5```.

<a id="using-serial"></a>
#### Using serial console software

### TODO

<a id="commands"></a>
### Commands

<a id="hci"></a>
#### Bluetooth Classic

Control the Bluetooth Classic scanner.

```sh
h[ci] [<radioStatus>]
radioStatus ::= <disable> | <enable> | <status>
disable ::= 0
enable ::= 1
status ::= 2
```
e.g. ```h 1``` to enable Bluetooth Classic scanning

<a id="ble"></a>
#### Bluetooth Low Energy

Control the Bluetooth Low Energy scanner.

```sh
b[le] [<radioStatus>]
radioStatus ::= <disable> | <enable> | <status>
disable ::= 0
enable ::= 1
status ::= 2
```
e.g. ```ble 2``` to display BLE status information

<a id="wifi"></a>
#### WiFi

Control the WiFi scanner.

```sh
w[ifi] [<radioStatus>]
radioStatus ::= <disable> | <enable> | <status>
disable ::= 0
enable ::= 1
status ::= 2
```
e.g. ```wifi 0``` to disable WiFi scanning

<a id="channel"></a>
#### WiFi Channels

Display or set the enabled WiFi channels. On startup Wendigo defaults to enabling all 2.4GHz channels, and WiFi scanning will stay on a channel for a short time (500 milliseconds by default) before moving to the next channel so it can obtain information from all channels you're interested in.

However, Wendigo cannot receive packets on one channel while it is tuned to another, and this means that Wendigo only sees one-twelfth of the traffic that occurs on any channel because Wendigo is dividing its time between 12 2.4GHz WiFi channels. Because of this, if you're doing serious work it's a good idea to restrict the channels included in channel hopping once you know which channel(s) the devices of interest are operating on.

```sh
c[hannel] <channelNum>*
```
The asterisk in this syntax indicates that ```<channelNum>``` is included zero or more times.

```<channelNum>``` must be a channel that is supported by your ESP32. Currently Wendigo only supports 2.4GHz WiFi, so

```sh
channelNum ::= 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12
```
The following are both valid channel commands:
* ```channel``` (displays a list of enabled channels)
* ```c 3 11 1 6 9``` (set the enabled channels to 1, 3, 6, 9 and 11).

<a id="mac"></a>
#### MAC Addresses

```sh
mac [ <interface> <MAC> ]
interface ::= IF_BASE | IF_WIFI | IF_BLUETOOTH
IF_BASE ::= 0
IF_WIFI ::= 1
IF_BLUETOOTH ::= 2
MAC ::= xx:xx:xx:xx:xx:xx
x ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | a | b | c | d | e | f
```

When ```mac``` is run without any arguments it displays both the WiFi and Bluetooth MAC addresses.

When provided arguments to specify the interface to change and the desired MAC address the ESP32 will have its specified MAC address changed to the specified value. This is a temporary change, the device's MAC addresses will revert to their original values when the device is reset.

The command syntax above includes a special interface specifier ```IF_BASE``` (```0```). Explaining this would be quite convoluted and [Espressif have a great document covering it](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#_CPPv414esp_mac_type_t) so I'll link to that instead.

Currently Wendigo does not display the base MAC, nor give any indication what sort of MAC address is being used. This means that it's currently possible to *set* a new base MAC for the device, but not possible to *view* the base MAC, or even view the new WiFi and Bluetooth MACs that were assigned as a result of changing the base MAC.

<a id="status"></a>
#### Status

```sh
s[tatus]
```

The ```status``` command provides a summary of the ESP32's supported features and the current status of scanning, for example:

```sh
*****************************************************
*                                                   *
*              Wendigo version   0.5.0              *
*                                                   *
*     Chris Bennetts-Cash   github.com/chris-bc     *
*                                                   *
*                                                   *
*    Compiled with Bluetooth Dictionary:     YES    *
*    Bluetooth Classic Supported:            YES    *
*    Bluetooth Low Energy Supported:         YES    *
*    WiFi Supported:                         YES    *
*    Bluetooth Classic Scanning:            IDLE    *
*    Bluetooth Low Energy Scanning:         IDLE    *
*    WiFi Scanning:                         IDLE    *
*    BT Classic Devices:                       0    *
*    BT Low Energy Devices:                   13    *
*    WiFi Access Points:                       3    *
*    WiFi Stations:                            5    *
*                                                   *
*****************************************************
```

<a id="version"></a>
#### Version

```sh
v[er]
```

**Note** that the full command here is ```ver```, not ```version```, because the ESP32 Console library, which is used by Wendigo, includes a built-in ```version``` command to display chip information.

<a id="tag"></a>
#### Tag

Devices can be *tagged*, or selected, to allow you to view only those devices and ignore everything else.

```sh
t[ag] <DeviceType> <MAC> <Action>

DeviceType ::= b[t] | w[ifi]
Action ::= <Untag> | <Tag> | <Status>
Untag ::= 0
Tag ::= 1
Status ::= 2
MAC ::= xx:xx:xx:xx:xx:xx
x ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | a | b | c | d | e | f
```

When the ```bt``` device type is selected Wendigo will search for either a Bluetooth Classic or BLE device with the specified MAC.

<a id="focus"></a>
#### Focus

Focus mode restricts the displayed devices to only devices that have been tagged.

```sh
f[ocus] <Action>

Action ::= <Disable> | <Enable> | <Status>
Disable ::= 0
Enable ::= 1
Status ::= 2
```

<a id="interactive"></a>
#### Interactive Mode

```sh
i[nteractive] <Action>

Action ::= <Disable> | <Enable> | <Status>
Disable ::= 0
Enable ::= 1
Status ::= 2
```
