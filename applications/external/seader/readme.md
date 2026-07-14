# 🔐 Seader

A [Flipper Zero](https://flipperzero.one/) application (aka "fap") 
that read credential from HID: iClass, iClass SE, Desfire EV1/EV2, and Seos using a HID SAM and UART adapter.  Latest release on the [App Catalog](https://lab.flipper.net/apps/seader).

## 🐬 Bugs

File issues in [GitHub](https://github.com/bettse/seader/issues).

## 🛠️ Hardware

### Option 1: NARD flipper add-on

Buy it assembled at [Red Team Tools](https://www.redteamtools.com/nard-sam-expansion-board-for-flipper-zero-with-hid-seos-iclass-sam/), with or without SAM.

Or build it yourself from the files in the [NARD repo](https://github.com/killergeek/nard).

Optionally 3d print a [case designed by Antiklesys](https://www.printables.com/model/576735-flipper-zero-samnard-protecting-cover).

### Option 2: SAMAdams

Buy it at [Red Team Tools](https://www.redteamtools.com/sam-adams-for-flipper-zero/)

### Option 3: Flippermeister

Buy it at [Red Team Tools](https://www.redteamtools.com/flippermeister/).

### Option 4: Smart Card 2 Click

Buy HID SAM:
 * [USA](https://www.cdw.com/product/hp-sim-for-hid-iclass-for-hip2-reader-security-sim/4854794)
 * [Canada](https://www.pc-canada.com/item/hp-sim-for-hid-iclass-se-and-hid-iclass-seos-for-hip2-reader/y7c07a)

Put SAM into **[adapter](https://a.co/d/1E9Zk1h)** (because of chip on top) and plug into **Smart Card 2 Click** ([Mikroe](https://www.mikroe.com/smart-card-2-click) [digikey](https://www.digikey.com/en/products/detail/mikroelektronika/MIKROE-5492/20840872) with cheaper US shipping). Connect Smart Card 2 Click to Flipper Zero (See `Connections` below).

Optionally 3d print a [case designed by sean](https://www.printables.com/model/543149-case-for-flipper-zero-devboard-smart2click-samsim)

#### Connections

| Smart Card 2 Click | Flipper     |
| ------------------ | ----------- |
| 5v                 | 1           |
| GND                | 8 / 11 / 18 |
| TX                 | 16          |
| RX                 | 15          |

## 🧩 Development

### To Build App

 * Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)
 * `git submodule update --init --recursive` to get dependencies
 * `ufbt` to build
 * `ufbt launch` to launch

### To Build ASN1 (if you change seader.asn1)

 * Install git version of [asnc1](https://github.com/vlm/asn1c) (`brew install asn1c --head` on macos)
 * Run `asn1c -D ./lib/asn1 -no-gen-example -no-gen-OER -no-gen-PER -pdu=all seader.asn1` in in root to generate asn1c files

## 🗃️ References

- [omnikey_5025_cl_software_developer_guide_mn_en](https://www.virtualsecurity.nl/amfile/file/download/file/18/product/1892/)
- [omnikey_5326_dfr_softwaredeveloperguide](https://www.hidglobal.com/sites/default/files/documentlibrary/omnikey_5326_dfr_softwaredeveloperguide.pdf)
- [omnikey_5027_software_developer_guide](https://www.hidglobal.com/sites/default/files/documentlibrary/omnikey_5027_software_developer_guide.pdf)
- [PLT-03362 A.0 - iCLASS Reader Writer Migration Application Note](http://web.archive.org/web/20230330180023/https://info.hidglobal.com/rs/289-TSC-352/images/PLT-03362%20A.0%20-%20iCLASS%20Reader%20Writer%20Migration%20Application%20Note.pdf)
- [HID SE reader消息模块的ANS.1 BER学习](https://blog.csdn.net/eyasys/article/details/8501200)

## 💾 Memory usage commands

- `arm-none-eabi-nm ~/.ufbt/build/seader.fap -CS --size-sort`
- `arm-none-eabi-readelf ~/.ufbt/build/seader.fap -t`
- `ufbt cli` -> `free_blocks`

