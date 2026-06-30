# MagSpoof for the Flipper Zero
MagSpoof-ing, as [coined / invented by Samy Kamkar](https://github.com/samyk/magspoof), is the use of an electromagnet to emulate the data on a traditional magstripe card —  wirelessly! 

This app ports the original MagSpoof project to the Flipper Zero, adding file management and emulation controls, along with [some experimental features](#skunkworks-and-experimental-features). Visit the [Wiki](https://github.com/zacharyweiss/magspoof_flipper/wiki) for information on getting started.

> [!TIP]
> After the first launch of the app, the `/apps_data/magspoof/` folder will be created on your Flipper's SD card, and populated with sample `.mag` files. Place your own files here, using the [sample files](resources) as a template.

> [!WARNING]
> Use responsibly, and at your own risk. MagSpoof is intended solely for authorized security audits and educational purposes. Ensure legal compliance and obtain necessary permissions before using this tool.
>
> ***I neither condone nor am sympathetic to malicious uses of my code.***
 

## TX Module
The GPIO module is a simple electromagnet consisting of an H-bridge, a capacitor, and a coil. One can build the pictured circuit, or purchase a MagSpoof board from the community. There is no "official board", but the following are popular choices:
- [Rabbit-Labs'](https://www.rabbit-labs.com/) [Multi Pass MagSpoof](https://rabbit-labs.com/product/rabbit-labs-multi-pass-magspoof-flipper-board/)
- [astro's](https://cyberpa.ws/) [flipspoof](https://www.tindie.com/products/astrowave/flipper-zero-magspoof-module/)
- [Electronic Cats'](https://electroniccats.com) [Magspoof](https://electroniccats.com/store/flipper-add-on-magspoof/) and [Marauder_Spoof](https://electroniccats.com/store/flipper-add-on-marauder_spoof/)

> [!IMPORTANT]
> The linked boards ***do not read magstripes***, they solely ***emulate***.

> [!TIP]
> Positioning can be finicky, especially on readers with significant EMF shielding. Situating the corner of one's coil over the read-head tends to give the most consistent results; try a few different angles / depths / positions until you find the sweet spot for your module.

<img src="https://user-images.githubusercontent.com/20050953/215654078-1f4b370e-21b3-4324-b63c-3bbbc643120e.png" alt="Wiring diagram" title="Wiring diagram" style="height:320px">

A7, A6, and A4 are the default pins used by the app, but can be changed in the app's settings for custom boards.

## Skunkworks and Experimental Features
Additional efforts have been made towards internal TX over the RFID / NFC / RF / Piezo systems. While there has been some success with internal TX being picked up/detected by more compact mag readers such as Square, it typically cannot produce a strong enough field for any reader I've tested to decode/parse all the transmitted data properly; as such, all alternate internal TX options are hidden behind debug mode. Old testing, notes, ideas, and planning can be found in [`NOTES.md`](NOTES.md).

There exists a `Read` scene in the app courtesy of hummusec. This is untested, and exclusively for use with UART magstripe readers; hence, as with the experimental internal TX, it is locked behind debug mode and intended only for developers. There do not exist any purchase-able / manufactured boards that include a reader. Independently, I am working on creating a general USB host for the Flipper using a MAX3421E; if and when this is completed one will be able to use a generic USB magstripe reader with the app, however, there is still much to do before this is ready.

> [!CAUTION]
> If using experimental internal TX — while in my testing I've seen no reason to believe this could damage the RFID (or other) hardware, this is inherently using those systems in ways they were neither designed nor intended for; I take no responsibility for fried/bricked Flippers.

## Credits
This project interpolates work from [Samy Kamkar](https://github.com/samyk/)'s [original MagSpoof project](https://github.com/samyk/magspoof), [Alexey D. (dunaevai135)](https://github.com/dunaevai135/) & [Alexandr Yaroshevich](https://github.com/AYaro)'s [Flipper hackathon project](https://github.com/dunaevai135/flipperzero-firmware/tree/dev/applications/magspoof), and the [Flipper team](https://github.com/flipperdevices)'s [LF RFID](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/main/lfrfid) and [SubGhz](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/main/subghz) apps.  

Many thanks to everyone who has helped in addition to those above, most notably: 
- [arha](https://github.com/arha) for bitmapping work, skunkworks testing, and innumerable suggestions/ideas/feedback (now a collaborator!)
- [Zalán Kórósi (Z4urce)](https://github.com/Z4urce) for an earlier app icon
- [Salvatore Sanfilippo (antirez)](https://github.com/antirez) for bitmapping suggestions and general C wisdom
- [skotopes](https://github.com/skotopes) for RFID consultation
- [Tiernan (NVX)](https://github.com/nvx) + dlz for NFC consultation
- davethepirate for EE insight and acting as a sounding board
- [cool4uma](https://github.com/cool4uma) for their work on custom text_input scenes 
- [hummusec](https://github.com/hummusec) for testing of UART RX
- [xMasterX](https://github.com/xMasterX) and [WillyJL](https://github.com/Willy-JL) for keeping the app updated across API changes while I was away!
- Everyone else I've had the pleasure of chatting with!
