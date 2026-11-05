# NFC&LFRFID Field Detector

This application allows you to detect the presence of NFC and LF RFID fields. It can be used to check what technology is used in a reader that you don't have documentation for, or to check whether a reader is working properly.

## Usage

Open the app and bring your Flipper Zero up to the reader. The app will display the detected field type (NFC or LF RFID). For LF RFID, the app will also display the frequency of the field. If the reader is using both NFC and LF RFID, the app will display both.

Hold the Flipper on the reader to measure an LF field's exact frequency. The longer you hold it, the more precise the reading (about 1 ppm after 10 seconds).

## Calibration

Every Flipper's crystal oscillator runs a little fast or slow, causing an error in the LF field reading. There are two ways to correct it:

- **Calibrate with a computer**: Measure the Flipper's crystal against internet time for 30 minutes. Progress is displayed on the Flipper screen, and the result is saved in the app. Connect USB and open the page in the QR code. Alternatively, run `tools/flipper_crystal.py` from the app folder in the repository.

- **Set calibration manually**: Enter a calibration you already know, for example from measuring a field that has a known frequency.

The calibration is slightly sensitive to temperature, and shifts by about 1 ppm for every 4 °C. Calibrate at about the temperature you'll take readings at, for example indoors for indoor readers.
