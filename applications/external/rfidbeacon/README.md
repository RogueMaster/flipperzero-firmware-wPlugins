# flipperzero-letterbeacon
A letter/number/symbol [Morse](https://fr.wikipedia.org/wiki/Code_Morse_international) beacon on **RFID**/**NFC** interfaces of the **Flipper Zero**

![banner](https://raw.githubusercontent.com/nmrr/flipperzero-letterbeacon/main/img/banner-rfid.jpg)
(banner has been made with **Dall.E 2**)



Like Russia with their HF [letter beacons](https://en.wikipedia.org/wiki/Letter_beacon), it's possible to use the **Flipper Zero** as a letter/number/symbol [Morse](https://fr.wikipedia.org/wiki/Code_Morse_international) beacon. **Flipper Zero** has multiple RF interfaces (RFID, NFC and UHF) and it's possible to transmit raw data over these interfaces.

This application can transmit the whole alphabet (A to Z), numbers (0 to 9) and symbols in [Morse](https://fr.wikipedia.org/wiki/Code_Morse_international) via **RFID** (125 kHz) and **NFC** (13.56 MHz) interfaces.

On **RFID** (125 kHz) range is about 5 meters with a small loop antenna. With an amplified loop, the range can extend to 15 meters according to my measurements. 

On **NFC** (13.56 MHz) range is about 50 meters with a small loop antenna. 


## Gallery

Menu of the application:

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/screen1.png" width=25% height=25%> <img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/screen2.png" width=25% height=25%> <img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/screen3.png" width=25% height=25%>

The LED turns red when transmitting via RFID and blue when using NFC

**Airspy Discovery HF+** **SDR** bas been used during these tests with a small loop antenna. Following measures have been made at a distance of 2m from the **Flipper Zero**.

**A** letter:

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/a.png" width=50% height=50%>

**3** number:

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/3.png" width=50% height=50%>

**NFC** at 65m:

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/Screenshot_20250615-171102.png" width=50% height=50%>

**RFID** at 10m:

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/Screenshot_20250615-171754.png" width=50% height=50%>

## Build the program

Assuming the toolchain is already installed, copy **flipper_letterbeacon** directory to **applications_user**

Plug your **Flipper Zero** and build the RFID beacon:
```
./fbt launch_app APPSRC=applications_user/flipper_letterbeacon
```

The program will automatically be launched after compilation

<img src="https://github.com/nmrr/flipperzero-letterbeacon/blob/main/img/flipperzero.png" width=25% height=25%>

**Button assignments**: 

button  | function
------------- | -------------
**Left/Right** *[short press]* | Select the letter or the value of tick/pause in ms
**Left/Right** *[long press]* | Select the letter or the value of tick/pause in ms (more)
**Up/Down** *[short press]* | Navigate in the menu
**Ok** *[short press]*  | Choose between RFID/NFC
**Ok** *[long press]*  | Enable/disable the transmission
**Back** *[long press]*  | Exit

If you don't want to build this application, just simply copy **flipper_letterbeacon.fap** on your **Flipper Zero** 

Build has been made with official toolchain (1.3.4), **API Mismatch** error may appear if you are using custom firmware. You can bypass this error but the program may crash.

## What's next ?
* ~~Choose the speed morse code~~ **DONE !**
* ~~Transmit on NFC interface (13.56 MHz), range is wider than **RFID**~~ **DONE !**
* Transmit on UHF interface (433 MHz, etc)
* Transmit personalized text message over the air

## Changelog

* 2025-05-29
  * Add TX to the NFC interface (13.56 MHz)
  * Pause between transmissions is now accurate
  * '-' is now used to display a dash instead of '_'
  * Project has been renamed from flipperzero-rfidbeacon to flipperzero-letterbeacon

* 2025-05-25
  * Add customizable transmission speed
  * Add adjustable pause timing
    
* 2023-07-11
  * Bug fixing
  * Symbols have been added
  * Morse code view has been added

* 2023-07-09
  * Initial release
