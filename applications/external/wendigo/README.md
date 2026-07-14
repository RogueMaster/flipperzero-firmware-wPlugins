<a id="readme-top"></a>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![LinkedIn][linkedin-shield]][linkedin-url]

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/chris-bc/wendigo">
    <img src="The_Wendigo.jpg" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Wendigo</h3>

  <p align="center">
    Wendigo is a creature from North American mythology that stalks and overpowers its victims. If The Wendigo has you in its sights there's no escaping. You won't hear it coming and what happens next is up to The Wendigo.
    <br />
    <br />
    <a href="https://github.com/chris-bc/wendigo/issues/new?labels=bug&template=bug-report---.md">Report Bug</a>
    &middot;
    <a href="https://github.com/chris-bc/wendigo/issues/new?labels=enhancement&template=feature-request---.md">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
    </li>
    <li><a href="#whats-new">What's New</a>
      <ul>
        <li><a href="#whats-new-040">v0.4.0</a></li>
        <li><a href="#whats-new-030">v0.3.0</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <ul>
          <li><a href="#flipper-prerequisites">Flipper Zero</a></li>
          <li><a href="#esp32-prerequisites">ESP32</a></li>
          <li><a href="#wendigo-prerequisites">Wendigo</a></li>
        </ul>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<a id="about-the-project"></a>

## About The Project

Wendigo is an application for the Flipper Zero and ESP32 microcontrollers. It is primarily a WiFi and Bluetooth scanner/sniffer, with a few extra tricks up its sleeve. Wendigo is in its early stages of development, with features and bugfixes regularly committed, and releases to mark significant milestones.

