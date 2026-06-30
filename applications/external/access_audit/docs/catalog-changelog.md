## 1.12.0
Password-protected MIFARE Ultralight and NTAG cards (such as NTAG213-based hotel keys) no longer hang the scanner — they are now classified from the chip type and UID and scored HIGH (UID-cloneable). Reports note when user memory is password-protected. Outputs now show the underlying air interface next to the card name, e.g. "HID Seos (ISO 14443-4A)", "HID iCLASS (ISO 15693)", "NTAG213 (ISO 14443-3A)".

## 1.11.2
The report now shows the on-screen verdict word (SECURE, HIGH RISK, etc.) in brackets next to the OWASP likelihood band, so the saved report matches what the device displays.

## 1.11.1
Clarified the iCLASS scan mode: the prompt now reads "Tap iCLASS SE/Legacy..." with a "Seos? use NFC mode" hint. iCLASS Legacy/SE are on ISO 15693; Seos is ISO 14443-4A (NFC mode).

## 1.11.0
HID Seos detection: Seos cards (ISO14443-4A) are now identified via an AID SELECT and scored SECURE, instead of showing as a generic ISO 14443-4A card. Passive identify only — reading the PACS still needs a HID SAM.

## 1.10.2
Scan-screen layout fix: the version line and prompts are evenly spaced (no longer squashed under the title divider).

## 1.10.1
Scoring corrections: MIFARE Plus SL2 now scores MODERATE (it is the weak transitional mode — AES auth but Classic frames), and FeliCa Standard now scores SECURE (it has proprietary mutual authentication). FeliCa Lite remains HIGH RISK.

## 1.10.0
Reports now present the score as a **likelihood-of-compromise** rating aligned with the [OWASP Risk Rating Methodology](https://owasp.org/www-community/OWASP_Risk_Rating_Methodology). Each card shows a Likelihood band (HIGH/MODERATE/LOW/MINIMAL) and an Ease-of-exploit factor. The tool rates likelihood only; impact (what the credential protects) is assessed in engagement context.

## 1.9.0
Expanded the MIFARE Classic default-key check to 8 well-known public keys, and the default-key finding is now shown on the result screen, not just in the saved report.

## 1.8.x
MIFARE Classic scan reliability fix; the app version is shown on the scan screen.

## Earlier
FeliCa sub-type detection, HID iCLASS Legacy support, 125 kHz RFID support, on-device report viewer, named sessions, lint/CI hardening, and the initial NFC classification with 0-100 risk scoring.
