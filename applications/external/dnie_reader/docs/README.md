# IDentity

Read electronic identity documents with your Flipper Zero: the **Spanish eID (DNIe)**, **ePassports** from any ICAO country and ICAO national ID cards. The app shows a little ID card with the **photo as pixel art** and **checks that the document is genuine**.

Only use it with your own documents or with the holder's permission.

## How to use

- **Spanish ID**: enter the 6-digit CAN printed on the front.
- **Passport** or **Other ID (with MRZ)**: enter the document number, birth date and expiry date exactly as they appear in the MRZ (the lines of letters and < at the bottom).
- **Other ID (with CAN)**: enter the 6-digit CAN printed on the card.
- Hold the document flat against the back of the Flipper and do not move it until it finishes.

## What it does

- **PACE** (CAN or MRZ) and **BAC** secure access, like official readers. ISO14443-4A and 4B.
- Holder data from the MRZ and the **photo** (JPEG2000 or JPEG) as pixel art, with three dithering styles.
- **Authenticity**: SOD hashes, Document Signer signature (RSA and RSA-PSS), **CSCA chain** for Spain and 20 major countries, and **Chip Authentication** against cloned chips.
- Seals: **GENUINE**, **DATA OK**, **SOD OK**, and a full-screen warning when something does not match.
- The issuing country's **national anthem** plays when the document is genuine (volume adjustable).
- **Saved readings**, encrypted with the Flipper's unique key: full document, or album mode (name, photo, birthday and country only).

## Privacy

- Saved readings cannot be opened on another device.
- The unencrypted photo is deleted from the SD card right after the pixel-art conversion.
- Cards that require an authorised terminal (e.g. the German ID card) are not supported by design.
