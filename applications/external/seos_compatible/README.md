# 📖 Flipper Seos

Flipper app for reading and emulating Seos®-compatible cards/fobs/mobile credentials.

🎬 [Demo Video](demo.mp4)

## 🔑 Keys

**The app uses all zero keys by default**. If you'd like to use your own keys/ADF OID, use the format of the  `keys-example.txt` to specify them, and place into `SD Card/apps_data/seos/`.

- **No key files**: If no key files are present, the app defaults to all zeros (00) for keys and an ADF OID of 030107090000000000 ("0.3.1.7.9.0.0.0.0.0").  
- **keys.txt**: If `keys.txt` is present in `SD Card/apps_data/seos/`, it is automatically loaded at launch
- **Additional key sets**: Other key files named `*_keys.txt` (e.g., `work_keys.txt`, `home_keys.txt`) can be placed in the same directory and selected via the Key Switcher in the app menu

## ⚠️ Note

This software incorporates a third-party implementation of Seos® technology. It is **not** developed, authorized, licensed, or endorsed by HID Global®, ASSA ABLOY®, or any of their affiliates. References to Seos® are solely for descriptive and compatibility purposes.

No guarantee of compatibility or functionality is made. This implementation may not work with all Seos®-enabled systems, and its performance, security, and reliability are not assured. Users assume all risks associated with its use.

Seos®, HID Global®, and ASSA ABLOY® are trademarks or registered trademarks of their respective owners. This software is not associated with or sponsored by them in any way.

## 🛠️ To do:

- [ ] Fix iso14443a-4 framing
- [ ] ASN.1 for serializing/deserializing
- [ ] Support for larger message wrapping/unwrapping
- [ ] When parsing incoming data, use buffer + len instead of BitBuffer so I can increment buffer pointer as I parse header(s)
- [ ] CMAC checking where I missed it

## 💡 Hardware for BLE support (experimental)

1. Install/setup Nordic SDK
1. Install Toolchain manager
1. Launch Toolchain manager
1. Next to SDK version click down arrow and "open terminal"
1. Navigate to `samples/bluetooth/hci_uart_3wire`
1. Build with `west build -b nrf52840dk_nrf52840 -p auto`
1. `west flash`

## 🧪 nRF52840

1. Edit `boards/nrf52840dk_nrf52840.overlay` and change current speed to 460800 to match Flipper app.
1. `west build -b nrf52840dk_nrf52840 -p auto`
1. `west flash`

## 🦝 nRF52840 dongle

1. Copy `boards/nrf52840dongle_nrf52840.overlay` to `hci_uart_3wire`
1. May need to: `nrfutil install nrf5sdk-tools`
1. `west build -b nrf52840dongle_nrf52840 -p auto`
1. `nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req=0x00  --application ./build/hci_uart_3wire/zephyr/zephyr.hex --application-version 1 app.zip`
1. Put dongle into DFU by pressing 'reset' button
1. `nrfutil nrf5sdk-tools dfu usb-serial -pkg app.zip -p /dev/cu.usbmodemD39BF26162261`

## 🔌 Connection

| flipper purpose | pin | color  | nRF52840 dk pin | nRF52840 dongle pin |
|-----------------|-----|--------|-----------------|---------------------|
| rx              | 16  | yellow | P0.06           | P0.20               |
| tx              | 15  | orange | P0.08           | P0.24               |
| gnd             | 11  | black  | any ground      | GND                 |
| power           | 5v  | red    | VIN 3-5v        | VBUS                |


