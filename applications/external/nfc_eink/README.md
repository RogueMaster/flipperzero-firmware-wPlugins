# nfc_eink

- This app allows emulation and writing of NFC Eink tags from Waveshare and Goodisplay
- Image saved as one screen type can be then loaded to screen of another type

Here is the list of displays wich app supports now:

**Waveshare**:
- Waveshare 2.13 inch
- Waveshare 2.7 inch
- Waveshare 2.9 inch
- Waveshare 4.2 inch
- Waveshare 7.5 inch

**Goodisplay**:
- GDEY0154D67
- GDEY0213B74
- GDEY029T94
- GDEY037T03

# How to use 

## Emulation

**Goodisplay emulation**:
1. Install Goodisplay NFC-D1 application from [here ](https://www.good-display.com/product/410.html) (scan QR code)
2. Choose screen type and select any image from gallery, or use predefined one
3. On Flipper select Emulate->Goodisplay
4. Apply phone to Flippers back

**Waveshare emulation**:
1. Install Waveshare NFCTag application from [here ](https://www.waveshare.com/wiki/7.5inch_NFC-Powered_e-Paper)
2. Choose screen type and select any image from gallery
3. On Flipper select Emulate->Waveshare
4. Apply phone to Flippers back

**Waveshare emulation by Proxmark**:
1. On Flipper select Emulate->Waveshare
2. Put Flipper close to Proxmark
3. On proxmark use **hf waveshare** command to send image

## Writing

1. Choose saved image on Flipper 
2. Choose Write
3. Select screen type you want to write
4. Apply Eink tag of that type, or another Flipper with emulation.

Writing depends on 'Writing mode' on Settings menu:

- **Strict** - image can be written only to that type of screen which was used during emulation (initial screen).
- **Size** - image can be written to any screen with the same resolution (example Waveshare 2.13 inch to GDEY0213B74)
- **Vendor** - image can be written to any screen with the same vendor as in initial screen.
- **Free** - image can be written to any screen without restrictions.

When initial image is smaller then target screen, then it will be copied to target and other part in target will be filled with black color.
When initial image is larger then target screen, then it will be cropped to fit target screen size.

