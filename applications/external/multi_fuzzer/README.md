## Multi Fuzzer
### Flipper Zero app for checking ibutton and RFID<sub>(125khz)</sub> readers .

**Not meant for any illegal activity/purposes.**

You can check your readers for vulnerabilities using the default UIDs and frequently used UIDs.

The application will also help to identify the "denial of service" vulnerability when providing an incorrect uid or when accessing the reader too often.

This is a completely remade app, visual style inspired by [iButton fuzzer](https://github.com/DarkFlippers/unleashed-firmware/tree/58338ff51f6f9857f39ef07d5eb4495cdc02290d/applications/external/ibtn_fuzzer), the compatibility of user dictionaries is also preserved

## Supported protocols

### **iButton**
- DS1990 (Dallas)
- Metakom
- Cyfral

### **LFRFID**
- EM4100
- HIDProx
- PAC/Stanley
- H10301
- IoProxXSF
- Paradox
- Indala26
- Viking
- Pyramid
- Keri
- Jablotron
- Electra
- Idteck
- Gallagher
- Nexwatch

## Application Features
### Main screen
- **Header** - selected protocol
- **Menu** - available attacks
    - **Default Values** - Using the dictionary from the app
    - **Load UIDs from file** - Loading a custom dictionary from an SD card
    - **Load file** - Loading UID from ***FlipperFormat*** key file with the ability to edit and further iterate over the selected byte
    - **BFCustomer ID** - Iterates over the selected byte with the remaining bytes equal to zero

### Attack screen
- **Header** - selected attack
- **2nd line**
    - **Time delay (TD)** - idle time between UID submissions
    - **Emulation time (EmT)** - transmission time of one UID
    - Both are adjustable in `0.01s` steps from `0.00` to `8.00s` (shown as `X.XX`). Short press = `±0.01s`, long press = `±0.10s`. A value of `0.00` uses the smallest possible non-zero delay.
- **3rd line** - Prtocol name
- **4th line** - Current UID

> **Note on timing:** RFID emulation drives the `furi_hal_rfid` hardware directly rather than going through the firmware's `lfrfid_worker`, which has a ~`0.1s` restart floor (and crashes below it on recent firmware). This lets you push TD/EmT to the real hardware limits on stock firmware. Very low values are intended for vulnerability discovery - a reader may stop reading once the field is on too briefly.