Why write Wendigo when there are already great apps like [Marauder](https://github.com/justcallmekoko/ESP32Marauder) and [GhostESP](https://github.com/jaylikesbunda/Ghost_ESP) that do this and more? Flipper Zero has a large and fairly complete SDK (Software Development Kit) that makes it easy to create interactive interfaces; all of the Flipper + microcontroller apps I've seen display information in a serial console and require you to select devices and enter commands in a console, using the fairly clunky Flipper Zero keyboard. Wendigo has a graphical interface - instead of scrolling through pages of text to determine you want to select Access Point #13, and then typing ```13``` with the keyboard, in Wendigo you use the navigation buttons to navigate to the Access Point you're interested in, then scroll through the available commands and select the one you want.

My hope is that Wendigo will be a quick and easy way to get information about a wireless device; make it easier to troubleshoot and diagnose issues; and provide an interesting and accessible platform to explore the wireless spectrum and understand the data that it exposes. I think of Marauder as the tool you use to carry out an attack, and Wendigo as the tool to help you find the right target.

At a high level, these are Wendigo's features - Both implemented and planned:

* [X] Bluetooth Classic discovery
* [X] Bluetooth Low Energy discovery
* [X] WiFi Access Point discovery (2.4GHz)
* [X] WiFi Station (client device) discovery (2.4GHz)
* [X] WiFi channel hopping (2.4GHz)
* [X] Select which WiFi channels and which radios are used for scanning
* [X] Console Mode to use ESP32-Wendigo with other tools (e.g. laptop)
* [X] Tagging (selecting) devices of interest to view only them
* [X] Display discovered devices in a native Flipper Zero menu
  * [X] Device type
  * [X] Bluetooth device name (if present) or Bluetooth Device Address (BDA - looks like a MAC)
  * [X] Access Point SSID (if present) or MAC
  * [X] RSSI
  * [X] Bluetooth Class of Device (CoD)
  * [X] WiFi channel
  * [X] Access Point authentication mode (WEP, WPA2, etc.)
  * [X] SSID that a WiFi Station (client) is connected to
  * [X] Time since device was last seen
  * [X] Tag/Untag to select devices of interest
  * [X] List dynamically updates as new devices, or additional information about existing devices, are found
* [X] Select which device types are displayed (BT Classic, BLE, WiFi Access Points, WiFi Stations, All Bluetooth, All WiFi, All Devices)
* [ ] Display detailed device information
* [ ] Bluetooth service discovery
* [ ] Bluetooth attribute read/write
* [ ] Browse Bluetooth devices and their services & attributes
* [X] Browse WiFi networks, navigating from an Access Point to its connected Stations or from a Station to its Access Point
* [ ] Deauthenticate a device or all devices on a network
* [X] Collect & display a Station's (WiFi client - such as a phone) saved network list
* Put this list into Wigle to get a good idea where the device's owner lives, works and plays
* [X] Display all networks that probes were received for and support browsing from probed SSIDs to the Stations that probed for them.
* [ ] 5GHz WiFi channels ([requires ESP32-C5](https://www.aliexpress.com/item/1005009128201189.html))
* [X] Change Bluetooth BDA and WiFi MAC
* [ ] Use Flipper Zero's LED to indicate events

### Roadmap

For a more detailed overview of planned and implemented changes [refer to the Wendigo roadmap](https://github.com/chris-bc/wendigo/blob/main/docs/Wendigo-Roadmap.md).

More might come after this - Or it might not. My previous Flipper app, [Gravity](https://github.com/chris-bc/flipper-gravity), tried to include everything including the kitchen sink, and as a result it did everything poorly. My primary goal with Wendigo is a quick and easy way to do reconnaissance and extract as much information as possible from a device(s), displaying everything using native Flipper UI components.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="whats-new"></a>

## What's New

<a id ="whats-new-050"></a>

### v0.5.1

Released for Flipper-Wendigo only - Latest ESP32-Wendigo is still v0.5.0.

Fixes stability issues with v0.5.0. Further improvements coming soon!

### v0.5.0

* Because crowded areas get poor results running BLE and WiFi simultaneously, restructured menu to simplify running one or the other.
* Partial implementation of additional 802.11 packets
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

<a id="whats-new-040"></a>

### Previous Releases

A summary of all releases can be found [in the Wendigo Change Log](https://github.com/chris-bc/wendigo/blob/main/docs/CHANGELOG.md).

<a id="getting-started"></a>

## Getting Started

Wendigo is still under active development. As you can see from the above list there is still a lot to do before a version 1.0 release. Currently it can scan for and display Bluetooth (Classic and Low Energy) and WiFi (2.4GHz) devices; collate and display devices' saved networks; and not much else. Because it's under active development you might find a bug. If the Main branch is ever unuseable you'll have better luck with a release, which you can download from the [Releases](https://github.com/chris-bc/wendigo/releases) page or check out using the release tags.

Binaries for Flipper Zero and ESP32 are available from the [Releases](https://github.com/chris-bc/wendigo/releases) page. Significant releases will provide binaries for a variety of ESP32 models but - for now at least - minor releases include just standard ESP32 (Wroom) binaries. If you use a different ESP32 model, or want the latest features or to contribute to development, you can compile the application yourself as described below. These instructions assume you're using a Unix-like OS - MacOS, Linux or BSD. Windows users can either install WSL or adapt the instructions to suit windows.

<a id="prerequisites"></a>

### Prerequisites

<a id="flipper-prerequisites"></a>

To use Wendigo you'll need a Flipper Zero and an ESP32 microcontroller. Any model of ESP32 should work fine - Wendigo probes for available features and will disable Bluetooth and/or 5GHz WiFi if they're not supported by the chip. I'd recommend against using an ESP32-S2 because it lacks Bluetooth support. ESP32-C5 is currently the only model with 5GHz WiFi support, which will be added to Wendigo within the next few months. You can pick one up for under AUD$25 from [Espressif's official Ali Express store](https://www.aliexpress.com/item/1005009128201189.html).

#### Flipper Zero

Download source code for your preferred Flipper Zero firmware. The most popular unofficial distributions are [Momentum](https://github.com/next-flip/Momentum-Firmware), [RogueMaster](https://github.com/RogueMaster/flipperzero-firmware-wPlugins) and [Unleashed](https://github.com/DarkFlippers/unleashed-firmware), although a variety of other distributions are also available.

* Momentum Firmware:

  ```sh
  git clone https://github.com/Next-Flip/Momentum-Firmware.git
  ```

* RogueMaster Firmware:

  ```sh
  git clone https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git
  ```

* Unleashed Firmware:

  ```sh
  git clone https://github.com/DarkFlippers/unleashed-firmware.git
  ```

* Official Firmware:

  ```sh
  git clone https://github.com/flipperdevices/flipperzero-firmware.git
  ```

<a id="esp32-prerequisites"></a>

#### ESP32

Download ESP-IDF, the Espressif IoT Development Framework. If you use VSCode you can download the extension *ESP-IDF by Espressif Systems* from the VSCode Extension Marketplace and follow the setup wizard. This guide focuses on building and flashing using the command line, but the setup wizard for the VSCode ESP-IDF extension makes it a breeze to get up and running.

  ```sh
  git clone https://github.com/espressif/esp-idf.git
  ```

<a id="wendigo-prerequisites"></a>

#### Wendigo

  ```sh
  git clone https://github.com/chris-bc/wendigo.git
  ```

  To check out a specific tagged version use ```git checkout <tag name>```, for example:

  ```sh
  git checkout tags/v0.1.0
  ```

<a id="installation"></a>

### Building & Flashing

These instructions are written under the assumption that the above repositories have been downloaded to the following locations. Substitute those locations for your own in the instructions that follow.

* Flipper Zero firmware: ~/flipperzero
* ESP-IDF: ~/esp-idf
* Wendigo: ~/wendigo

1. Configure and Install ESP-IDF

   ```sh
   cd ~/esp-idf
   ./install.sh
   ```

2. Configure your current terminal session for ESP-IDF. This must be done before running ```idf.py``` in your current terminal session (note that if you have multiple terminal windows open you must run this in *every* terminal window you would like to run ```idf.py``` in).

   ```sh
   source ./export.sh
   ```

3. Connect an ESP32 and verify it has been detected by the OS.

   ```sh
   ls /dev/tty*
   ```

   It should appear (on Linux) as ```ttyUSB0``` or ```ttyACM0```.

4. Compile and flash ESP32-Wendigo

   ```sh
   cd ~/wendigo/esp32
   idf.py set-target <target chipset>
   (optional) idf.py menuconfig
   idf.py build flash
   ```

   ```<target chipset>``` is the model of ESP32 you are using. For example ```esp32```, ```esp32-s3```, etc.

   You only need to run ```menuconfig``` if there's a configuration setting you'd like to change. The repository default is configured for a standard ESP32 Wroom.

5. Install the Flipper Zero toolchain and compile firmware

   ```sh
   cd ~/flipperzero
   ./fbt
   ```

6. Configure the Flipper Build Tool (FBT) for your current terminal session. As with step (2), this needs to be done in each terminal session you wish to use FBT.

   ```sh
   source `./fbt -s env`
   ```

7. Remove the ESP32, connect a Flipper Zero, and again verify that it has been detected by the OS

   ```sh
   ls /dev/tty*
   ```

8. Link Flipper-Wendigo into the Flipper Zero firmware, build and flash the firmware

   ```sh
   ln -s ~/wendigo/flipper ~/flipperzero/applications_user/wendigo
   ./fbt firmware_all flash_usb_full
   ```

9. Alternatively, to flash just Wendigo and not the entire firmware:

   ```sh
   ./fbt launch APPSRC=wendigo
   ```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="usage"></a>

## Usage

Connect the ESP32 to the Flipper Zero by connecting the following pins:

* Flipper Zero TX (13) to ESP32 RX
* Flipper Zero RX (14) to ESP32 TX
* Flipper Zero GND (8 or 18) to ESP32 GND
* Flipper Zero 3V3 (9) to ESP32 3V3 (sometimes labelled VCC)

Launch the Wendigo application from Apps => GPIO => ESP => [ESP32] Wendigo BT+BLE+WiFi Monitor. The ```Setup``` menu allows you to select which radios are enabled (Bluetooth Classic, Bluetooth Low Energy, WiFi) and which WiFi channels are included in channel hopping, otherwise you can just start scanning and check out the results.

Alternatively you can connect ESP32-Wendigo to any device with a serial console and control it from there. Connect your serial console in the usual way, for example ```screen /dev/ttyUSB0 115200```, then enter the command ```i 1``` (or ```interactive 1```) to switch from Binary Mode to Interactive Mode. You now have tab completion, command history, and can use the commands ```commands``` to get a list of available commands and ```help``` to get a brief overview and syntax of each command.

*For more information about Interactive Mode please refer to the [Command Documentation](https://github.com/chris-bc/wendigo/blob/main/docs/ESP32-Wendigo-Commands.md)*

### TODO: Screenshots

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="contributing"></a>

## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Top contributors

<a href="https://github.com/chris-bc/wendigo/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=chris-bc/wendigo" alt="contrib.rocks image" />
</a>

<a id="license"></a>

## License

Distributed under the MIT Licence. See `LICENSE` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<a id="contact"></a>

## Contact

Chris BC - [@chris_bc](https://twitter.com/chris_bc) - chris@bennettscash.id.au

Project Link: [https://github.com/chris-bc/wendigo](https://github.com/chris-bc/wendigo)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/chris-bc/wendigo.svg?style=for-the-badge
[contributors-url]: https://github.com/chris-bc/wendigo/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/chris-bc/wendigo.svg?style=for-the-badge
[forks-url]: https://github.com/chris-bc/wendigo/network/members
[stars-shield]: https://img.shields.io/github/stars/chris-bc/wendigo.svg?style=for-the-badge
[stars-url]: https://github.com/chris-bc/wendigo/stargazers
[issues-shield]: https://img.shields.io/github/issues/chris-bc/wendigo.svg?style=for-the-badge
[issues-url]: https://github.com/chris-bc/wendigo/issues
[license-shield]: https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge
[license-url]: https://opensource.org/licenses/MIT
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/chrisbennettscash
[product-screenshot]: images/screenshot.png
[Next.js]: https://img.shields.io/badge/next.js-000000?style=for-the-badge&logo=nextdotjs&logoColor=white
[Next-url]: https://nextjs.org/
[React.js]: https://img.shields.io/badge/React-20232A?style=for-the-badge&logo=react&logoColor=61DAFB
[React-url]: https://reactjs.org/
[Vue.js]: https://img.shields.io/badge/Vue.js-35495E?style=for-the-badge&logo=vuedotjs&logoColor=4FC08D
[Vue-url]: https://vuejs.org/
[Angular.io]: https://img.shields.io/badge/Angular-DD0031?style=for-the-badge&logo=angular&logoColor=white
[Angular-url]: https://angular.io/
[Svelte.dev]: https://img.shields.io/badge/Svelte-4A4A55?style=for-the-badge&logo=svelte&logoColor=FF3E00
[Svelte-url]: https://svelte.dev/
[Laravel.com]: https://img.shields.io/badge/Laravel-FF2D20?style=for-the-badge&logo=laravel&logoColor=white
[Laravel-url]: https://laravel.com
[Bootstrap.com]: https://img.shields.io/badge/Bootstrap-563D7C?style=for-the-badge&logo=bootstrap&logoColor=white
[Bootstrap-url]: https://getbootstrap.com
[JQuery.com]: https://img.shields.io/badge/jQuery-0769AD?style=for-the-badge&logo=jquery&logoColor=white
[JQuery-url]: https://jquery.com 
