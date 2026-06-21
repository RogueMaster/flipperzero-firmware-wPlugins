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
