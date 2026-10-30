# Flipper Zero: QRCode Generator
Generates and displays QRCodes on the flipper zero.

| ![](screenshots/Screenshot-menu.png) | ![](screenshots/Screenshot-entered-text.png) | ![](screenshots/Screenshot-saved-qrcodes.png)  |
| ------------- | ------------- | ------------- |
| ![](screenshots/Screenshot-open-saved-qrcode.png)  | ![](screenshots/Screenshot-readme.png)  | &nbsp;  |

## Download
Grab the `qrcode_generator.fap` from the latest release. 

## Installation 
After the download copy the `.fap` file onto your flipper in the folder 
`apps/Tools`. Then create in the `apps_data` folder the subfolder 
`qrcode_generator` to open existing QRCodes or to save it on the sdcard.

## Firmware Compatibility

This application supports building against the official Flipper Zero
firmware SDK and Momentum Firmware.

### Official Firmware

Build normally using the official SDK:

```sh
ufbt
```

### Momentum Firmware

When building against the Momentum SDK, enable extended keyboard symbols with:

```sh
ufbt --extra-define=QRCODE_MOMENTUM_BUILD
```
This enables additional characters such as `:`, `/`, `\`, and `?`, allowing URLs to be entered directly into the QR code generator.

The `QRCODE_MOMENTUM_BUILD` flag must be explicitly enabled when building against the Momentum SDK. Without it, the original keyboard behavior is preserved.

This feature relies on a Momentum-specific API that is not available in the official Flipper firmware SDK.

## Features
- Generating QRCodes based on the entered input
- Open QRCodes which are saved on the sdcard

## Future Features
- [ ] Save newly generated QRCodes on the sdcard
- [ ] Export generated QRCodes as bitmap (.pbm image)

## Data format
To generate newly QRCodes, you can just enter the data you want to stored 
in the QRCode.

To open saved QRCodes you need on a regular basis, just create a `.txt` file 
in the `apps_data/qrcode_generator/` folder on the sdcard with just the 
data in it. The app reads the content of the file and creates an QRCode with 
it. You need no further specific format or something else. 

**Examples:**
```text
# Open new mail:
mailto:example@example.com

# Open link:
https://github.com/qw3rtty

# Connect to a WIFI:
WIFI:S:<ssid>;P:<password>;T:<encryption>;
```

## Using the App
The app is acutally straightforward. If you start it, you have three menu
options. One to generate a new QRCode, one to open existing ones from the 
sdcard and one to get the basic information about this app.

## qrcode library
The application uses the awesome C library from [nayuki](https://github.com/nayuki/QR-Code-generator)
to generate and display the QRCodes. 

