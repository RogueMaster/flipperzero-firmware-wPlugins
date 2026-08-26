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


## 🔌 USB SAM Reader (ACR39U-style passthrough)

Seader can present the attached HID SAM to a host PC as a **USB CCID contact
smart-card reader**, so any PC/SC application can drive the SAM directly (e.g.
the PM3 SAM host tools). The SAM appears as the card in slot 0.

Menu: with a SAM detected, choose **USB SAM Reader**. Connect the Flipper to a
PC over USB; a PC/SC reader appears (default identity: ACS / "ACR39U ICC
Reader"). Press **Back** to stop and restore the normal USB console.

- Host APDUs (`PC_TO_RDR_XfrBlock`) are relayed to the SAM over Seader's
  existing T=1 / CCID path; the response is returned to the host verbatim.
- The reader name is **user-editable in the app**: SAM detected → **USB Reader
  Name** → set the **manufacturer**, then the **product** string. Both persist
  (`usb_reader.conf`) and the USB PID auto-bumps on any change so Windows
  re-enumerates a fresh device node instead of showing the cached old name.
  Windows builds the PC/SC reader name as `<manufacturer> <product> <slot>`,
  which is what name-matching host tools key on. Keep at least the manufacturer
  non-empty: with no manufacturer string (`iManufacturer = 0`) Windows names the
  reader from the generic driver description and PC/SC apps stop finding it.
  Compile-time defaults / VID live in `sam_reader.h` (`SEADER_READER_DEFAULT_*`).
  Neutral VID `0x1209` keeps the inbox CCID driver bound (avoid a real vendor's
  VID like ACS `0x072F`, which loads the vendor driver and refuses a non-genuine
  device).
- Implementation: `usb_ccid_reader.{c,h}` (self-contained USB CCID gadget) +
  `sam_reader.{c,h}` (SAM relay bridge) + `scenes/seader_scene_reader.c`, worker
  state `SeaderWorkerStateReaderEmulation`.

**Why a custom USB gadget:** the firmware's stock `usb_ccid` interface
advertises only 2 endpoints (no interrupt IN) and `dwProtocols = T=0` only.
Windows' WUDF usbccid driver refuses to start it ("This device cannot start.
Code 10"), and it can't present a T=1 card. `usb_ccid_reader.c` therefore
defines its own `FuriHalUsbInterface` with a 3-endpoint descriptor (bulk
in/out + interrupt in) and `dwProtocols = T=0|T=1`, then bridges host APDUs to
the SAM. Built against the f7 SDK's `libusb_stm32` (`usb_ccid.h`, `usb_std.h`,
`usbd_core.h`) + `furi_hal_usb.h`.
